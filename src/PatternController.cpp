#include "PatternController.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QMutex>
#include <QQuickImageProvider>
#include <QSaveFile>
#include <QTimer>

#include <atomic>
#include <mutex>
#include <thread>

#ifdef FISHEYE_ROS_ENABLED
#include <chrono>
#include <string>

#include <rclcpp/rclcpp.hpp>

#include "moil_interfaces/srv/render_for_direction.hpp"
#include "moil_interfaces/srv/render_pattern.hpp"
#include "moil_interfaces/srv/show_pattern_spec.hpp"
#endif

namespace {

constexpr char kRenderService[] = "/compute/render_pattern";
constexpr char kRenderType[] = "moil_interfaces/srv/RenderPattern";

constexpr char kShowService[] = "/monitor/show_pattern_spec";
constexpr char kDirectionService[] = "/monitor/render_for_direction";

constexpr int kSlotPreviewMaxSide = 640;

constexpr int kServiceWaitMs = 5000;
constexpr int kWaitSliceMs = 200;
constexpr int kSpinSliceMs = 100;
constexpr int kRenderTimeoutMs = 15000;
constexpr int kShowTimeoutMs = 15000;

QMutex previewMutex;
QHash<QString, QImage> previewImages;
QHash<QString, QByteArray> previewBytes;

class PreviewProvider : public QQuickImageProvider {
public:
    PreviewProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}

    QImage requestImage(const QString &id, QSize *size, const QSize &) override {
        QMutexLocker locker(&previewMutex);
        const QImage image = previewImages.value(id.section(QLatin1Char('/'), 0, 0));
        if (size) *size = image.size();
        return image;
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
    rclcpp::Client<moil_interfaces::srv::ShowPatternSpec>::SharedPtr showClient;
    rclcpp::Client<moil_interfaces::srv::RenderForDirection>::SharedPtr directionClient;
#endif
};

PatternController::PatternController(QObject *parent) : QObject(parent), d_(new Impl) {
    connect(qApp, &QCoreApplication::aboutToQuit, this, [this] { stopWorker(); });
}

PatternController::~PatternController() { stopWorker(); }

QQmlImageProviderBase *PatternController::createImageProvider() { return new PreviewProvider; }

void PatternController::stopWorker() {
    ++d_->generation;
    if (d_->worker.joinable()) d_->worker.join();
}

void PatternController::setStatus(ProbeStatus::Status status) {
    if (status_ == status) return;
    status_ = status;
    emit statusChanged();
}

void PatternController::updateBusy() {
    const bool busy = !pending_.isEmpty();
    if (busy_ == busy) return;
    busy_ = busy;
    emit busyChanged();
}

void PatternController::finishRender(const QString &patternType) {
    pending_.remove(patternType);
    ++renderTokens_[patternType];
    updateBusy();
}

void PatternController::setLastError(const QString &message) {
    if (!message.isEmpty()) emit errorRaised(message);
    if (lastError_ == message) return;
    lastError_ = message;
    emit lastErrorChanged();
}

void PatternController::applyLink(int status, const QString &message, quint64 generation) {
    if (generation != d_->generation.load()) return;

    setStatus(static_cast<ProbeStatus::Status>(status));
    setLastError(message);
    if (status_ != ProbeStatus::Ok) {
        pending_.clear();
        updateBusy();
    }
}

void PatternController::applyPreview(const QString &patternType, bool ok, const QString &message, quint64 token, quint64 generation) {
    if (generation != d_->generation.load() || token != renderTokens_.value(patternType)) return;

    finishRender(patternType);

    if (ok) {
        previewUrls_[patternType] = QStringLiteral("image://moilpattern/%1/%2")
                                        .arg(patternType)
                                        .arg(++revisions_[patternType]);
        setLastError(QString());
        emit previewChanged();
    } else {
        setLastError(message.isEmpty() ? tr("the render failed") : message);
    }
}

void PatternController::applyShow(const QString &direction, bool ok, const QString &message,
                                  int width, int height, quint64 token, quint64 generation) {
    if (generation != d_->generation.load() || token != showToken_) return;

    ++showToken_;

    if (ok) {
        setLastError(QString());
        emit patternShown(direction, width, height);
        refreshDirection(direction);
    } else {
        setLastError(message.isEmpty() ? tr("the rig refused to show the pattern") : message);
    }
}

void PatternController::connectTo(int domainId) {
    stopWorker();

    domainId_ = domainId;
    ++showToken_;
    setStatus(ProbeStatus::Checking);
    setLastError(QString());
    for (const QString &patternType : pending_) ++renderTokens_[patternType];
    pending_.clear();
    updateBusy();

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
            auto show = node->create_client<moil_interfaces::srv::ShowPatternSpec>(kShowService);
            auto perDirection = node->create_client<moil_interfaces::srv::RenderForDirection>(kDirectionService);

            const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(kServiceWaitMs);
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
                    d_->showClient = show;
                    d_->directionClient = perDirection;
                }
                post(ProbeStatus::Ok, QString());

                while (alive()) executor.spin_once(std::chrono::milliseconds(kSpinSliceMs));

                std::lock_guard<std::mutex> lock(d_->clientMutex);
                d_->renderClient.reset();
                d_->showClient.reset();
                d_->directionClient.reset();
            }
        } catch (const std::exception &error) {
            post(ProbeStatus::Failed, QString::fromUtf8(error.what()));
        }

        context->shutdown("pattern link closed");
    });
#endif
}

bool PatternController::savePreview(const QString &patternType, const QUrl &fileUrl) {
    if (!fileUrl.isLocalFile()) {
        setLastError(tr("%1 is not a local file").arg(fileUrl.toString()));
        return false;
    }

    QByteArray bytes;
    {
        QMutexLocker locker(&previewMutex);
        bytes = previewBytes.value(patternType);
    }
    if (bytes.isEmpty()) {
        setLastError(tr("there is no rendered %1 preview to save").arg(patternType));
        return false;
    }

    const QString path = fileUrl.toLocalFile();
    const QDir dir = QFileInfo(path).absoluteDir();
    if (!dir.exists() && !QDir().mkpath(dir.absolutePath())) {
        setLastError(tr("could not create %1").arg(dir.absolutePath()));
        return false;
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        setLastError(file.errorString());
        return false;
    }

    setLastError(QString());
    return true;
}

void PatternController::renderPreview(const QString &patternType, const QString &specJson,
                                      int width, int height) {
    if (status_ != ProbeStatus::Ok) {
        setLastError(tr("not connected to %1, press ROS Update first")
                         .arg(QString::fromLatin1(kRenderService)));
        return;
    }

#ifndef FISHEYE_ROS_ENABLED
    Q_UNUSED(patternType)
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
    if (!client) {
        setLastError(tr("the pattern link is up but the client is gone"));
        return;
    }

    setLastError(QString());

    const quint64 generation = d_->generation.load();
    const quint64 token = ++renderTokens_[patternType];
    pending_.insert(patternType);
    updateBusy();

    auto request = std::make_shared<moil_interfaces::srv::RenderPattern::Request>();
    request->pattern_json = specJson.toStdString();
    request->width = width;
    request->height = height;
    request->max_side = 0;

    client->async_send_request(
        request,
        [this, generation, token,
         patternType](rclcpp::Client<moil_interfaces::srv::RenderPattern>::SharedFuture future) {
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
                previewImages[patternType] = image;
                previewBytes[patternType] = bytes;
            }

            QMetaObject::invokeMethod(this, "applyPreview", Qt::QueuedConnection,
                                      Q_ARG(QString, patternType), Q_ARG(bool, ok),
                                      Q_ARG(QString, message), Q_ARG(quint64, token),
                                      Q_ARG(quint64, generation));
        });

    QTimer::singleShot(kRenderTimeoutMs, this, [this, generation, token, patternType] {
        if (generation != d_->generation.load() || token != renderTokens_.value(patternType)) return;
        finishRender(patternType);
        setLastError(tr("no reply from %1 within %2 s")
                         .arg(QString::fromLatin1(kRenderService))
                         .arg(kRenderTimeoutMs / 1000));
    });
#endif
}

void PatternController::showOnMonitor(const QString &direction, const QString &specJson) {
    if (status_ != ProbeStatus::Ok) {
        setLastError(tr("not connected to the rig, press ROS Update first"));
        return;
    }

#ifndef FISHEYE_ROS_ENABLED
    Q_UNUSED(direction)
    Q_UNUSED(specJson)
    setLastError(tr("this build has no ROS 2 support"));
#else
    rclcpp::Client<moil_interfaces::srv::ShowPatternSpec>::SharedPtr client;
    {
        std::lock_guard<std::mutex> lock(d_->clientMutex);
        client = d_->showClient;
    }
    if (!client) {
        setLastError(tr("the pattern link is up but the client is gone"));
        return;
    }
    if (!client->service_is_ready()) {
        setLastError(tr("nothing is serving %1 on domain %2 "
                        "(wrong domain, wrong subnet, or the monitor node is not running)")
                         .arg(QString::fromLatin1(kShowService), QString::number(domainId_)));
        return;
    }

    setLastError(QString());

    const quint64 generation = d_->generation.load();
    const quint64 token = ++showToken_;

    lastSpecs_[direction] = specJson;

    auto request = std::make_shared<moil_interfaces::srv::ShowPatternSpec::Request>();
    request->direction = direction.toStdString();
    request->spec_json = specJson.toStdString();

    client->async_send_request(
        request,
        [this, generation, token,
         direction](rclcpp::Client<moil_interfaces::srv::ShowPatternSpec>::SharedFuture future) {
            const auto response = future.get();

            if (generation != d_->generation.load()) return;

            const bool ok = response->success;
            QString message = QString::fromStdString(response->message);
            if (!ok && message.isEmpty())
                message = tr("%1 refused the pattern, is a display mapped to %2?")
                              .arg(QString::fromLatin1(kShowService), direction.toUpper());

            const int width = response->widths.empty() ? 0 : response->widths.front();
            const int height = response->heights.empty() ? 0 : response->heights.front();

            QMetaObject::invokeMethod(this, "applyShow", Qt::QueuedConnection,
                                      Q_ARG(QString, direction), Q_ARG(bool, ok),
                                      Q_ARG(QString, message), Q_ARG(int, width),
                                      Q_ARG(int, height), Q_ARG(quint64, token),
                                      Q_ARG(quint64, generation));
        });

    QTimer::singleShot(kShowTimeoutMs, this, [this, generation, token] {
        if (generation != d_->generation.load() || token != showToken_) return;
        ++showToken_;
        setLastError(tr("no reply from %1 within %2 s")
                         .arg(QString::fromLatin1(kShowService))
                         .arg(kShowTimeoutMs / 1000));
    });
#endif
}

void PatternController::refreshDirection(const QString &direction) {
    const QString spec = lastSpecs_.value(direction);
    if (spec.isEmpty() || status_ != ProbeStatus::Ok) return;

#ifndef FISHEYE_ROS_ENABLED
    return;
#else
    rclcpp::Client<moil_interfaces::srv::RenderForDirection>::SharedPtr client;
    {
        std::lock_guard<std::mutex> lock(d_->clientMutex);
        client = d_->directionClient;
    }
    if (!client || !client->service_is_ready()) return;

    const quint64 generation = d_->generation.load();
    const quint64 token = ++renderTokens_[direction];
    pending_.insert(direction);
    updateBusy();

    auto request = std::make_shared<moil_interfaces::srv::RenderForDirection::Request>();
    request->direction = direction.toStdString();
    request->spec_json = spec.toStdString();
    request->max_side = kSlotPreviewMaxSide;

    client->async_send_request(
        request,
        [this, generation, token,
         direction](rclcpp::Client<moil_interfaces::srv::RenderForDirection>::SharedFuture future) {
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
            if (!ok && message.isEmpty())
                message = tr("%1 gave no preview for %2")
                              .arg(QString::fromLatin1(kDirectionService), direction.toUpper());

            if (ok) {
                QMutexLocker locker(&previewMutex);
                previewImages[direction] = image;
                previewBytes[direction] = bytes;
            }

            QMetaObject::invokeMethod(this, "applyPreview", Qt::QueuedConnection,
                                      Q_ARG(QString, direction), Q_ARG(bool, ok),
                                      Q_ARG(QString, message), Q_ARG(quint64, token),
                                      Q_ARG(quint64, generation));
        });

    QTimer::singleShot(kRenderTimeoutMs, this, [this, generation, token, direction] {
        if (generation != d_->generation.load() || token != renderTokens_.value(direction)) return;
        finishRender(direction);
        setLastError(tr("no reply from %1 within %2 s")
                         .arg(QString::fromLatin1(kDirectionService))
                         .arg(kRenderTimeoutMs / 1000));
    });
#endif
}
