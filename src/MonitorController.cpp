#include "MonitorController.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QVariantMap>

#include <atomic>
#include <mutex>
#include <thread>

#ifdef FISHEYE_ROS_ENABLED
#include <chrono>
#include <string>

#include <rclcpp/rclcpp.hpp>

#include "moil_interfaces/srv/close_pattern.hpp"
#include "moil_interfaces/srv/describe_screens.hpp"
#include "moil_interfaces/srv/get_brightness.hpp"
#include "moil_interfaces/srv/monitor_command.hpp"
#include "moil_interfaces/srv/prepare_patterns.hpp"
#include "moil_interfaces/srv/set_brightness.hpp"
#include "moil_interfaces/srv/set_display_direction.hpp"
#include "moil_interfaces/srv/show_pattern.hpp"
#include "moil_interfaces/srv/show_prepared.hpp"
#endif

namespace {

constexpr char kSetBrightnessService[] = "/monitor/set_brightness";
constexpr char kGetBrightnessService[] = "/monitor/get_brightness";
constexpr char kShowPatternService[] = "/monitor/show_pattern";
constexpr char kClosePatternService[] = "/monitor/close_pattern";
constexpr char kDescribeService[] = "/monitor/describe_screens";
constexpr char kSetDirectionService[] = "/monitor/set_display_direction";
constexpr char kCommandService[] = "/monitor/command";
constexpr char kPrepareService[] = "/monitor/prepare_patterns";
constexpr char kShowPreparedService[] = "/monitor/show_prepared";

constexpr char kProbeType[] = "moil_interfaces/srv/SetBrightness";

constexpr int kServiceWaitMs = 5000;
constexpr int kWaitSliceMs = 200;
constexpr int kSpinSliceMs = 100;

// Kinds for applySimple, so one slot serves the three fire-and-forget calls
// rather than three near-identical ones.
enum SimpleKind { KindBrightnessSet = 0, KindShow = 1, KindClose = 2, KindDirections = 3,
                  KindNumbers = 4 };

MonitorController *g_instance = nullptr;

// CompressedImage wants "png" or "jpeg". Anything else the operator picked is
// transcoded to PNG rather than refused: the file dialog's filter is a
// suggestion and a .bmp on the glass is still a valid calibration target.
bool loadForWire(const QString &path, QByteArray *bytes, QString *format, QString *error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = QObject::tr("cannot read %1: %2").arg(QFileInfo(path).fileName(),
                                                       file.errorString());
        return false;
    }

    const QByteArray raw = file.readAll();
    if (raw.isEmpty()) {
        *error = QObject::tr("%1 is empty").arg(QFileInfo(path).fileName());
        return false;
    }

    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == QLatin1String("png")) {
        *bytes = raw;
        *format = QStringLiteral("png");
        return true;
    }
    if (suffix == QLatin1String("jpg") || suffix == QLatin1String("jpeg")) {
        *bytes = raw;
        *format = QStringLiteral("jpeg");
        return true;
    }

    QImage image;
    if (!image.loadFromData(raw)) {
        *error = QObject::tr("%1 is not an image this build can read")
                     .arg(QFileInfo(path).fileName());
        return false;
    }

    QByteArray encoded;
    QBuffer buffer(&encoded);
    buffer.open(QIODevice::WriteOnly);
    if (!image.save(&buffer, "PNG")) {
        *error = QObject::tr("could not re-encode %1 as PNG").arg(QFileInfo(path).fileName());
        return false;
    }
    *bytes = encoded;
    *format = QStringLiteral("png");
    return true;
}

#ifdef FISHEYE_ROS_ENABLED
QString describeMissingService(rclcpp::Node &node, int domainId) {
    const QString name = QString::fromLatin1(kSetBrightnessService);
    const auto services = node.get_service_names_and_types();
    const auto found = services.find(kSetBrightnessService);

    if (found != services.end()) {
        for (const std::string &type : found->second) {
            if (type != kProbeType) {
                return QObject::tr("%1 is served as %2, this client speaks %3")
                    .arg(name, QString::fromStdString(type), QString::fromLatin1(kProbeType));
            }
        }
        return QObject::tr("%1 was found on domain %2 but did not answer within %3 ms")
            .arg(name, QString::number(domainId), QString::number(kServiceWaitMs));
    }

    return QObject::tr("nothing is serving %1 on domain %2 "
                       "(wrong domain, wrong subnet, or the monitor node is not running)")
        .arg(name, QString::number(domainId));
}
#endif

}  // namespace

struct MonitorController::Impl {
    std::atomic<quint64> generation{0};
    std::thread worker;
    std::mutex clientMutex;
#ifdef FISHEYE_ROS_ENABLED
    rclcpp::Client<moil_interfaces::srv::SetBrightness>::SharedPtr setBrightness;
    rclcpp::Client<moil_interfaces::srv::GetBrightness>::SharedPtr getBrightness;
    rclcpp::Client<moil_interfaces::srv::ShowPattern>::SharedPtr showPattern;
    rclcpp::Client<moil_interfaces::srv::ClosePattern>::SharedPtr closePattern;
    rclcpp::Client<moil_interfaces::srv::DescribeScreens>::SharedPtr describe;
    rclcpp::Client<moil_interfaces::srv::SetDisplayDirection>::SharedPtr setDirection;
    rclcpp::Client<moil_interfaces::srv::MonitorCommand>::SharedPtr command;
    rclcpp::Client<moil_interfaces::srv::PreparePatterns>::SharedPtr prepare;
    rclcpp::Client<moil_interfaces::srv::ShowPrepared>::SharedPtr showPrepared;
#endif
};

MonitorController::MonitorController(QObject *parent) : QObject(parent), d_(new Impl) {
    g_instance = this;
    connect(qApp, &QCoreApplication::aboutToQuit, this, [this] { stopWorker(); });
}

MonitorController::~MonitorController() {
    stopWorker();
    if (g_instance == this) g_instance = nullptr;
}

MonitorController *MonitorController::instance() { return g_instance; }

void MonitorController::stopWorker() {
    ++d_->generation;
    if (d_->worker.joinable()) d_->worker.join();
}

void MonitorController::setStatus(ProbeStatus::Status status) {
    if (status_ == status) return;
    status_ = status;
    emit statusChanged();
}

void MonitorController::setLastError(const QString &message) {
    if (!message.isEmpty()) emit errorRaised(message);
    if (lastError_ == message) return;
    lastError_ = message;
    emit lastErrorChanged();
}

void MonitorController::beginCall() {
    const bool was = busy();
    ++busy_;
    if (was != busy()) emit busyChanged();
}

void MonitorController::endCall() {
    const bool was = busy();
    if (busy_ > 0) --busy_;
    if (was != busy()) emit busyChanged();
}

bool MonitorController::ready() {
    if (status_ == ProbeStatus::Ok) return true;
    setLastError(tr("not connected to the monitor node, press Update in the Server panel"));
    return false;
}

void MonitorController::applyLink(int status, const QString &message, quint64 generation) {
    if (generation != d_->generation.load()) return;

    setStatus(static_cast<ProbeStatus::Status>(status));
    setLastError(message);

    if (status_ != ProbeStatus::Ok) {
        // Outstanding calls will never answer on a link that just went away, and
        // a busy flag that never clears leaves every Update button disabled with
        // no way back.
        if (busy_ > 0) {
            busy_ = 0;
            emit busyChanged();
        }
        if (preparedReady_) {
            preparedReady_ = false;
            emit preparedChanged();
        }
    } else {
        describeScreens();
    }
}

void MonitorController::applySimple(int kind, const QString &direction, bool ok,
                                    const QString &message, quint64 generation) {
    if (generation != d_->generation.load()) return;
    endCall();

    if (!ok) {
        setLastError(message.isEmpty() ? tr("the monitor node refused the request") : message);
        return;
    }
    setLastError(QString());

    switch (kind) {
        case KindShow:
            emit imageShown(direction);
            emit notice(tr("Image shown on %1").arg(direction.toUpper()));
            break;
        case KindClose:
            emit patternClosed(direction);
            break;
        case KindBrightnessSet:
            break;
        case KindDirections:
            emit directionsApplied();
            emit notice(tr("Display mapping applied"));
            describeScreens();
            break;
        case KindNumbers:
            emit notice(tr("Each screen is showing its display number"));
            break;
        default:
            break;
    }
}

void MonitorController::applyBrightness(const QString &direction, bool ok, double brightness,
                                        const QString &message, quint64 generation) {
    if (generation != d_->generation.load()) return;
    endCall();

    if (!ok) {
        setLastError(message.isEmpty() ? tr("could not read the brightness of %1").arg(direction)
                                       : message);
        return;
    }
    setLastError(QString());
    emit brightnessRead(direction, brightness);
}

void MonitorController::applyScreens(const QVariantList &screens, bool complete,
                                     const QString &message, quint64 generation) {
    if (generation != d_->generation.load()) return;
    endCall();

    screens_ = screens;
    mappingComplete_ = complete;

    QStringList parts;
    for (const QVariant &v : screens) {
        const QVariantMap m = v.toMap();
        const QString dir = m.value(QStringLiteral("direction")).toString();
        parts << QStringLiteral("%1 %2x%3 %4")
                     .arg(m.value(QStringLiteral("name")).toString(),
                          QString::number(m.value(QStringLiteral("width")).toInt()),
                          QString::number(m.value(QStringLiteral("height")).toInt()),
                          dir == QLatin1String("-") ? tr("unmapped") : dir.toUpper());
    }
    screensSummary_ = parts.isEmpty() ? message : parts.join(QStringLiteral("   "));

    emit screensChanged();

    // An incomplete mapping is the single most common reason a pattern lands
    // nowhere, and it is silent -- show_pattern still answers success for the
    // panels it did reach. Say it once, here, rather than let every later call
    // look mysteriously half-broken.
    if (!complete && !screens.isEmpty())
        setLastError(tr("the five calibration directions are not all mapped -- "
                        "use Setup Monitor Direction before showing a pattern"));
}

void MonitorController::applyPrepared(const QString &polarity, bool ok, const QStringList &shown,
                                      const QString &message, quint64 generation) {
    if (generation != d_->generation.load()) return;
    endCall();

    if (!ok) {
        setLastError(message.isEmpty() ? tr("nothing prepared to show -- press Update to Monitor "
                                            "on the pattern first")
                                       : message);
        emit preparedFailed(polarity, lastError_);
        return;
    }

    setLastError(QString());
    emit preparedShown(polarity, shown);

    // Short of five panels means one is unmapped or refused. The shot would still
    // be taken, against a rig that is not showing what was asked for, so this is
    // reported rather than assumed away.
    if (shown.size() < 5)
        emit notice(tr("%1 pattern is on %2 of 5 panels (%3)")
                        .arg(polarity)
                        .arg(shown.size())
                        .arg(shown.join(QStringLiteral(", ")).toUpper()));
}

void MonitorController::applyPrepareDone(bool ok, const QString &directory, const QString &message,
                                         quint64 generation) {
    if (generation != d_->generation.load()) return;
    endCall();

    if (!ok) {
        setLastError(message.isEmpty() ? tr("the rig could not prepare the patterns") : message);
        return;
    }

    setLastError(QString());
    if (!preparedReady_) {
        preparedReady_ = true;
        emit preparedChanged();
    }
    emit notice(directory.isEmpty() ? tr("Patterns prepared on the rig")
                                    : tr("Patterns prepared in %1").arg(directory));
}

void MonitorController::reconnect() { connectTo(domainId_); }

void MonitorController::connectTo(int domainId) {
    stopWorker();

    domainId_ = domainId;
    setStatus(ProbeStatus::Checking);
    setLastError(QString());
    if (busy_ > 0) {
        busy_ = 0;
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
            auto node = std::make_shared<rclcpp::Node>("fisheye_cali_jojo_monitor", nodeOptions);

            rclcpp::ExecutorOptions executorOptions;
            executorOptions.context = context;
            rclcpp::executors::SingleThreadedExecutor executor(executorOptions);
            executor.add_node(node);

            auto setB = node->create_client<moil_interfaces::srv::SetBrightness>(
                kSetBrightnessService);
            auto getB = node->create_client<moil_interfaces::srv::GetBrightness>(
                kGetBrightnessService);
            auto showP = node->create_client<moil_interfaces::srv::ShowPattern>(kShowPatternService);
            auto closeP =
                node->create_client<moil_interfaces::srv::ClosePattern>(kClosePatternService);
            auto desc = node->create_client<moil_interfaces::srv::DescribeScreens>(kDescribeService);
            auto setD = node->create_client<moil_interfaces::srv::SetDisplayDirection>(
                kSetDirectionService);
            auto cmd = node->create_client<moil_interfaces::srv::MonitorCommand>(kCommandService);
            auto prep = node->create_client<moil_interfaces::srv::PreparePatterns>(kPrepareService);
            auto showPrep =
                node->create_client<moil_interfaces::srv::ShowPrepared>(kShowPreparedService);

            const auto deadline =
                std::chrono::steady_clock::now() + std::chrono::milliseconds(kServiceWaitMs);
            bool ready = false;
            while (alive() && std::chrono::steady_clock::now() < deadline) {
                if (setB->wait_for_service(std::chrono::milliseconds(kWaitSliceMs))) {
                    ready = true;
                    break;
                }
            }

            if (!ready) {
                post(ProbeStatus::Failed, describeMissingService(*node, domainId));
            } else {
                {
                    std::lock_guard<std::mutex> lock(d_->clientMutex);
                    d_->setBrightness = setB;
                    d_->getBrightness = getB;
                    d_->showPattern = showP;
                    d_->closePattern = closeP;
                    d_->describe = desc;
                    d_->setDirection = setD;
                    d_->command = cmd;
                    d_->prepare = prep;
                    d_->showPrepared = showPrep;
                }
                post(ProbeStatus::Ok, QString());

                while (alive()) executor.spin_once(std::chrono::milliseconds(kSpinSliceMs));

                std::lock_guard<std::mutex> lock(d_->clientMutex);
                d_->setBrightness.reset();
                d_->getBrightness.reset();
                d_->showPattern.reset();
                d_->closePattern.reset();
                d_->describe.reset();
                d_->setDirection.reset();
                d_->command.reset();
                d_->prepare.reset();
                d_->showPrepared.reset();
            }
        } catch (const std::exception &error) {
            post(ProbeStatus::Failed, QString::fromUtf8(error.what()));
        }

        context->shutdown("monitor link closed");
    });
#endif
}

// The bodies below share one shape: bail unless connected, take a client copy
// under the mutex, send, and route the reply back through an apply* on the GUI
// thread carrying the generation it was issued under. Every one of them is
// called from QML on the GUI thread.

void MonitorController::setBrightness(const QString &direction, double brightness) {
    if (!ready()) return;

#ifndef FISHEYE_ROS_ENABLED
    Q_UNUSED(direction)
    Q_UNUSED(brightness)
#else
    rclcpp::Client<moil_interfaces::srv::SetBrightness>::SharedPtr client;
    {
        std::lock_guard<std::mutex> lock(d_->clientMutex);
        client = d_->setBrightness;
    }
    if (!client) return;

    beginCall();
    const quint64 generation = d_->generation.load();

    auto request = std::make_shared<moil_interfaces::srv::SetBrightness::Request>();
    request->direction = direction.toStdString();
    request->brightness = brightness;

    client->async_send_request(
        request, [this, generation, direction](
                     rclcpp::Client<moil_interfaces::srv::SetBrightness>::SharedFuture future) {
            const auto response = future.get();
            QMetaObject::invokeMethod(this, "applySimple", Qt::QueuedConnection,
                                      Q_ARG(int, KindBrightnessSet), Q_ARG(QString, direction),
                                      Q_ARG(bool, response->success),
                                      Q_ARG(QString, QString::fromStdString(response->message)),
                                      Q_ARG(quint64, generation));
        });
#endif
}

void MonitorController::readBrightness(const QString &direction) {
    if (!ready()) return;

#ifndef FISHEYE_ROS_ENABLED
    Q_UNUSED(direction)
#else
    rclcpp::Client<moil_interfaces::srv::GetBrightness>::SharedPtr client;
    {
        std::lock_guard<std::mutex> lock(d_->clientMutex);
        client = d_->getBrightness;
    }
    if (!client) return;

    beginCall();
    const quint64 generation = d_->generation.load();

    auto request = std::make_shared<moil_interfaces::srv::GetBrightness::Request>();
    request->direction = direction.toStdString();

    client->async_send_request(
        request, [this, generation, direction](
                     rclcpp::Client<moil_interfaces::srv::GetBrightness>::SharedFuture future) {
            const auto response = future.get();
            QMetaObject::invokeMethod(this, "applyBrightness", Qt::QueuedConnection,
                                      Q_ARG(QString, direction), Q_ARG(bool, response->success),
                                      Q_ARG(double, response->brightness),
                                      Q_ARG(QString, QString::fromStdString(response->message)),
                                      Q_ARG(quint64, generation));
        });
#endif
}

void MonitorController::showImage(const QString &direction, const QUrl &fileUrl) {
    showImagePath(direction, fileUrl.isLocalFile() ? fileUrl.toLocalFile() : fileUrl.toString());
}

void MonitorController::showImagePath(const QString &direction, const QString &path) {
    if (path.trimmed().isEmpty()) {
        setLastError(tr("no image chosen for %1").arg(direction.toUpper()));
        return;
    }
    if (!ready()) return;

    QByteArray bytes;
    QString format;
    QString error;
    if (!loadForWire(path, &bytes, &format, &error)) {
        setLastError(error);
        return;
    }

#ifndef FISHEYE_ROS_ENABLED
    Q_UNUSED(direction)
#else
    rclcpp::Client<moil_interfaces::srv::ShowPattern>::SharedPtr client;
    {
        std::lock_guard<std::mutex> lock(d_->clientMutex);
        client = d_->showPattern;
    }
    if (!client) return;

    beginCall();
    const quint64 generation = d_->generation.load();

    auto request = std::make_shared<moil_interfaces::srv::ShowPattern::Request>();
    request->direction = direction.toStdString();
    request->image.format = format.toStdString();
    request->image.data.assign(bytes.constBegin(), bytes.constEnd());

    client->async_send_request(
        request, [this, generation, direction](
                     rclcpp::Client<moil_interfaces::srv::ShowPattern>::SharedFuture future) {
            const auto response = future.get();
            QMetaObject::invokeMethod(this, "applySimple", Qt::QueuedConnection,
                                      Q_ARG(int, KindShow), Q_ARG(QString, direction),
                                      Q_ARG(bool, response->success),
                                      Q_ARG(QString, QString::fromStdString(response->message)),
                                      Q_ARG(quint64, generation));
        });
#endif
}

void MonitorController::closePattern(const QString &direction) {
    if (!ready()) return;

#ifndef FISHEYE_ROS_ENABLED
    Q_UNUSED(direction)
#else
    rclcpp::Client<moil_interfaces::srv::ClosePattern>::SharedPtr client;
    {
        std::lock_guard<std::mutex> lock(d_->clientMutex);
        client = d_->closePattern;
    }
    if (!client) return;

    beginCall();
    const quint64 generation = d_->generation.load();

    auto request = std::make_shared<moil_interfaces::srv::ClosePattern::Request>();
    request->direction = direction.toStdString();

    client->async_send_request(
        request, [this, generation, direction](
                     rclcpp::Client<moil_interfaces::srv::ClosePattern>::SharedFuture future) {
            const auto response = future.get();
            QMetaObject::invokeMethod(this, "applySimple", Qt::QueuedConnection,
                                      Q_ARG(int, KindClose), Q_ARG(QString, direction),
                                      Q_ARG(bool, response->success),
                                      Q_ARG(QString, QString::fromStdString(response->message)),
                                      Q_ARG(quint64, generation));
        });
#endif
}

void MonitorController::describeScreens() {
    if (status_ != ProbeStatus::Ok) return;

#ifdef FISHEYE_ROS_ENABLED
    rclcpp::Client<moil_interfaces::srv::DescribeScreens>::SharedPtr client;
    {
        std::lock_guard<std::mutex> lock(d_->clientMutex);
        client = d_->describe;
    }
    if (!client) return;

    beginCall();
    const quint64 generation = d_->generation.load();

    auto request = std::make_shared<moil_interfaces::srv::DescribeScreens::Request>();

    client->async_send_request(
        request, [this, generation](
                     rclcpp::Client<moil_interfaces::srv::DescribeScreens>::SharedFuture future) {
            const auto response = future.get();

            QVariantList screens;
            const size_t count = response->names.size();
            for (size_t i = 0; i < count; ++i) {
                QVariantMap entry;
                entry[QStringLiteral("name")] = QString::fromStdString(response->names[i]);
                entry[QStringLiteral("width")] =
                    i < response->widths.size() ? response->widths[i] : 0;
                entry[QStringLiteral("height")] =
                    i < response->heights.size() ? response->heights[i] : 0;
                entry[QStringLiteral("direction")] =
                    i < response->directions.size()
                        ? QString::fromStdString(response->directions[i])
                        : QStringLiteral("-");
                screens.append(entry);
            }

            QMetaObject::invokeMethod(this, "applyScreens", Qt::QueuedConnection,
                                      Q_ARG(QVariantList, screens),
                                      Q_ARG(bool, response->mapping_complete),
                                      Q_ARG(QString, QString::fromStdString(response->message)),
                                      Q_ARG(quint64, generation));
        });
#endif
}

void MonitorController::setDisplayDirection(const QString &top, const QString &north,
                                            const QString &west, const QString &south,
                                            const QString &east) {
    if (!ready()) return;

#ifndef FISHEYE_ROS_ENABLED
    Q_UNUSED(top)
    Q_UNUSED(north)
    Q_UNUSED(west)
    Q_UNUSED(south)
    Q_UNUSED(east)
#else
    rclcpp::Client<moil_interfaces::srv::SetDisplayDirection>::SharedPtr client;
    {
        std::lock_guard<std::mutex> lock(d_->clientMutex);
        client = d_->setDirection;
    }
    if (!client) return;

    beginCall();
    const quint64 generation = d_->generation.load();

    auto request = std::make_shared<moil_interfaces::srv::SetDisplayDirection::Request>();
    request->display_top = top.toStdString();
    request->display_n = north.toStdString();
    request->display_w = west.toStdString();
    request->display_s = south.toStdString();
    request->display_e = east.toStdString();

    client->async_send_request(
        request,
        [this, generation](
            rclcpp::Client<moil_interfaces::srv::SetDisplayDirection>::SharedFuture future) {
            const auto response = future.get();
            QMetaObject::invokeMethod(this, "applySimple", Qt::QueuedConnection,
                                      Q_ARG(int, KindDirections), Q_ARG(QString, QString()),
                                      Q_ARG(bool, response->success),
                                      Q_ARG(QString, QString::fromStdString(response->message)),
                                      Q_ARG(quint64, generation));
        });
#endif
}

void MonitorController::showDisplayNumbers() {
    if (!ready()) return;

#ifdef FISHEYE_ROS_ENABLED
    rclcpp::Client<moil_interfaces::srv::MonitorCommand>::SharedPtr client;
    {
        std::lock_guard<std::mutex> lock(d_->clientMutex);
        client = d_->command;
    }
    if (!client) return;

    beginCall();
    const quint64 generation = d_->generation.load();

    auto request = std::make_shared<moil_interfaces::srv::MonitorCommand::Request>();
    request->command = "show_display_number";

    client->async_send_request(
        request, [this, generation](
                     rclcpp::Client<moil_interfaces::srv::MonitorCommand>::SharedFuture future) {
            const auto response = future.get();
            QMetaObject::invokeMethod(this, "applySimple", Qt::QueuedConnection,
                                      Q_ARG(int, KindNumbers), Q_ARG(QString, QString()),
                                      Q_ARG(bool, response->success),
                                      Q_ARG(QString, QString::fromStdString(response->message)),
                                      Q_ARG(quint64, generation));
        });
#endif
}

void MonitorController::preparePatterns(const QString &specConcentric,
                                        const QString &specStripeline,
                                        const QVariantList &positiveRgb,
                                        const QVariantList &negativeRgb) {
    if (!ready()) return;

#ifndef FISHEYE_ROS_ENABLED
    Q_UNUSED(specConcentric)
    Q_UNUSED(specStripeline)
    Q_UNUSED(positiveRgb)
    Q_UNUSED(negativeRgb)
#else
    rclcpp::Client<moil_interfaces::srv::PreparePatterns>::SharedPtr client;
    {
        std::lock_guard<std::mutex> lock(d_->clientMutex);
        client = d_->prepare;
    }
    if (!client) return;

    beginCall();
    const quint64 generation = d_->generation.load();

    auto request = std::make_shared<moil_interfaces::srv::PreparePatterns::Request>();
    request->spec_concentric = specConcentric.toStdString();
    request->spec_stripeline = specStripeline.toStdString();
    for (const QVariant &v : positiveRgb) request->positive_rgb.push_back(v.toInt());
    for (const QVariant &v : negativeRgb) request->negative_rgb.push_back(v.toInt());

    client->async_send_request(
        request, [this, generation](
                     rclcpp::Client<moil_interfaces::srv::PreparePatterns>::SharedFuture future) {
            const auto response = future.get();
            QMetaObject::invokeMethod(this, "applyPrepareDone", Qt::QueuedConnection,
                                      Q_ARG(bool, response->success),
                                      Q_ARG(QString, QString::fromStdString(response->directory)),
                                      Q_ARG(QString, QString::fromStdString(response->message)),
                                      Q_ARG(quint64, generation));
        });
#endif
}

void MonitorController::showPrepared(const QString &polarity) {
    if (status_ != ProbeStatus::Ok) {
        const QString why = tr("not connected to the monitor node");
        setLastError(why);
        emit preparedFailed(polarity, why);
        return;
    }

#ifndef FISHEYE_ROS_ENABLED
    Q_UNUSED(polarity)
#else
    rclcpp::Client<moil_interfaces::srv::ShowPrepared>::SharedPtr client;
    {
        std::lock_guard<std::mutex> lock(d_->clientMutex);
        client = d_->showPrepared;
    }
    if (!client) {
        emit preparedFailed(polarity, tr("the monitor link is not up"));
        return;
    }

    beginCall();
    const quint64 generation = d_->generation.load();

    auto request = std::make_shared<moil_interfaces::srv::ShowPrepared::Request>();
    request->polarity = polarity.toStdString();

    client->async_send_request(
        request, [this, generation, polarity](
                     rclcpp::Client<moil_interfaces::srv::ShowPrepared>::SharedFuture future) {
            const auto response = future.get();

            QStringList shown;
            for (const std::string &s : response->shown) shown << QString::fromStdString(s);

            QMetaObject::invokeMethod(this, "applyPrepared", Qt::QueuedConnection,
                                      Q_ARG(QString, polarity), Q_ARG(bool, response->success),
                                      Q_ARG(QStringList, shown),
                                      Q_ARG(QString, QString::fromStdString(response->message)),
                                      Q_ARG(quint64, generation));
        });
#endif
}
