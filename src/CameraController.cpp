#include "CameraController.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QQuickImageProvider>
#include <QTime>
#include <QTimer>

#include <atomic>
#include <mutex>
#include <thread>

#include "ImageStore.h"
#include "MonitorController.h"

#ifdef FISHEYE_ROS_ENABLED
#include <chrono>
#include <string>

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/compressed_image.hpp>

#include "moil_interfaces/srv/capture.hpp"
#endif

namespace {

constexpr char kCaptureService[] = "/camera/capture";
constexpr char kCaptureType[] = "moil_interfaces/srv/Capture";
constexpr char kLiveTopic[] = "/camera/image_raw/compressed";

constexpr int kServiceWaitMs = 5000;
constexpr int kWaitSliceMs = 200;
constexpr int kSpinSliceMs = 100;
constexpr int kCaptureTimeoutMs = 20000;

// The preview is there to aim the rig, not to be watched frame by frame. Above
// ~15/s the GUI thread spends its time decoding 3040x3040 JPEGs it will never
// finish painting, and the whole UI goes sticky while the stream is on.
constexpr qint64 kMinFrameGapMs = 66;

const QString kSlotSingle = QStringLiteral("single");
const QString kSlotPositive = QStringLiteral("positive");
const QString kSlotNegative = QStringLiteral("negative");
const QString kSlotLive = QStringLiteral("live");

QString normalisedSlot(const QString &slot) {
    const QString s = slot.trimmed().toLower();
    if (s == kSlotPositive || s == QLatin1String("pos")) return kSlotPositive;
    if (s == kSlotNegative || s == QLatin1String("neg")) return kSlotNegative;
    return kSlotSingle;
}

QString stamp() { return QTime::currentTime().toString(QStringLiteral("HH:mm:ss")); }

class FrameProvider : public QQuickImageProvider {
public:
    FrameProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}

    // The URL is image://moilcamera/<slot>/<revision>. Only the slot selects the
    // picture; the revision exists purely so the URL changes and Qt stops
    // serving the cached one.
    QImage requestImage(const QString &id, QSize *size, const QSize &) override {
        const QImage image = ImageStore::image(id.section(QLatin1Char('/'), 0, 0));
        if (size) *size = image.size();
        return image;
    }
};

#ifdef FISHEYE_ROS_ENABLED
QString describeMissingService(rclcpp::Node &node, int domainId) {
    const QString name = QString::fromLatin1(kCaptureService);
    const auto services = node.get_service_names_and_types();
    const auto found = services.find(kCaptureService);

    if (found != services.end()) {
        for (const std::string &type : found->second) {
            if (type == kCaptureType) continue;
            return QObject::tr("%1 is served as %2, this client speaks %3")
                .arg(name, QString::fromStdString(type), QString::fromLatin1(kCaptureType));
        }
    }

    return QObject::tr("nothing is serving %1 on domain %2 "
                       "(wrong domain, wrong subnet, or the camera node is not running)")
        .arg(name)
        .arg(domainId);
}
#endif

}  // namespace

struct CameraController::Impl {
    std::atomic<quint64> generation{0};
    std::thread worker;
    std::mutex clientMutex;

    // Read by the worker loop, written from the GUI thread. The subscription is
    // created and destroyed INSIDE the worker, because a topic subscription on a
    // node owned by another thread's executor cannot be added from here safely.
    std::atomic<bool> wantStream{false};
#ifdef FISHEYE_ROS_ENABLED
    rclcpp::Client<moil_interfaces::srv::Capture>::SharedPtr client;
#endif
};

CameraController::CameraController(QObject *parent) : QObject(parent), d_(new Impl) {
    connect(qApp, &QCoreApplication::aboutToQuit, this, [this] { stopWorker(); });
}

CameraController::~CameraController() { stopWorker(); }

QQmlImageProviderBase *CameraController::createImageProvider() { return new FrameProvider; }

void CameraController::stopWorker() {
    d_->wantStream.store(false);
    ++d_->generation;
    if (d_->worker.joinable()) d_->worker.join();
}

QString CameraController::frameUrl() const {
    return singleRevision_ > 0
               ? QStringLiteral("image://moilcamera/%1/%2").arg(kSlotSingle).arg(singleRevision_)
               : QString();
}

QString CameraController::positiveUrl() const {
    return positiveRevision_ > 0 ? QStringLiteral("image://moilcamera/%1/%2")
                                       .arg(kSlotPositive)
                                       .arg(positiveRevision_)
                                 : QString();
}

QString CameraController::negativeUrl() const {
    return negativeRevision_ > 0 ? QStringLiteral("image://moilcamera/%1/%2")
                                       .arg(kSlotNegative)
                                       .arg(negativeRevision_)
                                 : QString();
}

QString CameraController::liveUrl() const {
    return liveRevision_ > 0
               ? QStringLiteral("image://moilcamera/%1/%2").arg(kSlotLive).arg(liveRevision_)
               : QString();
}

bool CameraController::hasPositive() const { return positiveRevision_ > 0; }
bool CameraController::hasNegative() const { return negativeRevision_ > 0; }

void CameraController::setError(const QString &message) {
    lastError_ = message;
    emit changed();
    if (!message.isEmpty()) emit errorRaised(message);
}

void CameraController::setFov(int degrees) {
    if (fov_ == degrees) return;
    fov_ = degrees;
    emit fovChanged();
}

void CameraController::clearSlot(const QString &slot) {
    const QString key = normalisedSlot(slot);
    ImageStore::clear(key);
    if (key == kSlotPositive) {
        positiveRevision_ = 0;
        positiveTime_.clear();
    } else if (key == kSlotNegative) {
        negativeRevision_ = 0;
        negativeTime_.clear();
    } else {
        singleRevision_ = 0;
        frameLabel_.clear();
    }
    emit changed();
}

void CameraController::applyLink(int status, const QString &message, quint64 generation) {
    if (generation != d_->generation.load()) return;

    status_ = static_cast<ProbeStatus::Status>(status);
    lastError_ = message;
    if (status_ != ProbeStatus::Ok) {
        busy_ = false;
        pendingSlot_.clear();
        if (sequenceRunning()) {
            sequence_.clear();
            awaitingGlass_ = false;
            emit pairFailed(tr("the camera link went away mid-shot"));
        }
        if (streaming_) {
            // The subscription is gone with the context; saying otherwise leaves
            // a LIVE badge over a frozen frame.
            d_->wantStream.store(false);
            streaming_ = false;
            fps_ = 0;
            lastFrameMs_ = 0;
            emit liveChanged();
        }
    }
    emit changed();
    if (!message.isEmpty()) emit errorRaised(message);
}

void CameraController::applyCapture(const QString &slot, bool ok, int width, int height,
                                    const QString &message, quint64 token, quint64 generation) {
    if (generation != d_->generation.load() || token != captureToken_) return;

    busy_ = false;
    pendingSlot_.clear();

    if (!ok) {
        setError(message.isEmpty() ? tr("the capture failed") : message);
        if (sequenceRunning()) abortSequence(lastError_);
        return;
    }

    lastError_.clear();
    const QString label = tr("capture %1  %2x%3").arg(stamp()).arg(width).arg(height);

    if (slot == kSlotPositive) {
        positiveRevision_ = ImageStore::revision(kSlotPositive);
        positiveTime_ = stamp();
    } else if (slot == kSlotNegative) {
        negativeRevision_ = ImageStore::revision(kSlotNegative);
        negativeTime_ = stamp();
    } else {
        singleRevision_ = ImageStore::revision(kSlotSingle);
        frameLabel_ = message.isEmpty() ? label : label + QStringLiteral("  ") + message;
    }

    emit changed();
    emit captured(slot, width, height);

    if (sequenceRunning()) advanceSequence();
}

void CameraController::applyLiveFrame(int width, int height, quint64 generation) {
    if (generation != d_->generation.load() || !streaming_) return;

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (lastFrameMs_ > 0) {
        const qint64 gap = now - lastFrameMs_;
        // Smoothed so the readout does not flicker between 11 and 19 fps on a
        // stream that is perfectly steady.
        if (gap > 0) fps_ = fps_ > 0 ? (fps_ * 0.7 + (1000.0 / gap) * 0.3) : 1000.0 / gap;
    }
    lastFrameMs_ = now;

    liveRevision_ = ImageStore::revision(kSlotLive);
    Q_UNUSED(width)
    Q_UNUSED(height)
    emit liveChanged();
}

void CameraController::connectTo(int domainId) {
    stopWorker();

    status_ = ProbeStatus::Checking;
    busy_ = false;
    pendingSlot_.clear();
    lastError_.clear();
    ++captureToken_;
    sequence_.clear();
    awaitingGlass_ = false;
    streaming_ = false;
    fps_ = 0;
    lastFrameMs_ = 0;
    emit changed();
    emit liveChanged();

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
            auto node = std::make_shared<rclcpp::Node>("fisheye_cali_jojo_camera", nodeOptions);

            rclcpp::ExecutorOptions executorOptions;
            executorOptions.context = context;
            rclcpp::executors::SingleThreadedExecutor executor(executorOptions);
            executor.add_node(node);

            auto client = node->create_client<moil_interfaces::srv::Capture>(kCaptureService);

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

                // The preview subscription is created only while the operator has
                // the stream on. It is a 3040x3040 BEST_EFFORT topic: subscribing
                // up front and discarding frames would put that on the wire for
                // the whole session to serve a panel nobody is looking at.
                rclcpp::Subscription<sensor_msgs::msg::CompressedImage>::SharedPtr live;
                auto lastPosted = std::chrono::steady_clock::now() - std::chrono::hours(1);

                while (alive()) {
                    const bool want = d_->wantStream.load();

                    if (want && !live) {
                        auto qos = rclcpp::SensorDataQoS();
                        live = node->create_subscription<sensor_msgs::msg::CompressedImage>(
                            kLiveTopic, qos,
                            [this, generation, &lastPosted](
                                const sensor_msgs::msg::CompressedImage::SharedPtr msg) {
                                if (generation != d_->generation.load()) return;

                                const auto now = std::chrono::steady_clock::now();
                                if (std::chrono::duration_cast<std::chrono::milliseconds>(
                                        now - lastPosted)
                                        .count() < kMinFrameGapMs)
                                    return;
                                lastPosted = now;

                                const QByteArray bytes(
                                    reinterpret_cast<const char *>(msg->data.data()),
                                    qsizetype(msg->data.size()));

                                QImage image;
                                if (!image.loadFromData(bytes)) return;

                                ImageStore::put(kSlotLive, bytes,
                                                QString::fromStdString(msg->format), image,
                                                image.width(), image.height());

                                QMetaObject::invokeMethod(
                                    this, "applyLiveFrame", Qt::QueuedConnection,
                                    Q_ARG(int, image.width()), Q_ARG(int, image.height()),
                                    Q_ARG(quint64, generation));
                            });
                    } else if (!want && live) {
                        live.reset();
                    }

                    executor.spin_once(std::chrono::milliseconds(kSpinSliceMs));
                }

                live.reset();

                std::lock_guard<std::mutex> lock(d_->clientMutex);
                d_->client.reset();
            }
        } catch (const std::exception &error) {
            post(ProbeStatus::Failed, QString::fromUtf8(error.what()));
        }

        context->shutdown("camera link closed");
    });
#endif
}

void CameraController::capture(const QString &slot) {
    const QString key = normalisedSlot(slot);

    if (busy_) return;
    if (status_ != ProbeStatus::Ok) {
        setError(tr("not connected to the camera node, press Update in the Server panel"));
        if (sequenceRunning()) abortSequence(lastError_);
        return;
    }

#ifndef FISHEYE_ROS_ENABLED
    setError(tr("this build has no ROS 2 support"));
#else
    rclcpp::Client<moil_interfaces::srv::Capture>::SharedPtr client;
    {
        std::lock_guard<std::mutex> lock(d_->clientMutex);
        client = d_->client;
    }
    if (!client) return;

    busy_ = true;
    pendingSlot_ = key;
    lastError_.clear();
    emit changed();

    const quint64 generation = d_->generation.load();
    const quint64 token = ++captureToken_;

    auto request = std::make_shared<moil_interfaces::srv::Capture::Request>();
    request->timeout = 0.0;
    // Deliberately empty. Writing into a server session slot is a separate
    // decision from taking a shot, and this app has no session layer to own one;
    // the frame is kept client-side in ImageStore and sent back with each op.
    request->session_slot = "";

    client->async_send_request(
        request, [this, generation, token,
                  key](rclcpp::Client<moil_interfaces::srv::Capture>::SharedFuture future) {
            const auto response = future.get();

            QImage image;
            QByteArray bytes;
            if (response->success && !response->image.data.empty()) {
                bytes = QByteArray(reinterpret_cast<const char *>(response->image.data.data()),
                                   qsizetype(response->image.data.size()));
                image.loadFromData(bytes);
            }

            if (generation != d_->generation.load()) return;

            const bool ok = !image.isNull();
            QString message = QString::fromStdString(response->message);
            if (!response->success && message.isEmpty())
                message = tr("the rig refused the capture");
            if (response->success && !ok)
                message = tr("the reply was not a decodable image (format \"%1\")")
                              .arg(QString::fromStdString(response->image.format));
            if (ok && response->width > 0 && response->height > 0 &&
                (response->width != image.width() || response->height != image.height()))
                message = tr("(the rig reported %1x%2)")
                              .arg(response->width)
                              .arg(response->height);

            if (ok)
                ImageStore::put(key, bytes, QString::fromStdString(response->image.format), image,
                                response->width, response->height);

            QMetaObject::invokeMethod(this, "applyCapture", Qt::QueuedConnection,
                                      Q_ARG(QString, key), Q_ARG(bool, ok),
                                      Q_ARG(int, ok ? image.width() : 0),
                                      Q_ARG(int, ok ? image.height() : 0), Q_ARG(QString, message),
                                      Q_ARG(quint64, token), Q_ARG(quint64, generation));
        });

    QTimer::singleShot(kCaptureTimeoutMs, this, [this, generation, token] {
        if (generation != d_->generation.load() || token != captureToken_ || !busy_) return;
        ++captureToken_;
        busy_ = false;
        pendingSlot_.clear();
        setError(tr("no reply from %1 within %2 s")
                     .arg(QString::fromLatin1(kCaptureService))
                     .arg(kCaptureTimeoutMs / 1000));
        if (sequenceRunning()) abortSequence(lastError_);
    });
#endif
}

// ---- the pair shot ---------------------------------------------------------
//
// Positive, swap the glass, negative -- driven from here rather than from QML
// because every step has to be undone correctly when one of them fails, and a
// half-finished pair that still looks like a pair is a measurement of two
// different scenes.
//
// The polarity swap goes through ShowPrepared, NOT through re-rendering the
// spec. A shot taken against a pattern regenerated from a spec the operator has
// since edited is a measurement of the wrong thing and looks exactly like a good
// one -- which is why ShowPrepared fails rather than falling back to rendering,
// and why this reports that failure instead of shooting anyway.

void CameraController::hookMonitor() {
    if (monitorHooked_) return;
    MonitorController *monitor = MonitorController::instance();
    if (!monitor) return;

    // The glass is now showing the polarity we asked for -- grab it.
    connect(monitor, &MonitorController::preparedShown, this,
            [this](const QString &polarity, const QStringList &) {
                if (!awaitingGlass_ || sequence_.isEmpty()) return;
                if (polarity != sequence_.first()) return;
                awaitingGlass_ = false;
                capture(sequence_.first());
            });

    connect(monitor, &MonitorController::preparedFailed, this,
            [this](const QString &, const QString &message) {
                if (sequenceRunning()) abortSequence(message);
            });

    monitorHooked_ = true;
}

void CameraController::beginSequence(const QStringList &polarities) {
    if (busy_ || sequenceRunning() || polarities.isEmpty()) return;

    // Null until QML has touched MonitorController, which the Server panel's
    // Update does. Worth saying plainly: "no monitor" and "never connected" look
    // identical from a button that simply does nothing.
    MonitorController *monitor = MonitorController::instance();
    if (!monitor) {
        emit pairFailed(tr("the monitor link has not been opened -- press Update in the "
                           "Server panel first"));
        return;
    }

    hookMonitor();

    sequence_ = polarities;
    sequenceTotal_ = static_cast<int>(polarities.size());
    awaitingGlass_ = true;
    emit changed();
    emit notice(tr("Showing the %1 pattern").arg(sequence_.first()));
    monitor->showPrepared(sequence_.first());
}

void CameraController::advanceSequence() {
    if (sequence_.isEmpty()) return;

    const QString done = sequence_.takeFirst();

    if (sequence_.isEmpty()) {
        emit changed();
        if (sequenceTotal_ > 1) {
            emit pairComplete();
            emit notice(tr("Pair complete -- positive and negative are both in hand"));
        } else {
            emit notice(tr("%1 shot taken against the %1 pattern").arg(done));
        }
        return;
    }

    MonitorController *monitor = MonitorController::instance();
    if (!monitor) {
        abortSequence(tr("the monitor link went away mid-sequence"));
        return;
    }

    awaitingGlass_ = true;
    emit changed();
    emit notice(tr("Showing the %1 pattern").arg(sequence_.first()));
    monitor->showPrepared(sequence_.first());
}

void CameraController::abortSequence(const QString &reason) {
    if (sequence_.isEmpty()) return;
    sequence_.clear();
    awaitingGlass_ = false;
    emit changed();
    emit pairFailed(reason);
}

void CameraController::captureShot(const QString &polarity) {
    beginSequence({normalisedSlot(polarity)});
}

void CameraController::capturePair() { beginSequence({kSlotPositive, kSlotNegative}); }

// ---- disk, and the live preview --------------------------------------------

bool CameraController::openImage(const QUrl &fileUrl, const QString &slot) {
    const QString path = fileUrl.isLocalFile() ? fileUrl.toLocalFile() : fileUrl.toString();
    const QString key = normalisedSlot(slot);

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(tr("cannot read %1: %2").arg(QFileInfo(path).fileName(), file.errorString()));
        return false;
    }

    const QByteArray bytes = file.readAll();
    QImage image;
    if (!image.loadFromData(bytes)) {
        setError(tr("%1 is not an image this build can read").arg(QFileInfo(path).fileName()));
        return false;
    }

    // The bytes are kept exactly as they came off disk. The detect ops measure
    // pixel values, and a re-encode here would hand them a different picture
    // from the one the operator is looking at.
    const QString suffix = QFileInfo(path).suffix().toLower();
    const QString format = (suffix == QLatin1String("jpg") || suffix == QLatin1String("jpeg"))
                               ? QStringLiteral("jpeg")
                               : QStringLiteral("png");

    const int revision =
        ImageStore::put(key, bytes, format, image, image.width(), image.height());

    if (key == kSlotPositive) {
        positiveRevision_ = revision;
        positiveTime_ = tr("file");
    } else if (key == kSlotNegative) {
        negativeRevision_ = revision;
        negativeTime_ = tr("file");
    } else {
        singleRevision_ = revision;
        frameLabel_ = QFileInfo(path).fileName();
    }

    lastError_.clear();
    emit changed();
    emit captured(key, image.width(), image.height());
    return true;
}

void CameraController::startStream() {
    if (streaming_) return;
    if (status_ != ProbeStatus::Ok) {
        setError(tr("not connected to the camera node, press Update in the Server panel"));
        return;
    }

    streaming_ = true;
    fps_ = 0;
    lastFrameMs_ = 0;
    d_->wantStream.store(true);
    emit liveChanged();
}

void CameraController::stopStream() {
    if (!streaming_) return;
    d_->wantStream.store(false);
    streaming_ = false;
    fps_ = 0;
    lastFrameMs_ = 0;
    emit liveChanged();
}

void CameraController::snapshot() {
    const ImageStore::Frame frame = ImageStore::frame(kSlotLive);
    if (frame.image.isNull()) {
        setError(tr("no live frame to keep yet"));
        return;
    }

    singleRevision_ =
        ImageStore::put(kSlotSingle, frame.bytes, frame.format, frame.image, frame.width,
                        frame.height);
    // Labelled as a preview frame on purpose. It came off the BEST_EFFORT topic,
    // so it may predate the pattern currently on the glass -- fine for aiming,
    // not a measurement.
    frameLabel_ = tr("preview frame %1  %2x%3").arg(stamp()).arg(frame.width).arg(frame.height);
    lastError_.clear();
    emit changed();
    emit captured(kSlotSingle, frame.width, frame.height);
}
