#include "CameraController.h"

#include <QCoreApplication>
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

                while (alive()) executor.spin_once(std::chrono::milliseconds(kSpinSliceMs));

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

            QImage image;
            if (response->success && !response->image.data.empty()) {
                const QByteArray bytes(reinterpret_cast<const char *>(response->image.data.data()),
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

            if (ok) {
                QMutexLocker locker(&frameMutex);
                frameImages.insert(key, image);
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
