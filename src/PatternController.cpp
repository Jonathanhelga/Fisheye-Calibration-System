#include "PatternController.h"

#include <QCoreApplication>
#include <QImage>
#include <QMutex>
#include <QQuickImageProvider>
#include <QTimer>

#include <atomic>
#include <mutex>
#include <thread>

#ifdef FISHEYE_ROS_ENABLED
#include <chrono>
#include <string>

#include <rclcpp/rclcpp.hpp>

#include "moil_interfaces/srv/render_pattern.hpp"
#endif

namespace {

constexpr char kRenderService[] = "/compute/render_pattern";
constexpr char kRenderType[] = "moil_interfaces/srv/RenderPattern";

constexpr int kServiceWaitMs = 5000;
constexpr int kWaitSliceMs = 200;
constexpr int kSpinSliceMs = 100;
constexpr int kRenderTimeoutMs = 15000;

QMutex previewMutex;
QImage previewImage;
QByteArray previewBytes;

class PreviewProvider : public QQuickImageProvider {
public:
    PreviewProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}

    QImage requestImage(const QString &, QSize *size, const QSize &) override {
        QMutexLocker locker(&previewMutex);
        if (size) *size = previewImage.size();
        return previewImage;
    }
};

#ifdef FISHEYE_ROS_ENABLED
QString describeMissingService(rclcpp::Node &node, int domainId) {
    const QString name = QString::fromLatin1(kRenderService);
    const auto services = node.get_service_names_and_types();
    const auto found = services.find(kRenderService);

    if (found != services.end()) {
        for (const std::string &type : found->second) {
            if (type != kRenderType) {
                return QObject::tr("%1 is served as %2, this client speaks %3")
                    .arg(name, QString::fromStdString(type), QString::fromLatin1(kRenderType));
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

} // namespace

struct PatternController::Impl {
    std::atomic<quint64> generation{0};
    std::thread worker;
    std::mutex clientMutex;
#ifdef FISHEYE_ROS_ENABLED
    rclcpp::Client<moil_interfaces::srv::RenderPattern>::SharedPtr renderClient;
#endif
};

PatternController::PatternController(QObject *parent) : QObject(parent), d_(new Impl) {
    connect(qApp, &QCoreApplication::aboutToQuit, this, [this] { stopWorker(); });
}

PatternController::~PatternController() { stopWorker(); }

QQmlImageProviderBase *PatternController::createImageProvider() { return new PreviewProvider; }

QString PatternController::previewUrl() const {
    return revision_ > 0 ? QStringLiteral("image://moilpattern/%1").arg(revision_) : QString();
}

void PatternController::stopWorker() {
    ++d_->generation;
    if (d_->worker.joinable()) d_->worker.join();
}

void PatternController::setStatus(ProbeStatus::Status status) {
    if (status_ == status) return;
    status_ = status;
    emit statusChanged();
}

void PatternController::setBusy(bool busy) {
    if (busy_ == busy) return;
    busy_ = busy;
    emit busyChanged();
}

void PatternController::setLastError(const QString &message) {
    if (lastError_ == message) return;
    lastError_ = message;
    emit lastErrorChanged();
}

void PatternController::applyLink(int status, const QString &message, quint64 generation) {
    if (generation != d_->generation.load()) return;

    setStatus(static_cast<ProbeStatus::Status>(status));
    setLastError(message);
    if (status_ != ProbeStatus::Ok) setBusy(false);
}

void PatternController::applyPreview(bool ok, int width, int height, const QString &message,
                                     quint64 token, quint64 generation) {
    if (generation != d_->generation.load() || token != renderToken_) return;

    setBusy(false);
    if (ok) {
        ++revision_;
        previewLabel_ = tr("%1x%2").arg(width).arg(height);
        setLastError(QString());
        emit previewChanged();
    } else {
        setLastError(message.isEmpty() ? tr("the render failed") : message);
    }
}

void PatternController::connectTo(int domainId) {
    stopWorker();

    setStatus(ProbeStatus::Checking);
    setBusy(false);
    setLastError(QString());
    ++renderToken_;

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
            auto node = std::make_shared<rclcpp::Node>("fisheye_cali_jojo_pattern", nodeOptions);

            rclcpp::ExecutorOptions executorOptions;
            executorOptions.context = context;
            rclcpp::executors::SingleThreadedExecutor executor(executorOptions);
            executor.add_node(node);

            auto client = node->create_client<moil_interfaces::srv::RenderPattern>(kRenderService);

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
                    d_->renderClient = client;
                }
                post(ProbeStatus::Ok, QString());

                while (alive()) executor.spin_once(std::chrono::milliseconds(kSpinSliceMs));

                std::lock_guard<std::mutex> lock(d_->clientMutex);
                d_->renderClient.reset();
            }
        } catch (const std::exception &error) {
            post(ProbeStatus::Failed, QString::fromUtf8(error.what()));
        }

        context->shutdown("pattern link closed");
    });
#endif
}

void PatternController::renderPreview(const QString &specJson, int width, int height) {
    if (status_ != ProbeStatus::Ok) return;

#ifndef FISHEYE_ROS_ENABLED
    Q_UNUSED(specJson)
    Q_UNUSED(width)
    Q_UNUSED(height)
    setLastError(tr("this build has no ROS 2 support"));
#else
    rclcpp::Client<moil_interfaces::srv::RenderPattern>::SharedPtr client;
    {
        std::lock_guard<std::mutex> lock(d_->clientMutex);
        client = d_->renderClient;
    }
    if (!client) return;

    setBusy(true);
    setLastError(QString());

    const quint64 generation = d_->generation.load();
    const quint64 token = ++renderToken_;

    auto request = std::make_shared<moil_interfaces::srv::RenderPattern::Request>();
    request->pattern_json = specJson.toStdString();
    request->width = width;
    request->height = height;
    request->max_side = 0;

    client->async_send_request(
        request,
        [this, generation,
         token](rclcpp::Client<moil_interfaces::srv::RenderPattern>::SharedFuture future) {
            const auto response = future.get();

            QByteArray bytes;
            QImage image;
            if (response->success && !response->image.data.empty()) {
                bytes = QByteArray(reinterpret_cast<const char *>(response->image.data.data()),
                                   qsizetype(response->image.data.size()));
                image.loadFromData(bytes);
            }

            if (generation != d_->generation.load()) return;

            const bool ok = !image.isNull();
            QString message = QString::fromStdString(response->message);
            if (!response->success && message.isEmpty())
                message = tr("the rig refused the render");
            if (response->success && !ok)
                message = tr("the reply was not a decodable image (format \"%1\")")
                              .arg(QString::fromStdString(response->image.format));

            if (ok) {
                QMutexLocker locker(&previewMutex);
                previewImage = image;
                previewBytes = bytes;
            }

            QMetaObject::invokeMethod(this, "applyPreview", Qt::QueuedConnection, Q_ARG(bool, ok),
                                      Q_ARG(int, ok ? image.width() : 0),
                                      Q_ARG(int, ok ? image.height() : 0), Q_ARG(QString, message),
                                      Q_ARG(quint64, token), Q_ARG(quint64, generation));
        });

    QTimer::singleShot(kRenderTimeoutMs, this, [this, generation, token] {
        if (generation != d_->generation.load() || token != renderToken_ || !busy_) return;
        ++renderToken_;
        setBusy(false);
        setLastError(tr("no reply from %1 within %2 s")
                         .arg(QString::fromLatin1(kRenderService))
                         .arg(kRenderTimeoutMs / 1000));
    });
#endif
}
