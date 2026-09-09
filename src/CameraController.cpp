#include "CameraController.h"

#include "ImageStore.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QImage>
#include <QMutex>
#include <QQuickImageProvider>
#include <QSize>
#include <QTime>
#include <QTimer>

#include <atomic>
#include <mutex>
#include <thread>

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

constexpr int kServiceWaitMs = 5000;
constexpr int kWaitSliceMs = 200;
constexpr int kSpinSliceMs = 100;
constexpr int kCaptureTimeoutMs = 20000;
constexpr int kMinFoldRadius = 8;

// The live preview. Restored 2026-09-08 -- v2.1_2026_New-UI-CPP-ROS never had a
// stream, so the merge left LiveCameraPanel's Start, Stop and Snapshot emitting
// into nothing.
constexpr char kLiveTopic[] = "/camera/image_raw/compressed";
constexpr char kSlotLive[] = "live";
constexpr char kSlotSingle[] = "single";

// The topic is 3040x3040 BEST_EFFORT. Decoding every frame that arrives costs
// more than the panel can show, so frames are dropped at the subscription rather
// than queued -- roughly 12/s reaches the GUI thread.
constexpr int kMinFrameGapMs = 80;

QMutex frameMutex;
QHash<QString, QImage> frameImages;

class FrameProvider : public QQuickImageProvider {
public:
    FrameProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}

    QImage requestImage(const QString &id, QSize *size, const QSize &) override {
        QMutexLocker locker(&frameMutex);
        const QImage image = frameImages.value(id.section(QLatin1Char('/'), 0, 0));
        if (size) *size = image.size();
        return image;
    }
};

QString slotKey(const QString &slot) {
    const QString trimmed = slot.trimmed().toLower();
    return trimmed.isEmpty() ? QStringLiteral("single") : trimmed;
}

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

} // namespace

struct CameraController::Impl {
    std::atomic<quint64> generation{0};
    std::thread worker;
    std::mutex clientMutex;
    // Read by the worker each spin to decide whether the preview subscription
    // should exist. Atomic rather than mutex-guarded because it is a single flag
    // written from the GUI thread and read from the executor thread.
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
    ++d_->generation;
    if (d_->worker.joinable()) d_->worker.join();
}

void CameraController::applyLink(int status, const QString &message, quint64 generation) {
    if (generation != d_->generation.load()) return;

    status_ = static_cast<ProbeStatus::Status>(status);
    lastError_ = message;
    if (status_ != ProbeStatus::Ok) busy_ = false;
    emit changed();
}

void CameraController::applyCapture(const QString &slot, bool ok, int width, int height,
                                    int frameWidth, int frameHeight, const QString &message,
                                    quint64 token, quint64 generation) {
    if (generation != d_->generation.load() || token != captureToken_) return;

    busy_ = false;
    if (ok) {
        const int revision = revisions_.value(slot) + 1;
        revisions_.insert(slot, revision);
        frameUrls_.insert(slot,
                          QStringLiteral("image://moilcamera/%1/%2").arg(slot).arg(revision));
        frameSizes_.insert(slot, frameWidth > 0 && frameHeight > 0
                                     ? QSize(frameWidth, frameHeight)
                                     : QSize(width, height));

        QString label = tr("capture %1  %2x%3")
                            .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")))
                            .arg(width)
                            .arg(height);
        if (!message.isEmpty()) label += QStringLiteral("  ") + message;
        frameLabels_.insert(slot, label);
        lastError_.clear();
    } else {
        lastError_ = message.isEmpty() ? tr("the capture failed") : message;
    }
    emit changed();
    emit captured(slot, ok, ok ? QString() : lastError_);
}

void CameraController::foldCheck(const QString &slot, int cx, int cy, int radius, int gain) {
    const QString key = slotKey(slot);
    const QString foldKey = key + QStringLiteral("_fold");

    auto clear = [&] {
        foldUrls_.remove(key);
        foldScores_.insert(key, -1.0);
        emit changed();
    };

    QImage source;
    {
        QMutexLocker locker(&frameMutex);
        source = frameImages.value(key);
    }
    if (source.isNull() || radius <= 0 || cx < 0 || cy < 0) {
        clear();
        return;
    }

    const QSize frame = frameSizes_.value(key).toSize();
    const double scale =
        frame.width() > 0 ? double(source.width()) / double(frame.width()) : 1.0;

    const int sx = qRound(cx * scale);
    const int sy = qRound(cy * scale);
    if (sx < 0 || sy < 0 || sx >= source.width() || sy >= source.height()) {
        clear();
        return;
    }

    int r = qRound(radius * scale);
    r = qMin(r, qMin(sx, source.width() - 1 - sx));
    r = qMin(r, qMin(sy, source.height() - 1 - sy));
    if (r < kMinFoldRadius) {
        clear();
        return;
    }

    const int n = 2 * r + 1;
    const QImage patch =
        source.copy(sx - r, sy - r, n, n).convertToFormat(QImage::Format_Grayscale8);
    QImage fold(n, n, QImage::Format_Grayscale8);

    const qint64 rr = qint64(r) * r;
    const int amplify = qMax(1, gain);
    qint64 sum = 0;
    qint64 count = 0;

    for (int y = 0; y < n; ++y) {
        const uchar *row = patch.constScanLine(y);
        const uchar *mirror = patch.constScanLine(n - 1 - y);
        uchar *out = fold.scanLine(y);
        const qint64 dy = qint64(y) - r;

        for (int x = 0; x < n; ++x) {
            const qint64 dx = qint64(x) - r;
            if (dx * dx + dy * dy > rr) {
                out[x] = 0;
                continue;
            }
            const int difference = qAbs(int(row[x]) - int(mirror[n - 1 - x]));
            out[x] = uchar(qMin(255, difference * amplify));
            sum += difference;
            ++count;
        }
    }

    {
        QMutexLocker locker(&frameMutex);
        frameImages.insert(foldKey, fold);
    }

    const int revision = revisions_.value(foldKey) + 1;
    revisions_.insert(foldKey, revision);
    foldUrls_.insert(key,
                     QStringLiteral("image://moilcamera/%1/%2").arg(foldKey).arg(revision));
    foldScores_.insert(key, count > 0 ? double(sum) / double(count) : -1.0);
    emit changed();
}

// Restored after the 2026-09-08 merge; see the note on the property in the
// header. Guarded so a repeated write does not emit -- changed() is this class's
// one catch-all notify signal, and every frame binding in the UI depends on it.
QString CameraController::liveUrl() const {
    // The revision is what makes the URL change. Without it Qt serves the cached
    // picture and the preview freezes on the first frame while the rig streams on.
    return liveRevision_ > 0 ? QStringLiteral("image://moilcamera/%1/%2")
                                   .arg(QLatin1String(kSlotLive))
                                   .arg(liveRevision_)
                             : QString();
}

// GUI thread. The frame itself is already in frameImages and ImageStore; this
// only bumps the revision so the binding re-reads it, and keeps the frame rate.
void CameraController::applyLiveFrame(int width, int height, quint64 generation) {
    Q_UNUSED(width)
    Q_UNUSED(height)
    if (generation != d_->generation.load() || !streaming_) return;

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (lastFrameMs_ > 0) {
        const qint64 gap = now - lastFrameMs_;
        // Smoothed so the readout does not flicker between 11 and 19 fps on a
        // stream that is perfectly steady.
        if (gap > 0) fps_ = fps_ > 0 ? (fps_ * 0.7 + (1000.0 / gap) * 0.3) : 1000.0 / gap;
    }
    lastFrameMs_ = now;

    ++liveRevision_;
    emit changed();
}

void CameraController::startStream() {
    if (streaming_) return;
    if (status_ != ProbeStatus::Ok) {
        lastError_ = tr("not connected to the camera node, press Update in the Server panel");
        emit changed();
        return;
    }
    streaming_ = true;
    // Cleared on every start, not carried over: a stale rate from the previous
    // session would be shown as current for the first second of this one.
    fps_ = 0;
    lastFrameMs_ = 0;
    d_->wantStream.store(true);
    emit changed();
}

void CameraController::stopStream() {
    if (!streaming_) return;
    d_->wantStream.store(false);
    streaming_ = false;
    fps_ = 0;
    lastFrameMs_ = 0;
    emit changed();
}

void CameraController::snapshot() {
    const ImageStore::Frame frame = ImageStore::frame(QString::fromLatin1(kSlotLive));
    if (frame.image.isNull()) {
        lastError_ = tr("no live frame to keep yet");
        emit changed();
        return;
    }

    const QString slot = QString::fromLatin1(kSlotSingle);

    // Straight across, bytes and all -- re-encoding here would defeat the reason
    // ImageStore holds the originals.
    ImageStore::put(slot, frame.bytes, frame.format, frame.image, frame.width, frame.height);
    {
        QMutexLocker locker(&frameMutex);
        frameImages.insert(slot, frame.image);
    }

    const int revision = revisions_.value(slot) + 1;
    revisions_.insert(slot, revision);
    frameUrls_.insert(slot, QStringLiteral("image://moilcamera/%1/%2").arg(slot).arg(revision));
    frameSizes_.insert(slot, QSize(frame.width, frame.height));
    frameLabels_.insert(slot, tr("preview frame %1  %2x%3")
                                  .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")))
                                  .arg(frame.width)
                                  .arg(frame.height));
    lastError_.clear();
    emit changed();
    emit captured(slot, true, QString());
}

void CameraController::setFov(int degrees) {
    if (fov_ == degrees) return;
    fov_ = degrees;
    emit changed();
}

void CameraController::connectTo(int domainId) {
    stopWorker();

    status_ = ProbeStatus::Checking;
    busy_ = false;
    lastError_.clear();
    ++captureToken_;
    emit changed();

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

                // The preview subscription exists only while the operator has the
                // stream on. Subscribing up front and discarding frames would put
                // a 3040x3040 topic on the wire for the whole session to serve a
                // panel nobody is looking at.
                rclcpp::Subscription<sensor_msgs::msg::CompressedImage>::SharedPtr live;
                auto lastPosted = std::chrono::steady_clock::now() - std::chrono::hours(1);

                while (alive()) {
                    const bool want = d_->wantStream.load();

                    if (want && !live) {
                        live = node->create_subscription<sensor_msgs::msg::CompressedImage>(
                            kLiveTopic, rclcpp::SensorDataQoS(),
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

                                // Both stores, for the same reason a capture goes
                                // to both: frameImages is what the QML image
                                // provider paints from, ImageStore is what keeps
                                // the original bytes so Snapshot can hand a frame
                                // on without re-encoding it.
                                {
                                    QMutexLocker locker(&frameMutex);
                                    frameImages.insert(QString::fromLatin1(kSlotLive), image);
                                }
                                ImageStore::put(QString::fromLatin1(kSlotLive), bytes,
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
    if (busy_ || status_ != ProbeStatus::Ok) return;

    const QString key = slotKey(slot);

#ifndef FISHEYE_ROS_ENABLED
    Q_UNUSED(key)
    lastError_ = tr("this build has no ROS 2 support");
    emit changed();
    emit captured(key, false, lastError_);
#else
    rclcpp::Client<moil_interfaces::srv::Capture>::SharedPtr client;
    {
        std::lock_guard<std::mutex> lock(d_->clientMutex);
        client = d_->client;
    }
    if (!client) return;

    busy_ = true;
    lastError_.clear();
    emit changed();

    const quint64 generation = d_->generation.load();
    const quint64 token = ++captureToken_;

    auto request = std::make_shared<moil_interfaces::srv::Capture::Request>();
    request->timeout = 0.0;
    request->session_slot = "";

    client->async_send_request(
        request,
        [this, key, generation,
         token](rclcpp::Client<moil_interfaces::srv::Capture>::SharedFuture future) {
            const auto response = future.get();

            // The bytes and the format are kept, not just the decoded QImage.
            //
            // This controller only needs the QImage -- it paints it. But
            // /compute/detect takes sensor_msgs/CompressedImage, so ComputeController
            // has to send a capture back out over the wire, and re-encoding the
            // QImage to PNG would hand the detect ops a picture that is not the one
            // the camera produced. Every centre fit in this system is a measurement
            // of exact pixel values, so that difference is not cosmetic.
            //
            // Learned the hard way on 2026-09-08: this file arrived from
            // v2.1_2026_New-UI-CPP-ROS, which has no ComputeController and therefore
            // no reason to keep the bytes. Nothing wrote ImageStore afterwards, so
            // every detect op sent an empty image -- Find Pos, Find Neg and
            // Direction Diff all failed, and the build and qmllint were both clean.
            QImage image;
            QByteArray bytes;
            QString format;
            if (response->success && !response->image.data.empty()) {
                bytes = QByteArray(reinterpret_cast<const char *>(response->image.data.data()),
                                   qsizetype(response->image.data.size()));
                format = QString::fromStdString(response->image.format);
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

            if (ok) {
                {
                    QMutexLocker locker(&frameMutex);
                    frameImages.insert(key, image);
                }
                // Scoped above so this is not called holding frameMutex --
                // ImageStore takes its own lock, and nesting two would be a
                // deadlock waiting for a second writer.
                ImageStore::put(key, bytes, format, image, response->width, response->height);
            }

            QMetaObject::invokeMethod(this, "applyCapture", Qt::QueuedConnection,
                                      Q_ARG(QString, key), Q_ARG(bool, ok),
                                      Q_ARG(int, ok ? image.width() : 0),
                                      Q_ARG(int, ok ? image.height() : 0),
                                      Q_ARG(int, response->width), Q_ARG(int, response->height),
                                      Q_ARG(QString, message), Q_ARG(quint64, token),
                                      Q_ARG(quint64, generation));
        });

    QTimer::singleShot(kCaptureTimeoutMs, this, [this, key, generation, token] {
        if (generation != d_->generation.load() || token != captureToken_ || !busy_) return;
        ++captureToken_;
        busy_ = false;
        lastError_ = tr("no reply from %1 within %2 s")
                         .arg(QString::fromLatin1(kCaptureService))
                         .arg(kCaptureTimeoutMs / 1000);
        emit changed();
        emit captured(key, false, lastError_);
    });
#endif
}
