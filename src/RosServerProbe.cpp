#include "RosServerProbe.h"

#include <atomic>
#include <thread>

#ifdef FISHEYE_ROS_ENABLED
#include <chrono>
#include <string>

#include <rclcpp/rclcpp.hpp>

#include "moil_interfaces/srv/axis_sensor.hpp"
#include "moil_interfaces/srv/set_brightness.hpp"
#endif

namespace {

constexpr int kProbeTimeoutMs = 5000;
constexpr int kPollIntervalMs = 100;

QString normaliseNamespace(const QString &value, const QString &fallback) {
    QString ns = value.trimmed();
    while (ns.endsWith('/')) ns.chop(1);
    if (ns.isEmpty()) return fallback;
    return ns.startsWith('/') ? ns : '/' + ns;
}

QString normaliseTopic(const QString &value, const QString &fallback) {
    const QString topic = value.trimmed();
    if (topic.isEmpty()) return fallback;
    return topic.startsWith('/') ? topic : '/' + topic;
}

} // namespace

struct RosServerProbe::Impl {
    std::atomic<quint64> generation{0};
    std::thread worker;
};

RosServerProbe::RosServerProbe(QObject *parent) : QObject(parent), d_(new Impl) {}

RosServerProbe::~RosServerProbe() { stopWorker(); }

void RosServerProbe::stopWorker() {
    ++d_->generation;
    if (d_->worker.joinable()) d_->worker.join();
}

void RosServerProbe::resetAll() {
    stopWorker();
    bool changed = !lastError_.isEmpty();
    lastError_.clear();
    for (ProbeStatus::Status &status : status_) {
        if (status != ProbeStatus::Unknown) {
            status = ProbeStatus::Unknown;
            changed = true;
        }
    }
    if (changed) emit statusesChanged();
}

void RosServerProbe::applyResult(int service, int status, const QString &error) {
    status_[service] = static_cast<ProbeStatus::Status>(status);
    if (!error.isEmpty()) lastError_ = error;
    emit statusesChanged();
}

void RosServerProbe::probeAll(int domainId, const QString &axisNamespace,
                              const QString &monitorNamespace, const QString &cameraTopic) {
    stopWorker();

    lastError_.clear();
    for (ProbeStatus::Status &status : status_) status = ProbeStatus::Checking;
    emit statusesChanged();

    const quint64 generation = d_->generation.load();

    const std::string axisService =
        (normaliseNamespace(axisNamespace, QStringLiteral("/axis")) + "/sensor").toStdString();
    const std::string monitorService =
        (normaliseNamespace(monitorNamespace, QStringLiteral("/monitor")) + "/set_brightness")
            .toStdString();
    const std::string cameraTopicName =
        normaliseTopic(cameraTopic, QStringLiteral("/camera/image_raw/compressed")).toStdString();

#ifndef FISHEYE_ROS_ENABLED
    Q_UNUSED(generation)
    Q_UNUSED(domainId)
    Q_UNUSED(axisService)
    Q_UNUSED(monitorService)
    Q_UNUSED(cameraTopicName)

    for (int service = 0; service < 3; ++service) {
        applyResult(service, ProbeStatus::Failed,
                    QStringLiteral("this build has no ROS 2 support "
                                   "(configure with -DFISHEYE_ENABLE_ROS=ON on Linux)"));
    }
#else
    d_->worker = std::thread([this, generation, domainId, axisService, monitorService,
                              cameraTopicName] {
        auto post = [this, generation](int service, ProbeStatus::Status status,
                                       const QString &error) {
            if (generation != d_->generation.load()) return;
            QMetaObject::invokeMethod(this, "applyResult", Qt::QueuedConnection,
                                      Q_ARG(int, service), Q_ARG(int, static_cast<int>(status)),
                                      Q_ARG(QString, error));
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
            const QString message = QString::fromUtf8(error.what());
            for (int service = 0; service < 3; ++service)
                post(service, ProbeStatus::Failed, message);
            return;
        }

        try {
            auto node = std::make_shared<rclcpp::Node>("fisheye_cali_jojo_probe", nodeOptions);
            auto axisClient = node->create_client<moil_interfaces::srv::AxisSensor>(axisService);
            auto monitorClient =
                node->create_client<moil_interfaces::srv::SetBrightness>(monitorService);

            bool found[3] = {false, false, false};
            const auto deadline =
                std::chrono::steady_clock::now() + std::chrono::milliseconds(kProbeTimeoutMs);

            while (generation == d_->generation.load()) {
                if (!found[Axis] && axisClient->service_is_ready()) {
                    found[Axis] = true;
                    post(Axis, ProbeStatus::Ok, {});
                }
                if (!found[Monitor] && monitorClient->service_is_ready()) {
                    found[Monitor] = true;
                    post(Monitor, ProbeStatus::Ok, {});
                }
                if (!found[Camera] && node->count_publishers(cameraTopicName) > 0) {
                    found[Camera] = true;
                    post(Camera, ProbeStatus::Ok, {});
                }

                if (found[Axis] && found[Monitor] && found[Camera]) break;
                if (std::chrono::steady_clock::now() >= deadline) break;

                std::this_thread::sleep_for(std::chrono::milliseconds(kPollIntervalMs));
            }

            for (int service = 0; service < 3; ++service) {
                if (found[service]) continue;
                post(service, ProbeStatus::Failed,
                     QStringLiteral("no responder on domain %1 within %2 s "
                                    "(wrong domain, wrong subnet, node not running, "
                                    "or namespace mismatch)")
                         .arg(domainId)
                         .arg(kProbeTimeoutMs / 1000));
            }
        } catch (const std::exception &error) {
            const QString message = QString::fromUtf8(error.what());
            for (int service = 0; service < 3; ++service)
                post(service, ProbeStatus::Failed, message);
        }

        context->shutdown("probe finished");
    });
#endif
}
