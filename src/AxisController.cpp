#include "AxisController.h"

#include "AxisState.h"

#include <QCoreApplication>
#include <QTimer>

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

#ifdef FISHEYE_ROS_ENABLED
#include <memory>
#include <string>

#include <rclcpp/rclcpp.hpp>

#include "moil_interfaces/msg/axis_state.hpp"
#include "moil_interfaces/srv/axis_command.hpp"
#include "moil_interfaces/srv/axis_move.hpp"
#include "moil_interfaces/srv/axis_position.hpp"
#include "moil_interfaces/srv/axis_sensor.hpp"
#include "moil_interfaces/srv/axis_watch.hpp"
#endif

namespace {

constexpr int kStalenessTickMs = 500;

constexpr int kMinDomainId = 0;
constexpr int kMaxDomainId = 232;

constexpr int kStopTimeoutMs = 8000;
constexpr int kMoveTimeoutMs = 120000;

#ifdef FISHEYE_ROS_ENABLED
struct AxisSensorNames {
    const char *axis;
    const char *low;
    const char *org;
    const char *high;
    const char *moving;
};

constexpr AxisSensorNames kSensorNames[] = {
    {"x", "x_left", "x_org", "x_right", "x_move"},
    {"y", "y_down", "y_org", "y_up", "y_move"},
    {"z", "z_back", "z_org", "z_forward", "z_move"},
    {"yaw", "yaw_left", "yaw_org", "yaw_right", "yaw_move"},
    {"pitch", "pitch_down", "pitch_org", "pitch_up", "pitch_move"},
};

constexpr int kAxisCount = 5;

constexpr int kDiscoveryTimeoutMs = 5000;
constexpr int kDiscoveryPollMs = 100;
constexpr int kCallTimeoutMs = 2000;
constexpr int kCapabilityGraceMs = 2000;
constexpr int kSpinSliceMs = 20;
constexpr int kCommandSpinGapMs = 5;
constexpr int kWaitSliceMs = 200;
constexpr double kSerialRoundTripsPerSecond = 5.7;
constexpr int kRoundTripsPerAxisSample = 5;
constexpr double kLinkBudget = 0.85;
constexpr double kIdleLinkBudget = 0.30;
constexpr double kAxisSampleSeconds = kRoundTripsPerAxisSample / kSerialRoundTripsPerSecond;

constexpr double watchHzFor(double budget, int axes) {
    return budget / (kAxisSampleSeconds * axes);
}

constexpr int pollGapMsFor(double budget) {
    return static_cast<int>(kAxisSampleSeconds * (1.0 / budget - 1.0) * 1000.0);
}

constexpr int kActivePollGapMs = pollGapMsFor(kLinkBudget);
constexpr int kIdlePollGapMs = pollGapMsFor(kIdleLinkBudget);
constexpr int kWatchCallTimeoutMs = 6000;
constexpr int kWatchCallAttempts = 2;
constexpr double kIdleWatchHz = watchHzFor(kIdleLinkBudget, kAxisCount);
constexpr double kFocusWatchHz = watchHzFor(kLinkBudget, 1);

int triFrom(bool success, bool value) { 
    if (!success) return AxisState::Unreadable;
    return value ? AxisState::Triggered : AxisState::Clear;
}
#endif

QString normaliseNamespace(const QString &value) {
    QString ns = value.trimmed();
    while (ns.endsWith('/')) ns.chop(1);
    if (ns.isEmpty()) return QStringLiteral("/axis");
    return ns.startsWith('/') ? ns : '/' + ns;
}

QString sideWord(const QString &axis, AxisController::Side side) {
    const bool high = side == AxisController::HighSide;
    if (axis == QLatin1String("x")) return high ? QStringLiteral("right") : QStringLiteral("left");
    if (axis == QLatin1String("y")) return high ? QStringLiteral("up") : QStringLiteral("down");
    if (axis == QLatin1String("z")) return high ? QStringLiteral("forward") : QStringLiteral("backward");
    if (axis == QLatin1String("yaw")) return high ? QStringLiteral("right") : QStringLiteral("left");
    if (axis == QLatin1String("pitch")) return high ? QStringLiteral("up") : QStringLiteral("down");
    return high ? QStringLiteral("high") : QStringLiteral("low");
}

QString directionWord(const QString &axis, AxisController::Side side) {
    const bool high = side == AxisController::HighSide;
    if (axis == QLatin1String("x")) return high ? QStringLiteral("right") : QStringLiteral("left");
    if (axis == QLatin1String("y")) return high ? QStringLiteral("up") : QStringLiteral("down");
    if (axis == QLatin1String("z")) return high ? QStringLiteral("forward") : QStringLiteral("back");
    if (axis == QLatin1String("yaw")) return high ? QStringLiteral("right") : QStringLiteral("left");
    if (axis == QLatin1String("pitch")) return high ? QStringLiteral("up") : QStringLiteral("down");
    return QString();
}

QString speedWord(AxisController::Speed speed) {
    switch (speed) {
    case AxisController::Low:
        return QStringLiteral("Low");
    case AxisController::Mid:
        return QStringLiteral("Mid");
    case AxisController::High:
        return QStringLiteral("High");
    }
    return QStringLiteral("Mid");
}

QString axisLabel(const QString &axis) {
    if (axis == QLatin1String("yaw")) return QStringLiteral("Yaw");
    if (axis == QLatin1String("pitch")) return QStringLiteral("Pitch");
    return axis.toUpper();
}

} // namespace

struct AxisController::Impl {
    std::atomic<quint64> generation{0};
    std::thread worker;
    std::thread commandWorker;

    std::mutex focusMutex;
    QString watchFocus;

    std::mutex commandMutex;
#ifdef FISHEYE_ROS_ENABLED
    rclcpp::Client<moil_interfaces::srv::AxisMove>::SharedPtr moveClient;
    rclcpp::Client<moil_interfaces::srv::AxisCommand>::SharedPtr commandClient;
#endif

    AxisState *x = nullptr;
    AxisState *y = nullptr;
    AxisState *z = nullptr;
    AxisState *yaw = nullptr;
    AxisState *pitch = nullptr;

    QList<AxisState *> all;
};

AxisController::AxisController(QObject *parent) : QObject(parent), d_(new Impl) {
    qRegisterMetaType<AxisSample>();

    d_->x = new AxisState(QStringLiteral("x"), QStringLiteral("mm"), this);
    d_->y = new AxisState(QStringLiteral("y"), QStringLiteral("mm"), this);
    d_->z = new AxisState(QStringLiteral("z"), QStringLiteral("mm"), this);
    d_->yaw = new AxisState(QStringLiteral("yaw"), QStringLiteral("°"), this);
    d_->pitch = new AxisState(QStringLiteral("pitch"), QStringLiteral("°"), this);
    d_->all = {d_->x, d_->y, d_->z, d_->yaw, d_->pitch};

    for (AxisState *state : std::as_const(d_->all)) {
        connect(state, &AxisState::activityChanged, this, &AxisController::recomputeActivity);
        connect(state, &AxisState::interlockChanged, this, &AxisController::recomputeActivity);
    }

    stalenessTimer_ = new QTimer(this);
    stalenessTimer_->setInterval(kStalenessTickMs);
    connect(stalenessTimer_, &QTimer::timeout, this, [this] {
        for (AxisState *state : std::as_const(d_->all)) state->refreshStaleness();

        if (!connected()) return;
        for (AxisState *state : std::as_const(d_->all)) {
            if (!state->stale()) return;
        }

        stalledFrom_ = connectionState_;
        setConnectionState(Stalled,
                           tr("the axis node stopped answering, press Update to reconnect"));
    });

    connect(qApp, &QCoreApplication::aboutToQuit, this, [this] { teardownSession(); });
}

AxisController::~AxisController() { teardownSession(); }

AxisState *AxisController::x() const { return d_->x; }
AxisState *AxisController::y() const { return d_->y; }
AxisState *AxisController::z() const { return d_->z; }
AxisState *AxisController::yaw() const { return d_->yaw; }
AxisState *AxisController::pitch() const { return d_->pitch; }
QList<AxisState *> AxisController::allAxes() const { return d_->all; }

AxisState *AxisController::axisOrNull(const QString &name) const {
    const QString key = name.trimmed().toLower();
    for (AxisState *state : std::as_const(d_->all)) {
        if (state->name() == key) return state;
    }
    return nullptr;
}

AxisState *AxisController::axis(const QString &name) const { return axisOrNull(name); }

bool AxisController::anyMoving() const {
    for (AxisState *state : std::as_const(d_->all)) {
        if (state->moving()) return true;
    }
    return false;
}

bool AxisController::busy() const {
    if (homing()) return true;
    for (AxisState *state : std::as_const(d_->all)) {
        if (state->busy()) return true;
    }
    return false;
}

bool AxisController::dataFresh() const {
    if (!connected()) return false;
    for (AxisState *state : std::as_const(d_->all)) {
        if (state->stale()) return false;
    }
    return true;
}

QStringList AxisController::axesInGroup(Group group) const {
    switch (group) {
    case GroupXY:
        return {QStringLiteral("x"), QStringLiteral("y")};
    case GroupZ:
        return {QStringLiteral("z")};
    case GroupRotation:
        return {QStringLiteral("yaw"), QStringLiteral("pitch")};
    }
    return {};
}

void AxisController::setConnectionState(ConnectionState state, const QString &message) {
    const bool changed = connectionState_ != state;
    connectionState_ = state;

    if (message != lastError_) {
        lastError_ = message;
        emit statusChanged();
    }

    if (state == Connected || state == Degraded) {
        stalenessTimer_->start();
    } else {
        stalenessTimer_->stop();
        for (AxisState *axis : std::as_const(d_->all)) axis->resetToUnknown();
    }

    if (changed) {
        emit connectionChanged();
        recomputeActivity();
    }
}

void AxisController::clearCapabilities() {
    moveAvailable_ = false;
    commandAvailable_ = false;
    sensorAvailable_ = false;
    positionAvailable_ = false;
    limitMoveAvailable_ = false;
    homeActionAvailable_ = false;
    stateSource_ = NoSource;
    capabilityText_.clear();
    emit capabilitiesChanged();
}

void AxisController::applyConnection(int state, const QString &message, quint64 generation) {
    if (generation != d_->generation.load()) return;
    setConnectionState(static_cast<ConnectionState>(state), message);
}

void AxisController::applyStateCapabilities(bool sensor, bool position, int stateSource,
                                            const QString &text, quint64 generation) {
    if (generation != d_->generation.load()) return;

    sensorAvailable_ = sensor;
    positionAvailable_ = position;
    stateSource_ = static_cast<StateSource>(stateSource);
    capabilityText_ = text;
    emit capabilitiesChanged();
}

void AxisController::applyCommandCapabilities(bool move, bool command, quint64 generation) {
    if (generation != d_->generation.load()) return;

    moveAvailable_ = move;
    commandAvailable_ = command;
    emit capabilitiesChanged();
}

void AxisController::applySample(const AxisSample &sample, quint64 generation) {
    if (generation != d_->generation.load()) return;

    AxisState *state = axisOrNull(sample.axis);
    if (!state) return;

    state->applySample(sample.low, sample.org, sample.high, sample.moving, sample.coordinate,
                       sample.raw, sample.hasZero, sample.positionValid);

    if (connectionState_ == Stalled) setConnectionState(stalledFrom_, QString());
}

void AxisController::applyMoveOutcome(const QString &axis, bool ok, const QString &message,
                                      quint64 generation) {
    if (generation != d_->generation.load()) return;

    ++commandToken_[axis];
    if (cancelledMoves_.remove(axis)) return;

    if (AxisState *state = axisOrNull(axis)) {
        if (ok) state->markRigReplied();
        else state->setCommandPending(false, QString());
    }

    if (!ok) emit commandFailed(axis, message);
}

void AxisController::applyStopOutcome(const QString &axis, bool ok, const QString &message,
                                      quint64 generation) {
    if (generation != d_->generation.load()) return;

    ++commandToken_[axis];
    if (AxisState *state = axisOrNull(axis)) state->setCommandPending(false, QString());

    if (!ok) emit commandFailed(axis, message);
}

void AxisController::setWatchFocus(const QString &axis) {
    std::lock_guard<std::mutex> lock(d_->focusMutex);
    d_->watchFocus = axis;
}

bool AxisController::sendMove(const QString &axis, const QString &direction, double distance,
                              const QString &speed) {
#ifdef FISHEYE_ROS_ENABLED
    rclcpp::Client<moil_interfaces::srv::AxisMove>::SharedPtr client;
    {
        std::lock_guard<std::mutex> lock(d_->commandMutex);
        client = d_->moveClient;
    }
    if (!client || !client->service_is_ready()) return false;

    auto message = std::make_shared<moil_interfaces::srv::AxisMove::Request>();
    message->direction = direction.toStdString();
    message->distance = distance;
    message->speed = speed.toStdString();

    const quint64 generation = d_->generation.load();
    client->async_send_request(
        message, [this, axis, generation](
                     rclcpp::Client<moil_interfaces::srv::AxisMove>::SharedFuture future) {
            const auto response = future.get();
            QMetaObject::invokeMethod(this, "applyMoveOutcome", Qt::QueuedConnection,
                                      Q_ARG(QString, axis), Q_ARG(bool, response->success),
                                      Q_ARG(QString, QString::fromStdString(response->message)),
                                      Q_ARG(quint64, generation));
        });
    return true;
#else
    Q_UNUSED(axis)
    Q_UNUSED(direction)
    Q_UNUSED(distance)
    Q_UNUSED(speed)
    return false;
#endif
}

bool AxisController::sendStop(const QString &axis) {
#ifdef FISHEYE_ROS_ENABLED
    rclcpp::Client<moil_interfaces::srv::AxisCommand>::SharedPtr client;
    {
        std::lock_guard<std::mutex> lock(d_->commandMutex);
        client = d_->commandClient;
    }
    if (!client || !client->service_is_ready()) return false;

    auto message = std::make_shared<moil_interfaces::srv::AxisCommand::Request>();
    message->command = "stop";
    message->axis = axis.toStdString();

    const quint64 generation = d_->generation.load();
    client->async_send_request(
        message, [this, axis, generation](
                     rclcpp::Client<moil_interfaces::srv::AxisCommand>::SharedFuture future) {
            const auto response = future.get();
            QMetaObject::invokeMethod(this, "applyStopOutcome", Qt::QueuedConnection,
                                      Q_ARG(QString, axis), Q_ARG(bool, response->success),
                                      Q_ARG(QString, QString::fromStdString(response->message)),
                                      Q_ARG(quint64, generation));
        });
    return true;
#else
    Q_UNUSED(axis)
    return false;
#endif
}

void AxisController::armTimeout(const QString &axis, int ms, const QString &what) {
    const quint64 token = ++commandToken_[axis];
    const quint64 generation = d_->generation.load();
    QTimer::singleShot(ms, this, [this, axis, token, generation, ms, what] {
        if (generation != d_->generation.load()) return;
        if (commandToken_.value(axis) != token) return;

        ++commandToken_[axis];
        cancelledMoves_.remove(axis);
        if (AxisState *state = axisOrNull(axis)) state->setCommandPending(false, QString());
        emit operationTimedOut(axis, what, ms);
    });
}

void AxisController::connectTo(int domainId, const QString &axisNamespace, bool force) {
    const QString ns = normaliseNamespace(axisNamespace);

    if (domainId < kMinDomainId || domainId > kMaxDomainId) {
        teardownSession();
        domainId_ = domainId;
        axisNamespace_ = ns;
        setConnectionState(Failed, QStringLiteral("domain id %1 is outside the valid range %2 to %3")
                                       .arg(domainId)
                                       .arg(kMinDomainId)
                                       .arg(kMaxDomainId));
        emit connectionChanged();
        return;
    }

    if (!force && connectionState_ == Connected && domainId == domainId_ && ns == axisNamespace_) {
        emit connectionChanged();
        return;
    }

    teardownSession();

    domainId_ = domainId;
    axisNamespace_ = ns;
    const quint64 generation = d_->generation.load();

    setConnectionState(Connecting, QString());
    emit connectionChanged();

#ifdef FISHEYE_ROS_ENABLED
    const std::string nsStd = ns.toStdString();

    d_->commandWorker = std::thread([this, generation, domainId, nsStd] {
        auto alive = [this, generation] { return generation == d_->generation.load(); };

        auto context = std::make_shared<rclcpp::Context>();
        rclcpp::NodeOptions nodeOptions;

        try {
            rclcpp::InitOptions initOptions;
            initOptions.set_domain_id(static_cast<size_t>(domainId));
            initOptions.auto_initialize_logging(false);
            context->init(0, nullptr, initOptions);
            nodeOptions.context(context);

            auto node = std::make_shared<rclcpp::Node>("fisheye_cali_jojo_axis_cmd", nodeOptions);

            rclcpp::ExecutorOptions executorOptions;
            executorOptions.context = context;
            rclcpp::executors::SingleThreadedExecutor executor(executorOptions);
            executor.add_node(node);

            auto moveClient = node->create_client<moil_interfaces::srv::AxisMove>(nsStd + "/move");
            auto commandClient =
                node->create_client<moil_interfaces::srv::AxisCommand>(nsStd + "/command");

            {
                std::lock_guard<std::mutex> lock(d_->commandMutex);
                d_->moveClient = moveClient;
                d_->commandClient = commandClient;
            }

            const auto deadline = std::chrono::steady_clock::now() +
                                  std::chrono::milliseconds(kDiscoveryTimeoutMs);
            bool announced = false;

            while (alive()) {
                executor.spin_some(std::chrono::milliseconds(kSpinSliceMs));
                if (!alive()) break;

                const bool moveReady = moveClient->service_is_ready();
                const bool commandReady = commandClient->service_is_ready();

                if (!announced &&
                    ((moveReady && commandReady) || std::chrono::steady_clock::now() > deadline)) {
                    announced = true;
                    QMetaObject::invokeMethod(this, "applyCommandCapabilities",
                                              Qt::QueuedConnection, Q_ARG(bool, moveReady),
                                              Q_ARG(bool, commandReady),
                                              Q_ARG(quint64, generation));
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(kCommandSpinGapMs));
            }

            {
                std::lock_guard<std::mutex> lock(d_->commandMutex);
                d_->moveClient.reset();
                d_->commandClient.reset();
            }
        } catch (const std::exception &error) {
            QMetaObject::invokeMethod(this, "applyCommandCapabilities", Qt::QueuedConnection,
                                      Q_ARG(bool, false), Q_ARG(bool, false),
                                      Q_ARG(quint64, generation));
            qWarning("axis command link failed: %s", error.what());
        }

        context->shutdown("axis command session finished");
    });

    d_->worker = std::thread([this, generation, domainId, ns, nsStd] {
        auto alive = [this, generation] { return generation == d_->generation.load(); };

        auto postConnection = [this, generation](ConnectionState state, const QString &message) {
            QMetaObject::invokeMethod(this, "applyConnection", Qt::QueuedConnection,
                                      Q_ARG(int, static_cast<int>(state)), Q_ARG(QString, message),
                                      Q_ARG(quint64, generation));
        };

        auto postCapabilities = [this, generation](bool sensor, bool position, StateSource source,
                                                   const QString &text) {
            QMetaObject::invokeMethod(this, "applyStateCapabilities", Qt::QueuedConnection,
                                      Q_ARG(bool, sensor), Q_ARG(bool, position),
                                      Q_ARG(int, static_cast<int>(source)), Q_ARG(QString, text),
                                      Q_ARG(quint64, generation));
        };

        auto postSample = [this, generation](const AxisSample &sample) {
            QMetaObject::invokeMethod(this, "applySample", Qt::QueuedConnection,
                                      Q_ARG(AxisSample, sample), Q_ARG(quint64, generation));
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
            postConnection(Failed, QString::fromUtf8(error.what()));
            return;
        }

        try {
            auto node = std::make_shared<rclcpp::Node>("fisheye_cali_jojo_axis", nodeOptions);

            rclcpp::ExecutorOptions executorOptions;
            executorOptions.context = context;
            rclcpp::executors::SingleThreadedExecutor executor(executorOptions);
            executor.add_node(node);

            auto sensorClient =
                node->create_client<moil_interfaces::srv::AxisSensor>(nsStd + "/sensor");
            auto positionClient =
                node->create_client<moil_interfaces::srv::AxisPosition>(nsStd + "/position");
            auto watchClient =
                node->create_client<moil_interfaces::srv::AxisWatch>(nsStd + "/watch");

            const auto discoveryDeadline =
                std::chrono::steady_clock::now() + std::chrono::milliseconds(kDiscoveryTimeoutMs);
            while (alive() && !sensorClient->service_is_ready() &&
                   std::chrono::steady_clock::now() < discoveryDeadline) {
                std::this_thread::sleep_for(std::chrono::milliseconds(kDiscoveryPollMs));
            }

            if (!alive()) {
                context->shutdown("axis session replaced");
                return;
            }

            if (!sensorClient->service_is_ready()) {
                postConnection(Failed,
                               QStringLiteral("no axis node answering %1/sensor on domain %2 "
                                              "within %3 s")
                                   .arg(ns)
                                   .arg(domainId)
                                   .arg(kDiscoveryTimeoutMs / 1000));
                context->shutdown("axis session failed");
                return;
            }

            const auto capabilityDeadline =
                std::chrono::steady_clock::now() + std::chrono::milliseconds(kCapabilityGraceMs);
            while (alive() &&
                   !(positionClient->service_is_ready() && watchClient->service_is_ready()) &&
                   std::chrono::steady_clock::now() < capabilityDeadline) {
                std::this_thread::sleep_for(std::chrono::milliseconds(kDiscoveryPollMs));
            }

            if (!alive()) {
                context->shutdown("axis session replaced");
                return;
            }

            const bool positionReady = positionClient->service_is_ready();
            const bool watchReady = watchClient->service_is_ready();

            rclcpp::Subscription<moil_interfaces::msg::AxisState>::SharedPtr subscription;
            StateSource source = Polling;

            auto waitFor = [&](auto &future, int totalMs) {
                for (int waited = 0; waited < totalMs; waited += kWaitSliceMs) {
                    if (!alive()) return false;
                    if (executor.spin_until_future_complete(
                            future, std::chrono::milliseconds(kWaitSliceMs)) ==
                        rclcpp::FutureReturnCode::SUCCESS)
                        return true;
                }
                return false;
            };

            auto callWatch = [&](const QString &focus, bool on) {
                auto request = std::make_shared<moil_interfaces::srv::AxisWatch::Request>();
                if (!on) {
                    request->hz = 0.0;
                } else if (focus.isEmpty()) {
                    for (const AxisSensorNames &names : kSensorNames)
                        request->axes.emplace_back(names.axis);
                    request->hz = kIdleWatchHz;
                } else {
                    request->axes.emplace_back(focus.toStdString());
                    request->hz = kFocusWatchHz;
                }

                auto future = watchClient->async_send_request(request);
                if (!waitFor(future, kWatchCallTimeoutMs)) {
                    watchClient->remove_pending_request(future);
                    return false;
                }
                return future.get()->success;
            };

            if (watchReady) {
                subscription = node->create_subscription<moil_interfaces::msg::AxisState>(
                    nsStd + "/state", rclcpp::QoS(rclcpp::KeepLast(10)),
                    [postSample](moil_interfaces::msg::AxisState::SharedPtr msg) {
                        AxisSample sample;
                        sample.axis = QString::fromStdString(msg->axis).trimmed().toLower();
                        sample.low = msg->sensor_low;
                        sample.org = msg->sensor_org;
                        sample.high = msg->sensor_high;
                        sample.moving = msg->sensor_moving;
                        sample.coordinate = QString::fromStdString(msg->coordinate);
                        sample.raw = QString::fromStdString(msg->raw);
                        sample.hasZero = msg->has_zero;
                        sample.positionValid = !sample.coordinate.isEmpty();
                        postSample(sample);
                    });

                for (int attempt = 0; attempt < kWatchCallAttempts && alive(); ++attempt) {
                    if (callWatch(QString(), true)) {
                        source = WatchTopic;
                        break;
                    }
                }
                if (source != WatchTopic) subscription.reset();
            }

            postCapabilities(true, positionReady, source,
                             source == WatchTopic
                                 ? QStringLiteral("streaming %1/state").arg(ns)
                                 : QStringLiteral("polling %1/sensor").arg(ns));

            postConnection(positionReady ? Connected : Degraded,
                           positionReady
                               ? QString()
                               : QStringLiteral("%1/position is not being served, "
                                                "coordinates are unavailable")
                                     .arg(ns));

            auto readSensor = [&](const char *name) -> int {
                if (!alive()) return AxisState::Unreadable;
                auto request = std::make_shared<moil_interfaces::srv::AxisSensor::Request>();
                request->name = name;
                auto future = sensorClient->async_send_request(request);
                if (!waitFor(future, kCallTimeoutMs)) {
                    sensorClient->remove_pending_request(future);
                    return AxisState::Unreadable;
                }
                const auto response = future.get();
                return triFrom(response->success, response->value);
            };

            auto readPosition = [&](AxisSample &sample) {
                if (!alive()) return;
                auto request = std::make_shared<moil_interfaces::srv::AxisPosition::Request>();
                request->axis = sample.axis.toStdString();
                auto future = positionClient->async_send_request(request);
                if (!waitFor(future, kCallTimeoutMs)) {
                    positionClient->remove_pending_request(future);
                    return;
                }
                const auto response = future.get();
                if (!response->success) return;
                sample.coordinate = QString::fromStdString(response->coordinate);
                sample.raw = QString::fromStdString(response->raw);
                sample.hasZero = response->has_zero;
                sample.positionValid = !sample.coordinate.isEmpty();
            };

            int cursor = 0;
            int primed = 0;
            QString appliedFocus;

            while (alive()) {
                executor.spin_some(std::chrono::milliseconds(kSpinSliceMs));
                if (!alive()) break;

                if (source == WatchTopic) {
                    QString desiredFocus;
                    {
                        std::lock_guard<std::mutex> lock(d_->focusMutex);
                        desiredFocus = d_->watchFocus;
                    }

                    if (desiredFocus != appliedFocus) {
                        callWatch(desiredFocus, true);
                        appliedFocus = desiredFocus;
                    }
                }

                if (source == WatchTopic) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(kSpinSliceMs));
                    continue;
                }

                QString focus;
                {
                    std::lock_guard<std::mutex> lock(d_->focusMutex);
                    focus = d_->watchFocus;
                }

                int index = -1;
                if (!focus.isEmpty()) {
                    for (int i = 0; i < kAxisCount; ++i) {
                        if (focus == QLatin1String(kSensorNames[i].axis)) {
                            index = i;
                            break;
                        }
                    }
                }
                if (index < 0) {
                    index = cursor;
                    cursor = (cursor + 1) % kAxisCount;
                }
                if (primed < kAxisCount) ++primed;

                const AxisSensorNames &names = kSensorNames[index];

                AxisSample sample;
                sample.axis = QString::fromLatin1(names.axis);
                sample.low = readSensor(names.low);
                sample.org = readSensor(names.org);
                sample.high = readSensor(names.high);
                sample.moving = readSensor(names.moving);
                if (positionReady) readPosition(sample);

                if (!alive()) break;

                const bool heardSomething = sample.low != AxisState::Unreadable ||
                                            sample.org != AxisState::Unreadable ||
                                            sample.high != AxisState::Unreadable ||
                                            sample.moving != AxisState::Unreadable ||
                                            sample.positionValid;
                if (heardSomething) postSample(sample);

                const bool idle = focus.isEmpty() && primed >= kAxisCount;
                const int gap = idle ? kIdlePollGapMs : kActivePollGapMs;

                for (int slept = 0; slept < gap && alive(); slept += kSpinSliceMs) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(kSpinSliceMs));
                    if (!idle) continue;
                    std::lock_guard<std::mutex> lock(d_->focusMutex);
                    if (!d_->watchFocus.isEmpty()) break;
                }
            }

            if (source == WatchTopic) callWatch(QString(), false);
        } catch (const std::exception &error) {
            postConnection(Failed, QString::fromUtf8(error.what()));
        }

        context->shutdown("axis session finished");
    });
#else
    Q_UNUSED(generation)
    applyConnection(Failed,
                    QStringLiteral("this build has no ROS 2 support "
                                   "(configure with -DFISHEYE_ENABLE_ROS=ON on Linux)"),
                    d_->generation.load());
#endif
}

void AxisController::disconnectFromRig() {
    teardownSession();
    setConnectionState(Disconnected, QString());
}

void AxisController::stopWorkers() {
    ++d_->generation;
    {
        std::lock_guard<std::mutex> lock(d_->focusMutex);
        d_->watchFocus.clear();
    }
    if (d_->worker.joinable()) d_->worker.join();
    if (d_->commandWorker.joinable()) d_->commandWorker.join();
}

void AxisController::teardownSession() {
    stopWorkers();
    commandToken_.clear();
    cancelledMoves_.clear();
    stalenessTimer_->stop();

    for (AxisState *state : std::as_const(d_->all)) state->resetToUnknown();

    homingGroup_.clear();
    clearCapabilities();
    recomputeActivity();
}

bool AxisController::guardCommand(const QString &axis, bool needsMove) {
    if (!connected()) {
        emit commandRejected(axis, tr("not connected to the axis node"));
        return false;
    }
    if (needsMove && !moveAvailable_) {
        emit commandRejected(axis, tr("the axis node is not serving %1/move").arg(axisNamespace_));
        return false;
    }
    AxisState *state = axisOrNull(axis);
    if (!state) {
        emit commandRejected(axis, tr("unknown axis"));
        return false;
    }
    if (state->busy()) {
        emit commandRejected(axis, tr("%1 is already moving").arg(axisLabel(axis)));
        return false;
    }
    if (busy()) {
        emit commandRejected(axis, tr("another axis is moving"));
        return false;
    }
    return true;
}

void AxisController::jog(const QString &axis, Side side, double distance, Speed speed) {
    const QString key = axis.trimmed().toLower();
    if (distance <= 0.0) {
        emit commandRejected(key, tr("step distance must be greater than zero"));
        return;
    }
    if (!guardCommand(key, true)) return;

    AxisState *state = axisOrNull(key);

    const bool high = side == HighSide;
    if (high ? state->highBlocked() : state->lowBlocked()) {
        emit commandRejected(key, tr("%1 %2 is blocked by its limit sensor")
                                      .arg(axisLabel(key), sideWord(key, side)));
        return;
    }

    const QString direction = directionWord(key, side);
    if (direction.isEmpty()) {
        emit commandRejected(key, tr("unknown axis"));
        return;
    }

    if (!sendMove(key, key + QLatin1Char('_') + direction, distance, speedWord(speed))) {
        emit commandRejected(key, tr("the axis node is not serving %1/move").arg(axisNamespace_));
        return;
    }

    cancelledMoves_.remove(key);
    state->setCommandPending(true, tr("Moving %1 %2").arg(axisLabel(key), sideWord(key, side)));
    armTimeout(key, kMoveTimeoutMs, tr("move"));
    setWatchFocus(key);
}

void AxisController::driveToLimit(const QString &axis, Side side, Speed speed) {
    Q_UNUSED(speed)

    const QString key = axis.trimmed().toLower();
    if (!guardCommand(key, false)) return;
    if (!limitMoveAvailable_) {
        emit commandRejected(key, tr("drive-to-limit is not wired to the rig yet"));
        return;
    }

    AxisState *state = axisOrNull(key);

    const bool high = side == HighSide;
    if (high ? state->highBlocked() : state->lowBlocked()) {
        emit commandRejected(key, tr("%1 is already on its %2 limit")
                                      .arg(axisLabel(key), sideWord(key, side)));
        return;
    }

    emit commandRejected(key, tr("drive-to-limit is not wired to the rig yet"));
}

void AxisController::homeGroup(Group group) {
    if (!connected()) {
        emit commandRejected(QString(), tr("not connected to the axis node"));
        return;
    }
    if (busy()) {
        emit commandRejected(QString(), tr("an axis is already moving"));
        return;
    }
    if (!dataFresh()) {
        emit commandRejected(QString(), tr("axis state is stale, position is unknown"));
        return;
    }
    emit homeGroupFinished(group, false, tr("homing is not wired to the rig yet"));
}

bool AxisController::guardStop(const QString &axis) {
    if (!hasSession()) {
        emit commandRejected(axis, tr("not connected to the axis node"));
        return false;
    }
    if (!commandAvailable_) {
        emit commandRejected(axis,
                             tr("the axis node is not serving %1/command").arg(axisNamespace_));
        return false;
    }
    return true;
}

void AxisController::stopAxis(const QString &axis) {
    const QString key = axis.trimmed().toLower();
    if (!guardStop(key)) return;
    if (!axisOrNull(key)) {
        emit commandRejected(key, tr("unknown axis"));
        return;
    }

    AxisState *state = axisOrNull(key);
    if (state->busy()) cancelledMoves_.insert(key);

    if (!sendStop(key)) {
        emit commandRejected(key,
                             tr("the axis node is not serving %1/command").arg(axisNamespace_));
        return;
    }

    armTimeout(key, kStopTimeoutMs, tr("stop"));
}

void AxisController::stopAll() {
    if (!guardStop(QString())) return;

    for (AxisState *state : std::as_const(d_->all)) stopAxis(state->name());
}

void AxisController::recomputeActivity() {
    QString text;
    for (AxisState *state : std::as_const(d_->all)) {
        if (!state->busy()) continue;
        const QString label = axisLabel(state->name());
        text = state->awaitingRig() ? tr("Sent to %1, waiting for the rig").arg(label)
             : state->moving()      ? tr("Moving %1").arg(label)
                                    : tr("%1 is stopping").arg(label);
        break;
    }
    if (homing()) text = tr("Homing %1").arg(homingGroup_);

    if (text == activityText_ && busy() == busyReported_ && anyMoving() == movingReported_ &&
        dataFresh() == freshReported_)
        return;

    activityText_ = text;
    busyReported_ = busy();
    if (!busyReported_) setWatchFocus(QString());
    movingReported_ = anyMoving();
    freshReported_ = dataFresh();
    emit activityChanged();
}
