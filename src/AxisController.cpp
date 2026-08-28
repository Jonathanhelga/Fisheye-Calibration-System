#include "AxisController.h"

#include "AxisState.h"

#include <QCoreApplication>
#include <QTimer>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <future>
#include <mutex>
#include <optional>
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
constexpr int kWaitSliceMs = 200;
constexpr double kSerialRoundTripsPerSecond = 5.7;
constexpr int kRoundTripsPerAxisSample = 5;
constexpr double kLinkBudget = 0.7;
constexpr double kAxisSampleSeconds = kRoundTripsPerAxisSample / kSerialRoundTripsPerSecond;

constexpr double watchHzForAxes(int axes) { return kLinkBudget / (kAxisSampleSeconds * axes); }

constexpr int kPollGapMs =
    static_cast<int>(kAxisSampleSeconds * (1.0 / kLinkBudget - 1.0) * 1000.0);
constexpr int kWatchCallTimeoutMs = 6000;
constexpr int kWatchCallAttempts = 2;
constexpr double kWatchHz = watchHzForAxes(kAxisCount);
constexpr double kFocusWatchHz = watchHzForAxes(1);
constexpr int kStopTimeoutMs = 8000;
constexpr int kMoveTimeoutMs = 120000;

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

    std::mutex queueMutex;
    std::condition_variable queueSignal;
    std::deque<AxisCommandRequest> queue;
    QString watchFocus;

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

void AxisController::applyCapabilities(bool move, bool command, bool sensor, bool position,
                                       int stateSource, const QString &text, quint64 generation) {
    if (generation != d_->generation.load()) return;

    moveAvailable_ = move;
    commandAvailable_ = command;
    sensorAvailable_ = sensor;
    positionAvailable_ = position;
    stateSource_ = static_cast<StateSource>(stateSource);
    capabilityText_ = text;
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

void AxisController::applyCommandOutcome(const QString &axis, bool ok, bool clearPending,
                                         const QString &message, quint64 generation) {
    if (generation != d_->generation.load()) return;

    if (AxisState *state = axisOrNull(axis)) {
        if (clearPending) state->setCommandPending(false, QString());
        else state->markRigReplied();
    }

    if (!ok) emit commandFailed(axis, message);
}

void AxisController::setWatchFocus(const QString &axis) {
    {
        std::lock_guard<std::mutex> lock(d_->queueMutex);
        if (d_->watchFocus == axis) return;
        d_->watchFocus = axis;
    }
    d_->queueSignal.notify_all();
}

void AxisController::enqueueCommand(const AxisCommandRequest &request) {
    {
        std::lock_guard<std::mutex> lock(d_->queueMutex);
        if (request.kind == AxisCommandRequest::Stop) {
            d_->queue.push_front(request);
        } else {
            d_->queue.push_back(request);
        }
    }
    d_->queueSignal.notify_all();
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

    d_->worker = std::thread([this, generation, domainId, ns, nsStd] {
        auto alive = [this, generation] { return generation == d_->generation.load(); };

        auto postConnection = [this, generation](ConnectionState state, const QString &message) {
            QMetaObject::invokeMethod(this, "applyConnection", Qt::QueuedConnection,
                                      Q_ARG(int, static_cast<int>(state)), Q_ARG(QString, message),
                                      Q_ARG(quint64, generation));
        };

        auto postCapabilities = [this, generation](bool move, bool command, bool sensor,
                                                   bool position, StateSource source,
                                                   const QString &text) {
            QMetaObject::invokeMethod(this, "applyCapabilities", Qt::QueuedConnection,
                                      Q_ARG(bool, move), Q_ARG(bool, command), Q_ARG(bool, sensor),
                                      Q_ARG(bool, position), Q_ARG(int, static_cast<int>(source)),
                                      Q_ARG(QString, text), Q_ARG(quint64, generation));
        };

        auto postOutcome = [this, generation](const QString &axis, bool ok, bool clearPending,
                                              const QString &message) {
            QMetaObject::invokeMethod(this, "applyCommandOutcome", Qt::QueuedConnection,
                                      Q_ARG(QString, axis), Q_ARG(bool, ok),
                                      Q_ARG(bool, clearPending), Q_ARG(QString, message),
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
            auto moveClient = node->create_client<moil_interfaces::srv::AxisMove>(nsStd + "/move");
            auto commandClient =
                node->create_client<moil_interfaces::srv::AxisCommand>(nsStd + "/command");

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
                   !(positionClient->service_is_ready() && watchClient->service_is_ready() &&
                     moveClient->service_is_ready() && commandClient->service_is_ready()) &&
                   std::chrono::steady_clock::now() < capabilityDeadline) {
                std::this_thread::sleep_for(std::chrono::milliseconds(kDiscoveryPollMs));
            }

            if (!alive()) {
                context->shutdown("axis session replaced");
                return;
            }

            const bool positionReady = positionClient->service_is_ready();
            const bool watchReady = watchClient->service_is_ready();
            const bool moveReady = moveClient->service_is_ready();
            const bool commandReady = commandClient->service_is_ready();

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
                    request->hz = kWatchHz;
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

            postCapabilities(moveReady, commandReady, true, positionReady, source,
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

            std::optional<rclcpp::Client<moil_interfaces::srv::AxisMove>::FutureAndRequestId>
                moveCall;
            QString moveAxis;
            std::chrono::steady_clock::time_point moveDeadline;
            bool moveCancelled = false;

            auto startMove = [&](const AxisCommandRequest &request) {
                auto message = std::make_shared<moil_interfaces::srv::AxisMove::Request>();
                message->direction = request.direction.toStdString();
                message->distance = request.distance;
                message->speed = request.speed.toStdString();

                moveCall.emplace(moveClient->async_send_request(message));
                moveAxis = request.axis;
                moveCancelled = false;
                moveDeadline = std::chrono::steady_clock::now() +
                               std::chrono::milliseconds(kMoveTimeoutMs);
            };

            auto settleMove = [&] {
                if (!moveCall) return;

                if (moveCall->future.wait_for(std::chrono::seconds(0)) ==
                    std::future_status::ready) {
                    const auto response = moveCall->future.get();
                    moveCall.reset();
                    if (!moveCancelled) {
                        postOutcome(moveAxis, response->success, !response->success,
                                    QString::fromStdString(response->message));
                    }
                    moveAxis.clear();
                    return;
                }

                if (std::chrono::steady_clock::now() < moveDeadline) return;

                moveClient->remove_pending_request(*moveCall);
                moveCall.reset();
                if (!moveCancelled) {
                    postOutcome(moveAxis, false, true,
                                QStringLiteral("no reply in %1 s, the axis may still be moving")
                                    .arg(kMoveTimeoutMs / 1000));
                }
                moveAxis.clear();
            };

            auto runStop = [&](const AxisCommandRequest &request) {
                if (moveCall && request.axis == moveAxis) moveCancelled = true;

                auto message = std::make_shared<moil_interfaces::srv::AxisCommand::Request>();
                message->command = "stop";
                message->axis = request.axis.toStdString();

                auto future = commandClient->async_send_request(message);
                if (!waitFor(future, kStopTimeoutMs)) {
                    commandClient->remove_pending_request(future);
                    postOutcome(request.axis, false, true,
                                QStringLiteral("no reply in %1 s, the axis may still be moving")
                                    .arg(kStopTimeoutMs / 1000));
                    return;
                }

                const auto response = future.get();
                postOutcome(request.axis, response->success, true,
                            QString::fromStdString(response->message));
            };

            auto drainCommands = [&] {
                while (alive()) {
                    AxisCommandRequest request;
                    {
                        std::lock_guard<std::mutex> lock(d_->queueMutex);
                        if (d_->queue.empty()) return;
                        if (d_->queue.front().kind == AxisCommandRequest::Move && moveCall) return;
                        request = d_->queue.front();
                        d_->queue.pop_front();
                    }

                    if (request.kind == AxisCommandRequest::Stop) {
                        runStop(request);
                    } else {
                        startMove(request);
                    }
                }
            };

            int cursor = 0;
            QString appliedFocus;

            while (alive()) {
                executor.spin_some(std::chrono::milliseconds(kSpinSliceMs));
                if (!alive()) break;

                settleMove();

                if (source == WatchTopic) {
                    QString desiredFocus;
                    {
                        std::lock_guard<std::mutex> lock(d_->queueMutex);
                        desiredFocus = d_->watchFocus;
                    }

                    if (desiredFocus != appliedFocus) {
                        callWatch(desiredFocus, true);
                        appliedFocus = desiredFocus;
                    }
                }

                drainCommands();
                if (!alive()) break;

                if (source == WatchTopic) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(kSpinSliceMs));
                    continue;
                }

                QString focus;
                {
                    std::lock_guard<std::mutex> lock(d_->queueMutex);
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

                std::this_thread::sleep_for(std::chrono::milliseconds(kPollGapMs));
            }

            if (moveCall) {
                moveClient->remove_pending_request(*moveCall);
                moveCall.reset();
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

void AxisController::stopWorker() {
    ++d_->generation;
    {
        std::lock_guard<std::mutex> lock(d_->queueMutex);
        d_->queue.clear();
        d_->watchFocus.clear();
    }
    d_->queueSignal.notify_all();
    if (d_->worker.joinable()) d_->worker.join();
}

void AxisController::teardownSession() {
    stopWorker();
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
    if (state->stale()) {
        emit commandRejected(key, tr("%1 state is stale, position is unknown").arg(axisLabel(key)));
        return;
    }

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

    AxisCommandRequest request;
    request.kind = AxisCommandRequest::Move;
    request.axis = key;
    request.direction = key + QLatin1Char('_') + direction;
    request.distance = distance;
    request.speed = speedWord(speed);

    state->setCommandPending(true, tr("Moving %1 %2").arg(axisLabel(key), sideWord(key, side)));
    setWatchFocus(key);
    enqueueCommand(request);
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
    if (state->stale()) {
        emit commandRejected(key, tr("%1 state is stale, position is unknown").arg(axisLabel(key)));
        return;
    }

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

    AxisCommandRequest request;
    request.kind = AxisCommandRequest::Stop;
    request.axis = key;
    enqueueCommand(request);
}

void AxisController::stopAll() {
    if (!guardStop(QString())) return;

    for (AxisState *state : std::as_const(d_->all)) {
        AxisCommandRequest request;
        request.kind = AxisCommandRequest::Stop;
        request.axis = state->name();
        enqueueCommand(request);
    }
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
