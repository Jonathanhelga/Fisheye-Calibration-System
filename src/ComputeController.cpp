#include "ComputeController.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QVariantList>

#include <atomic>
#include <mutex>
#include <thread>

#include "ImageStore.h"

#ifdef FISHEYE_ROS_ENABLED
#include <chrono>
#include <string>

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/compressed_image.hpp>

#include "moil_interfaces/srv/detect_op.hpp"
#endif

namespace {

constexpr char kDetectService[] = "/compute/detect";
constexpr char kDetectType[] = "moil_interfaces/srv/DetectOp";

constexpr int kServiceWaitMs = 5000;
constexpr int kWaitSliceMs = 200;
constexpr int kSpinSliceMs = 100;
constexpr int kOpTimeoutMs = 90000;

constexpr char kOpAutoCenter[] = "auto_center";
constexpr char kOpRoiExact[] = "roi_exact";
constexpr char kOpHistogram[] = "histogram_8dir";
constexpr char kOpNodes[] = "nodes_8dir";

const QStringList &dirs8() {
    static const QStringList d{QStringLiteral("n"),  QStringLiteral("s"),  QStringLiteral("w"),
                               QStringLiteral("e"),  QStringLiteral("nw"), QStringLiteral("se"),
                               QStringLiteral("sw"), QStringLiteral("ne")};
    return d;
}

QString dump(const QJsonObject &o) {
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

// A JSON object of direction -> number array becomes a QVariantMap of
// direction -> QVariantList, which is what a QML `var` binding can index.
QVariantMap numbersByDirection(const QJsonObject &o) {
    QVariantMap out;
    for (auto it = o.constBegin(); it != o.constEnd(); ++it) {
        QVariantList values;
        const QJsonArray a = it.value().toArray();
        values.reserve(a.size());
        for (const QJsonValue &v : a) values.append(v.toDouble());
        out.insert(it.key(), values);
    }
    return out;
}

#ifdef FISHEYE_ROS_ENABLED
QString describeMissingService(rclcpp::Node &node, int domainId) {
    const QString name = QString::fromLatin1(kDetectService);
    const auto services = node.get_service_names_and_types();
    const auto found = services.find(kDetectService);

    if (found != services.end()) {
        for (const std::string &type : found->second) {
            if (type != kDetectType) {
                return QObject::tr("%1 is served as %2, this client speaks %3")
                    .arg(name, QString::fromStdString(type), QString::fromLatin1(kDetectType));
            }
        }
        return QObject::tr("%1 was found on domain %2 but did not answer within %3 ms")
            .arg(name, QString::number(domainId), QString::number(kServiceWaitMs));
    }

    return QObject::tr("nothing is serving %1 on domain %2 "
                       "(wrong domain, wrong subnet, or the compute node is not running)")
        .arg(name, QString::number(domainId));
}
#endif

}  // namespace

struct ComputeController::Impl {
    std::atomic<quint64> generation{0};
    std::thread worker;
    std::mutex clientMutex;
#ifdef FISHEYE_ROS_ENABLED
    rclcpp::Client<moil_interfaces::srv::DetectOp>::SharedPtr client;
#endif
};

ComputeController::ComputeController(QObject *parent) : QObject(parent), d_(new Impl) {
    connect(qApp, &QCoreApplication::aboutToQuit, this, [this] { stopWorker(); });
}

ComputeController::~ComputeController() { stopWorker(); }

void ComputeController::stopWorker() {
    ++d_->generation;
    if (d_->worker.joinable()) d_->worker.join();
}

void ComputeController::setStatus(ProbeStatus::Status status) {
    if (status_ == status) return;
    status_ = status;
    emit statusChanged();
}

void ComputeController::setLastError(const QString &message) {
    if (!message.isEmpty()) emit errorRaised(message);
    if (lastError_ == message) return;
    lastError_ = message;
    emit lastErrorChanged();
}

void ComputeController::beginCall(const QString &what) {
    ++busy_;
    activity_ = what;
    emit busyChanged();
}

void ComputeController::endCall() {
    if (busy_ > 0) --busy_;
    if (busy_ == 0) activity_.clear();
    emit busyChanged();
}

void ComputeController::clearResults() {
    centers_.clear();
    histogram_.clear();
    nodes_.clear();
    emit centersChanged();
    emit histogramChanged();
    emit nodesChanged();
}

// NOT named `slots`: that is a Qt keyword macro and expands to nothing, so the
// parameter silently loses its name and the range-for has no range.
bool ComputeController::gather(const QStringList &slotNames,
                               QList<QPair<QByteArray, QString>> *out) {
    for (const QString &slot : slotNames) {
        const ImageStore::Frame f = ImageStore::frame(slot);
        if (f.bytes.isEmpty()) {
            setLastError(tr("no %1 shot yet -- take one before running this").arg(slot));
            return false;
        }
        out->append({f.bytes, f.format.isEmpty() ? QStringLiteral("png") : f.format});
    }
    return true;
}

void ComputeController::applyLink(int status, const QString &message, quint64 generation) {
    if (generation != d_->generation.load()) return;

    setStatus(static_cast<ProbeStatus::Status>(status));
    setLastError(message);

    if (status_ != ProbeStatus::Ok) {
        // Requests issued on a link that has gone away will never answer.
        liveTokens_.clear();
        if (busy_ > 0) {
            busy_ = 0;
            activity_.clear();
            emit busyChanged();
        }
    }
}

void ComputeController::applyResult(const QString &op, const QString &slot, bool ok,
                                    const QString &resultJson, const QString &message,
                                    quint64 token, quint64 generation) {
    if (generation != d_->generation.load() || !liveTokens_.remove(token)) return;
    endCall();

    if (!ok) {
        const QString why = message.isEmpty() ? tr("%1 failed").arg(op) : message;
        setLastError(why);
        if (op == QLatin1String(kOpAutoCenter) || op == QLatin1String(kOpRoiExact))
            emit centerRefused(slot, why);
        return;
    }

    const QJsonObject r = QJsonDocument::fromJson(resultJson.toUtf8()).object();
    setLastError(QString());

    if (op == QLatin1String(kOpAutoCenter)) {
        const int x = r.value(QStringLiteral("cx")).toInt(-1);
        const int y = r.value(QStringLiteral("cy")).toInt(-1);
        const bool good = r.value(QStringLiteral("ok")).toBool(false) && x >= 0 && y >= 0;
        const QString method = r.value(QStringLiteral("method")).toString();
        const QString confidence = r.value(QStringLiteral("confidence")).toString();

        QVariantMap entry;
        entry[QStringLiteral("ok")] = good;
        entry[QStringLiteral("x")] = x;
        entry[QStringLiteral("y")] = y;
        entry[QStringLiteral("method")] = method;
        entry[QStringLiteral("confidence")] = confidence;
        entry[QStringLiteral("offsetPx")] = r.value(QStringLiteral("offset_px")).toDouble();
        entry[QStringLiteral("cost")] = r.value(QStringLiteral("cost")).toDouble();
        entry[QStringLiteral("ringsUsed")] = r.value(QStringLiteral("rings_used")).toInt();
        entry[QStringLiteral("expectedRings")] = r.value(QStringLiteral("expected_rings")).toInt();
        centers_[slot] = entry;
        emit centersChanged();

        if (good) {
            emit centerFound(slot, x, y, method, confidence);
            emit notice(tr("%1 centre at %2, %3 by %4 (%5)")
                            .arg(slot)
                            .arg(x)
                            .arg(y)
                            .arg(method)
                            .arg(confidence));
        } else {
            // The cascade ran and declined. Reported as no centre, deliberately:
            // a refused fit is the correct answer on a badly aimed shot.
            const QString reason = r.value(QStringLiteral("reason")).toString();
            const QString why =
                reason.isEmpty()
                    ? tr("no centre found in the %1 shot -- the fit could not be trusted").arg(slot)
                    : tr("no centre in the %1 shot: %2").arg(slot, reason);
            emit centerRefused(slot, why);
            setLastError(why);
        }
        return;
    }

    if (op == QLatin1String(kOpRoiExact)) {
        const int x = r.value(QStringLiteral("x")).toInt(-1);
        const int y = r.value(QStringLiteral("y")).toInt(-1);
        const bool converged = r.value(QStringLiteral("converged")).toBool(false);

        QVariantMap entry;
        entry[QStringLiteral("ok")] = x >= 0 && y >= 0;
        entry[QStringLiteral("x")] = x;
        entry[QStringLiteral("y")] = y;
        entry[QStringLiteral("method")] = QStringLiteral("roi_exact");
        entry[QStringLiteral("confidence")] =
            converged ? QStringLiteral("good") : QStringLiteral("marginal");
        entry[QStringLiteral("iterations")] = r.value(QStringLiteral("iterations")).toInt();
        centers_[slot] = entry;
        emit centersChanged();

        emit centerFound(slot, x, y, QStringLiteral("roi_exact"),
                         converged ? QStringLiteral("good") : QStringLiteral("marginal"));
        if (!converged)
            emit notice(tr("%1 point did not settle in 20 passes -- treat it as approximate")
                            .arg(slot));
        return;
    }

    if (op == QLatin1String(kOpHistogram)) {
        QVariantMap out;
        out[QStringLiteral("pos")] = numbersByDirection(r.value(QStringLiteral("pos")).toObject());
        out[QStringLiteral("neg")] = numbersByDirection(r.value(QStringLiteral("neg")).toObject());
        out[QStringLiteral("nodes")] =
            numbersByDirection(r.value(QStringLiteral("nodes")).toObject());
        histogram_ = out;
        emit histogramChanged();
        emit notice(tr("Curves updated from the current pair"));
        return;
    }

    if (op == QLatin1String(kOpNodes)) {
        nodes_ = numbersByDirection(r.value(QStringLiteral("nodes")).toObject());
        emit nodesChanged();

        int total = 0;
        for (auto it = nodes_.constBegin(); it != nodes_.constEnd(); ++it)
            total += static_cast<int>(it.value().toList().size());
        emit notice(tr("%1 crossings found across 8 directions").arg(total));
        return;
    }
}

void ComputeController::connectTo(int domainId) {
    stopWorker();

    setStatus(ProbeStatus::Checking);
    setLastError(QString());
    liveTokens_.clear();
    if (busy_ > 0) {
        busy_ = 0;
        activity_.clear();
        emit busyChanged();
    }

    const quint64 generation = d_->generation.load();

#ifndef FISHEYE_ROS_ENABLED
    Q_UNUSED(domainId)
    applyLink(ProbeStatus::Failed,
              tr("this build has no ROS 2 support "
                 "(configure with -DFISHEYE_ENABLE_ROS=ON on Linux)"),
              generation);
#else
    d_->worker = std::thread([this, generation, domainId] {
        auto alive = [this, generation] { return generation == d_->generation.load(); };

        auto post = [this, generation, &alive](ProbeStatus::Status status,
                                               const QString &message) {
            if (!alive()) return;
            QMetaObject::invokeMethod(this, "applyLink", Qt::QueuedConnection,
                                      Q_ARG(int, static_cast<int>(status)),
                                      Q_ARG(QString, message), Q_ARG(quint64, generation));
        };

        auto context = std::make_shared<rclcpp::Context>();
        rclcpp::NodeOptions nodeOptions;

        try {
            rclcpp::InitOptions initOptions;
            initOptions.set_domain_id(static_cast<size_t>(domainId));
            initOptions.auto_initialize_logging(false);
            context->init(0, nullptr, initOptions);
            nodeOptions.context(context);
        } catch (const std::exception &error) {
            post(ProbeStatus::Failed, QString::fromUtf8(error.what()));
            return;
        }

        try {
            auto node = std::make_shared<rclcpp::Node>("fisheye_cali_jojo_detect", nodeOptions);

            rclcpp::ExecutorOptions executorOptions;
            executorOptions.context = context;
            rclcpp::executors::SingleThreadedExecutor executor(executorOptions);
            executor.add_node(node);

            auto client = node->create_client<moil_interfaces::srv::DetectOp>(kDetectService);

            const auto deadline =
                std::chrono::steady_clock::now() + std::chrono::milliseconds(kServiceWaitMs);
            bool ready = false;
            while (alive() && std::chrono::steady_clock::now() < deadline) {
                if (client->wait_for_service(std::chrono::milliseconds(kWaitSliceMs))) {
                    ready = true;
                    break;
                }
            }

            if (!ready) {
                post(ProbeStatus::Failed, describeMissingService(*node, domainId));
            } else {
                {
                    std::lock_guard<std::mutex> lock(d_->clientMutex);
                    d_->client = client;
                }
                post(ProbeStatus::Ok, QString());

                while (alive()) executor.spin_once(std::chrono::milliseconds(kSpinSliceMs));

                std::lock_guard<std::mutex> lock(d_->clientMutex);
                d_->client.reset();
            }
        } catch (const std::exception &error) {
            post(ProbeStatus::Failed, QString::fromUtf8(error.what()));
        }

        context->shutdown("detect link closed");
    });
#endif
}

bool ComputeController::sendDetect(const QString &op, const QString &slot,
                                   const QStringList &imageSlots, const QString &paramsJson,
                                   const QString &activity) {
    if (status_ != ProbeStatus::Ok) {
        setLastError(tr("not connected to the compute node, press Update in the Server panel"));
        return false;
    }

    QList<QPair<QByteArray, QString>> images;
    if (!gather(imageSlots, &images)) return false;

#ifndef FISHEYE_ROS_ENABLED
    Q_UNUSED(op)
    Q_UNUSED(slot)
    Q_UNUSED(paramsJson)
    Q_UNUSED(activity)
    return false;
#else
    rclcpp::Client<moil_interfaces::srv::DetectOp>::SharedPtr client;
    {
        std::lock_guard<std::mutex> lock(d_->clientMutex);
        client = d_->client;
    }
    if (!client) return false;

    beginCall(activity);
    const quint64 generation = d_->generation.load();
    const quint64 token = ++nextToken_;
    liveTokens_.insert(token);

    auto request = std::make_shared<moil_interfaces::srv::DetectOp::Request>();
    request->op = op.toStdString();
    request->params_json = paramsJson.toStdString();
    request->images.reserve(images.size());
    for (const auto &pair : images) {
        sensor_msgs::msg::CompressedImage image;
        image.format = pair.second.toStdString();
        image.data.assign(pair.first.constBegin(), pair.first.constEnd());
        request->images.push_back(image);
    }

    client->async_send_request(
        request, [this, generation, token, op,
                  slot](rclcpp::Client<moil_interfaces::srv::DetectOp>::SharedFuture future) {
            const auto response = future.get();
            QMetaObject::invokeMethod(
                this, "applyResult", Qt::QueuedConnection, Q_ARG(QString, op), Q_ARG(QString, slot),
                Q_ARG(bool, response->success),
                Q_ARG(QString, QString::fromStdString(response->result_json)),
                Q_ARG(QString, QString::fromStdString(response->message)), Q_ARG(quint64, token),
                Q_ARG(quint64, generation));
        });

    // Detect ops have no progress and no cancel (session_services.cpp:370), and
    // the auto_center cascade is measured in seconds, so the budget is generous.
    // Without it a request the node never answers leaves the panel disabled with
    // no message -- the exact silent failure this app is full of elsewhere.
    QTimer::singleShot(kOpTimeoutMs, this, [this, generation, token, op] {
        if (generation != d_->generation.load() || !liveTokens_.remove(token)) return;
        endCall();
        setLastError(tr("%1 did not answer within %2 s").arg(op).arg(kOpTimeoutMs / 1000));
    });
    return true;
#endif
}

void ComputeController::autoCenter(const QString &slot, int expectedRings, bool noiseCleaning) {
    QJsonObject params;
    // `slot` also tells auto_center which prepared PNG to count rings from, which
    // is the source it trusts over the modal count. See ComputeDetectOps.cpp.
    params[QStringLiteral("slot")] = slot;
    params[QStringLiteral("noise_cleaning")] = noiseCleaning;
    if (expectedRings > 0) params[QStringLiteral("expected_rings")] = expectedRings;

    sendDetect(QString::fromLatin1(kOpAutoCenter), slot, {slot}, dump(params),
               tr("finding the %1 centre").arg(slot));
}

void ComputeController::refineCenter(const QString &slot, int x, int y, int threshold) {
    QJsonObject params;
    params[QStringLiteral("org_x")] = x;
    params[QStringLiteral("org_y")] = y;
    params[QStringLiteral("threshold")] = threshold;

    sendDetect(QString::fromLatin1(kOpRoiExact), slot, {slot}, dump(params),
               tr("settling the %1 point").arg(slot));
}

void ComputeController::histogram8Dir(int posCx, int posCy, int negCx, int negCy,
                                      bool noiseCleaning) {
    QJsonObject params;
    params[QStringLiteral("pos_cx")] = posCx;
    params[QStringLiteral("pos_cy")] = posCy;
    params[QStringLiteral("neg_cx")] = negCx;
    params[QStringLiteral("neg_cy")] = negCy;
    params[QStringLiteral("noise_cleaning")] = noiseCleaning;

    QJsonArray dirs;
    for (const QString &d : dirs8()) dirs.append(d);
    params[QStringLiteral("dirs")] = dirs;

    // Positive first, then negative -- the order every two-image op documents.
    sendDetect(QString::fromLatin1(kOpHistogram), QString(),
               {QStringLiteral("positive"), QStringLiteral("negative")}, dump(params),
               tr("measuring 8 directions"));
}

void ComputeController::nodes8Dir(int posCx, int posCy, int negCx, int negCy, bool noiseCleaning) {
    QJsonObject params;
    params[QStringLiteral("pos_cx")] = posCx;
    params[QStringLiteral("pos_cy")] = posCy;
    params[QStringLiteral("neg_cx")] = negCx;
    params[QStringLiteral("neg_cy")] = negCy;
    params[QStringLiteral("noise_cleaning")] = noiseCleaning;

    sendDetect(QString::fromLatin1(kOpNodes), QString(),
               {QStringLiteral("positive"), QStringLiteral("negative")}, dump(params),
               tr("extracting nodes"));
}
