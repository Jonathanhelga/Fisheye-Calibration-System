#include "axis_node.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>

#include <QDateTime>
#include <QElapsedTimer>
#include <QString>
#include <QThread>

#include "server_context.h"

namespace {

// Sensor endpoint names per axis, in the order the lamps sit on the form:
// s1 = low end (left/down/back), s2 = origin, s3 = high end, mv = moving.
struct AxisMap {
    const char *s1;
    const char *s2;
    const char *s3;
    const char *mv;
    bool valid = true;
};

AxisMap axisMap(const QString &axis) {
    const QString a = axis.trimmed().toLower();
    if (a == "x") return {"is_sensor_x_left", "is_sensor_x_org", "is_sensor_x_right", "is_sensor_x_move"};
    if (a == "y") return {"is_sensor_y_down", "is_sensor_y_org", "is_sensor_y_up", "is_sensor_y_move"};
    if (a == "z") return {"is_sensor_z_back", "is_sensor_z_org", "is_sensor_z_forward", "is_sensor_z_move"};
    if (a == "yaw") return {"is_sensor_yaw_left", "is_sensor_yaw_org", "is_sensor_yaw_right", "is_sensor_yaw_move"};
    if (a == "pitch") return {"is_sensor_pitch_down", "is_sensor_pitch_org", "is_sensor_pitch_up", "is_sensor_pitch_move"};
    return {"", "", "", "", false};
}

// The move method for one axis and one side. Named exactly as the endpoints
// were, so a direction in a log line leads straight to the call.
QString driveOneWay(AxisDevice &axis, const QString &a, bool highSide, double mm,
                    const QString &speed) {
    if (a == "x") return highSide ? axis.x_right(mm, speed) : axis.x_left(mm, speed);
    if (a == "y") return highSide ? axis.y_up(mm, speed) : axis.y_down(mm, speed);
    if (a == "z") return highSide ? axis.z_forward(mm, speed) : axis.z_back(mm, speed);
    if (a == "yaw") return highSide ? axis.yaw_right(mm, speed) : axis.yaw_left(mm, speed);
    if (a == "pitch") return highSide ? axis.pitch_up(mm, speed) : axis.pitch_down(mm, speed);
    return {};
}

constexpr int kMonitorIntervalMs = 20;

// Bounds for the /axis/watch rate.
//
// The LOW end has to be genuinely low. A client asking for one sweep every ten
// seconds means it, and the previous floor of 0.5 Hz turned that request into
// five sweeps in the same ten seconds -- on a link where one sweep of five axes
// is 25 blocking round trips at 100 ms (Arduino) / 300 ms (CRUX) reply timeouts.
//
// The HIGH end is what the serial link can actually carry. Above this a sweep
// takes longer than its own period no matter what, so the rate is a fiction and
// the only real effect is that motion commands queue behind sensor reads.
constexpr double kWatchHzMin = 0.01;  // one sweep every 100 s
constexpr double kWatchHzMax = 5.0;   // the historical default, and about the ceiling

}  // namespace

AxisNode::AxisNode(ServerContext &ctx, const rclcpp::NodeOptions &options)
    // MutuallyExclusive: the services used to run on the node's DEFAULT group,
    // which is mutually exclusive, and this keeps that. jobGroup_ below is the
    // reentrant one.
    : ServerNode("moil_axis", ctx, options, rclcpp::CallbackGroupType::MutuallyExclusive) {
    // Reentrant, so a Stop can be served while a drive-to-limit is running. The
    // serialisation that actually protects the serial port is inside AxisDevice,
    // which funnels every call onto the one thread that owns the ports -- so a
    // Stop arriving mid-sweep queues behind at most one command, not behind the
    // whole drive.
    jobGroup_ = create_callback_group(rclcpp::CallbackGroupType::Reentrant);

    addService<AxisMove>("/axis/move", &AxisNode::onMove);
    addService<AxisCommand>("/axis/command", &AxisNode::onCommand);
    addService<AxisSensor>("/axis/sensor", &AxisNode::onSensor);
    addService<AxisPosition>("/axis/position", &AxisNode::onPosition);
    addService<AxisZero>("/axis/zero", &AxisNode::onZero);
    addService<RigInfo>("/rig/info", &AxisNode::onRigInfo);
    addService<AxisWatch>("/axis/watch", &AxisNode::onWatch);

    // The lamps and coordinates, as a stream. RELIABLE and KEEP_LAST(1): a client
    // must not miss the sample that says a limit switch tripped, and it has no use
    // for the one before the newest.
    pubState_ = create_publisher<moil_interfaces::msg::AxisState>(
        "/axis/state", rclcpp::QoS(rclcpp::KeepLast(1)).reliable());

    // Sweeps run here, not on jobGroup_ -- see the member declaration for why
    // overlapping sweeps are the failure this prevents.
    watchGroup_ = create_callback_group(rclcpp::CallbackGroupType::MutuallyExclusive);

    // The poll timer runs always; it does nothing while no axis is watched, which
    // is the resting state -- a tick over an empty list is a lock and a branch.
    //
    // It IS rebuilt when /axis/watch asks for a different rate. It used to be
    // built once here and never again, on the reasoning that rebuilding was more
    // moving parts for no gain, and that was the bug: onWatch recorded the rate
    // it was asked for, reported it back, logged it -- and kept polling at this
    // parameter's rate. A client asking for 0.1 Hz got 5 Hz, fifty times the
    // serial traffic it requested, and the log said 0.1 Hz while it happened.
    applyWatchTimer(declare_parameter<double>("axis_watch_hz", 5.0));

    addDetachedAction<LimitMove>("/axis/limit_move", &AxisNode::executeLimitMove, jobGroup_);

    addDetachedAction<Home>("/axis/home", &AxisNode::executeHome, jobGroup_);

    // The sensors topic is opt-in, exactly as AxisSensors.msg says: it polls all
    // 20 sensors, which is 20 blocking serial round trips, on ports the motion
    // commands share. Declared as a parameter rather than hardcoded off so a rig
    // being diagnosed can turn it on without a rebuild.
    const double hz = declare_parameter<double>("sensors_topic_hz", 0.0);
    if (hz > 0.0) {
        pubSensors_ = create_publisher<moil_interfaces::msg::AxisSensors>("/axis/sensors", 10);
        sensorTimer_ = create_wall_timer(
            std::chrono::milliseconds(static_cast<int>(1000.0 / hz)),
            [this] { publishSensors(); }, jobGroup_);
        RCLCPP_INFO(get_logger(), "/axis/sensors publishing at %.1f Hz", hz);
    }

    // Reopen a port that would not open, on the same 2 s period the camera has
    // always used. Existing behaviour was that AxisDevice opens once, in its
    // constructor, and a port held by something else left the axis FAILED until
    // /axis/command reconnect or a server restart.
    //
    // Two things keep this safe to run on a timer:
    //   * it only ever fires while isOpen() is false, so a working rig is never
    //     touched -- and isOpen() is false only when a port is genuinely not
    //     open, which is not a state any motion can be happening in;
    //   * reconnect() goes through AxisDevice::sync(), which funnels onto the one
    //     thread that owns the ports, so it queues behind an in-flight command
    //     rather than closing a port underneath one.
    //
    // It does what the manual reconnect does -- close all the ports, open them
    // all -- so a rig with some ports open and one busy gets all of them cycled.
    // That is the documented recovery already, and a partially open rig is not
    // drivable anyway.
    reopenSec_ = declare_parameter<double>("axis_reopen_sec", 2.0);
    if (reopenSec_ > 0.0) {
        reopenTimer_ = create_wall_timer(
            std::chrono::milliseconds(static_cast<int>(reopenSec_ * 1000.0)),
            [this] { retryReopen(); }, jobGroup_);
    }

    RCLCPP_INFO(get_logger(), "axis: %s", qPrintable(ctx_.axisUrl()));
}

void AxisNode::retryReopen() {
    if (!ctx_.axis || ctx_.axis->isOpen()) {
        // Recovered since the last tick: say so once, then go quiet. Only worth
        // printing if it had actually failed.
        if (reopenAttempts_ > 0) {
            RCLCPP_INFO(get_logger(), "axis: ports reopened after %d attempt(s)", reopenAttempts_);
            reopenAttempts_ = 0;
        }
        return;
    }

    const QString msg = ctx_.axis->reconnect();
    if (ctx_.axis->isOpen()) {
        RCLCPP_INFO(get_logger(), "axis: ports reopened after %d attempt(s)", reopenAttempts_ + 1);
        reopenAttempts_ = 0;
        return;
    }

    // Rate-limited exactly as the camera's loop is: the first failure, then one
    // line a minute. A port held by another process stays held for as long as
    // that process lives, and a line every 2 s buries everything else in the
    // status window.
    if (reopenAttempts_ % 30 == 0)
        RCLCPP_WARN(get_logger(), "axis: cannot open ports (%s); retrying every %.0f s",
                    qPrintable(msg), reopenSec_);
    ++reopenAttempts_;
}

int AxisNode::triState(const QString &endpoint) const {
    const QString v = ctx_.axis->sensor(endpoint).trimmed().toLower();
    if (v == "true" || v == "1" || v == "yes" || v == "on") return 1;
    if (v == "false" || v == "0" || v == "no" || v == "off" || v == "none") return 0;
    return -1;  // empty / unreachable / unrecognised -- NOT "clear"
}

QString AxisNode::coordinateOf(const QString &axis, QString *raw) const {
    const QString a = axis.trimmed().toLower();
    const QString text = ctx_.axis->read_position(a);
    bool ok = false;
    const double v = text.trimmed().toDouble(&ok);
    if (!ok) {
        // A failed read comes back as an error string, not a number. Empty is the
        // caller's signal to keep showing the last good value rather than jump.
        if (raw) raw->clear();
        return {};
    }
    if (raw) *raw = text.trimmed();
    // (counter - zero) * units. Without a zero this is the raw counter scaled,
    // which is an axis that has not been homed this run -- see AxisOrigin.
    return QString::asprintf("%.3f",
                             (v - ctx_.origins.zeroFor(a)) * ctx_.axis->unitsPerCount(a));
}

// ---------------------------------------------------------------- services ----

void AxisNode::onMove(const AxisMove::Request &req, AxisMove::Response &res) {
    const QString dir = QString::fromStdString(req.direction);
    const QString speed = QString::fromStdString(req.speed);
    const double d = req.distance;
    AxisDevice &ax = *ctx_.axis;

    QString out;
    if (dir == "x_left") out = ax.x_left(d, speed);
    else if (dir == "x_right") out = ax.x_right(d, speed);
    else if (dir == "y_up") out = ax.y_up(d, speed);
    else if (dir == "y_down") out = ax.y_down(d, speed);
    else if (dir == "z_forward") out = ax.z_forward(d, speed);
    else if (dir == "z_back") out = ax.z_back(d, speed);
    else if (dir == "yaw_left") out = ax.yaw_left(d, speed);
    else if (dir == "yaw_right") out = ax.yaw_right(d, speed);
    else if (dir == "pitch_up") out = ax.pitch_up(d, speed);
    else if (dir == "pitch_down") out = ax.pitch_down(d, speed);
    else {
        res.success = false;
        res.message = "unknown direction: " + req.direction;
        return;
    }
    // The device returns the reply text the service used to carry. Note the known
    // limitation from the rig log: RPS answers on COMPLETION, which is later than
    // the read window, so a move's reply is usually empty. That means an accepted
    // move and a rejected one look the same here -- which is why the drive loops
    // below watch the counter rather than trusting this string.
    res.success = ctx_.axis->isOpen();
    res.message = out.toStdString();
}

void AxisNode::onCommand(const AxisCommand::Request &req, AxisCommand::Response &res) {
    const QString cmd = QString::fromStdString(req.command).trimmed().toLower();
    const QString axis = QString::fromStdString(req.axis).trimmed().toLower();
    if (cmd == "home") {
        res.message = ctx_.axis->home(axis).toStdString();
        res.success = ctx_.axis->isOpen();
    } else if (cmd == "stop") {
        res.message = ctx_.axis->stop(axis).toStdString();
        res.success = ctx_.axis->isOpen();
    } else if (cmd == "reconnect") {
        // A reconnect is the one moment a power cycle is likely to have happened,
        // so every software zero is dropped with the ports. Keeping them would
        // silently reference coordinates to a counter that restarted.
        res.message = ctx_.axis->reconnect().toStdString();
        ctx_.origins.clear();
        ctx_.origins.save();
        res.success = ctx_.axis->isOpen();
    } else {
        res.success = false;
        res.message = "unknown command: " + req.command;
    }
}

void AxisNode::onSensor(const AxisSensor::Request &req, AxisSensor::Response &res) {
    const QString name = QString::fromStdString(req.name);
    const QString endpoint = name.startsWith("is_sensor_") ? name : "is_sensor_" + name;
    const int tri = triState(endpoint);
    // A serial error makes the device answer "", which HTTP delivered as null and
    // the old client treated as falsy. Here it becomes value=false with
    // success=false, so the caller can tell "clear" from "could not read".
    res.success = tri >= 0;
    res.value = tri == 1;
    res.message = tri < 0 ? "sensor read failed" : "";
}

void AxisNode::onPosition(const AxisPosition::Request &req, AxisPosition::Response &res) {
    const QString a = QString::fromStdString(req.axis).trimmed().toLower();
    QString raw;
    const QString coord = coordinateOf(a, &raw);
    res.success = !coord.isEmpty();
    res.raw = raw.toStdString();
    res.coordinate = coord.toStdString();
    res.has_zero = ctx_.origins.hasZero(a);
    res.zero_at = 0;
    if (res.has_zero) {
        const QDateTime t =
            QDateTime::fromString(ctx_.origins.zeroTakenAt.value(a), Qt::ISODate);
        if (t.isValid()) res.zero_at = t.toSecsSinceEpoch();
    }
    res.message = coord.isEmpty() ? "position read failed" : "";
}

void AxisNode::onZero(const AxisZero::Request &req, AxisZero::Response &res) {
    const QString a = QString::fromStdString(req.axis).trimmed().toLower();
    const QString action = QString::fromStdString(req.action).trimmed().toLower();

    if (action == "write_hardware") {
        // Gated inside AxisDevice by DeviceConfig::enableWritePosition: with the
        // flag off this sends nothing to the controller and answers with why.
        res.message = ctx_.axis->write_position(a, req.position).toStdString();
        res.success = true;
        res.coordinate = coordinateOf(a, nullptr).toStdString();
        return;
    }

    if (action == "clear") {
        ctx_.origins.zeroCount.remove(a);
        ctx_.origins.zeroTakenAt.remove(a);
        ctx_.origins.save();
        res.success = true;
        res.coordinate = coordinateOf(a, nullptr).toStdString();
        res.message = "zero cleared";
        return;
    }

    const QString text = ctx_.axis->read_position(a);
    bool ok = false;
    const double v = text.trimmed().toDouble(&ok);
    if (!ok) {
        // Homed, but the counter could not be read: keep the old zero rather than
        // replace a usable one with a guess.
        res.success = false;
        res.message = "counter did not read -- zero unchanged";
        return;
    }
    ctx_.origins.setZero(a, v);
    ctx_.origins.lastPos[a] = 0.0;
    ctx_.origins.save();
    res.success = true;
    res.coordinate = "0.000";
    res.message = "";
}

void AxisNode::onRigInfo(const RigInfo::Request &, RigInfo::Response &res) {
    res.success = true;
    res.axis_url = ctx_.axisUrl().toStdString();
    res.axis_open = ctx_.axis && ctx_.axis->isOpen();
    res.axis_error = ctx_.axis ? ctx_.axis->lastError().toStdString() : "";
    res.camera_url = ctx_.cameraUrl().toStdString();
    res.camera_open = ctx_.camera && ctx_.camera->isOpen();
    res.monitor_url = ctx_.monitorUrl().toStdString();
    res.screen_count = ctx_.monitor ? static_cast<int>(ctx_.monitor->describeScreens().size()) : 0;
    res.axis_system = ctx_.config.axisSystem.toStdString();
    const char *axes[5] = {"x", "y", "z", "yaw", "pitch"};
    for (int i = 0; i < 5; ++i)
        res.units_per_count[i] = ctx_.axis->unitsPerCount(QString::fromLatin1(axes[i]));
    res.message = "";
}

void AxisNode::publishSensors() {
    moil_interfaces::msg::AxisSensors m;
    m.header.stamp = now();
    const auto rd = [this](const char *n) { return triState(QString::fromLatin1(n)) == 1; };
    m.x_left = rd("is_sensor_x_left");   m.x_org = rd("is_sensor_x_org");
    m.x_right = rd("is_sensor_x_right"); m.x_move = rd("is_sensor_x_move");
    m.y_down = rd("is_sensor_y_down");   m.y_org = rd("is_sensor_y_org");
    m.y_up = rd("is_sensor_y_up");       m.y_move = rd("is_sensor_y_move");
    m.z_back = rd("is_sensor_z_back");   m.z_org = rd("is_sensor_z_org");
    m.z_forward = rd("is_sensor_z_forward"); m.z_move = rd("is_sensor_z_move");
    m.yaw_left = rd("is_sensor_yaw_left"); m.yaw_org = rd("is_sensor_yaw_org");
    m.yaw_right = rd("is_sensor_yaw_right"); m.yaw_move = rd("is_sensor_yaw_move");
    m.pitch_down = rd("is_sensor_pitch_down"); m.pitch_org = rd("is_sensor_pitch_org");
    m.pitch_up = rd("is_sensor_pitch_up"); m.pitch_move = rd("is_sensor_pitch_move");
    pubSensors_->publish(m);
}

// Build, or rebuild, the poll timer for `hz`. This is the ONLY place watchHz_ is
// assigned: a rate that is not in a timer is not a rate, it is a number in a
// field, and that distinction is exactly what was broken here.
std::chrono::milliseconds AxisNode::applyWatchTimer(double hz) {
    watchHz_ = std::clamp(hz, kWatchHzMin, kWatchHzMax);
    const auto period = std::chrono::milliseconds(
        std::max<long long>(1, std::llround(1000.0 / watchHz_)));

    // The watching client re-registers periodically with the same rate, so most
    // calls land here asking for what is already running.
    if (stateTimer_ && period == watchPeriod_) return period;

    // Safe while a sweep is in flight: the executor holds its own reference to
    // the timer for the duration of the callback.
    if (stateTimer_) stateTimer_->cancel();
    stateTimer_ = create_wall_timer(period, [this] { publishWatchedStates(); }, watchGroup_);
    watchPeriod_ = period;
    return period;
}

void AxisNode::onWatch(const AxisWatch::Request &req, AxisWatch::Response &res) {
    std::lock_guard<std::mutex> lock(watchMutex_);
    watched_.clear();
    for (const std::string &a : req.axes) {
        const QString name = QString::fromStdString(a).trimmed().toLower();
        if (!axisMap(name).valid) continue;  // unknown name: dropped, and said so below
        watched_.push_back(name.toStdString());
    }
    // hz 0 means "keep the server's rate", exactly as AxisWatch.srv says.
    const auto period = applyWatchTimer(req.hz > 0.0 ? req.hz : watchHz_);

    res.success = watched_.size() == req.axes.size();
    res.watching = watched_;
    // The rate IN FORCE after clamping, not the one asked for. A client that
    // asks for more than the serial link can carry has to be told what it is
    // actually getting -- it may be deciding what to display from this.
    res.hz = watchHz_;
    res.message = res.success ? "" : "one or more axis names were not recognised";

    // Log the period the timer really has. The old line printed the requested
    // rate, so a rig polling flat out at 5 Hz reported "0.1 Hz" in its own log.
    RCLCPP_INFO(get_logger(), "/axis/watch: %zu axis/axes at %.2f Hz (every %lld ms)",
                watched_.size(), watchHz_, static_cast<long long>(period.count()));
}

void AxisNode::publishWatchedStates() {
    std::vector<std::string> axes;
    {
        std::lock_guard<std::mutex> lock(watchMutex_);
        axes = watched_;
    }
    // Nobody is looking: no serial traffic. This is the resting state, and it is
    // what makes the stream safe to leave enabled.
    if (axes.empty()) return;

    for (const std::string &axisStd : axes) {
        const QString a = QString::fromStdString(axisStd);
        const AxisMap m = axisMap(a);
        if (!m.valid) continue;

        moil_interfaces::msg::AxisState msg;
        msg.header.stamp = now();
        msg.axis = axisStd;
        // Five blocking serial round trips, right here. This is why the watch list
        // is opt-in and why the rate is low: these queue against the motion
        // commands on the same two ports.
        msg.sensor_low = static_cast<int8_t>(triState(QString::fromLatin1(m.s1)));
        msg.sensor_org = static_cast<int8_t>(triState(QString::fromLatin1(m.s2)));
        msg.sensor_high = static_cast<int8_t>(triState(QString::fromLatin1(m.s3)));
        msg.sensor_moving = static_cast<int8_t>(triState(QString::fromLatin1(m.mv)));

        QString raw;
        const QString coord = coordinateOf(a, &raw);
        msg.raw = raw.toStdString();
        msg.coordinate = coord.toStdString();
        msg.has_zero = ctx_.origins.hasZero(a);

        pubState_->publish(msg);
    }
}

// ------------------------------------------------------------ drive to end ----

// Ported from ControllerMain::runLimitMove, unchanged in substance. What was a
// QtConcurrent worker posting to the GUI thread is now an action execution
// posting feedback; the loop, the constants and the reasons they have those
// values are the same, because they were measured on this rig.
void AxisNode::executeLimitMove(
    const std::shared_ptr<rclcpp_action::ServerGoalHandle<LimitMove>> gh) {
    const auto goal = gh->get_goal();
    const QString a = QString::fromStdString(goal->axis).trimmed().toLower();
    const bool highSide = goal->high_side;
    const QString speed = QString::fromStdString(goal->speed);
    auto result = std::make_shared<LimitMove::Result>();

    const AxisMap m = axisMap(a);
    if (!m.valid) {
        result->success = false;
        result->message = "unknown axis: " + goal->axis;
        gh->abort(result);
        return;
    }

    AxisDevice &ax = *ctx_.axis;
    const double travel = ax.travelToLimit(a);
    const qint64 kTimeoutMs = 180000;
    // A move is only believed to have ended early once it has had time to get
    // going: for the first reads the command is still crossing the serial link
    // and the axis is legitimately idle.
    const qint64 kStartGraceMs = 3000;
    const int kShortIdleNeeded = 10;
    // Enough to cross any axis here in whole-travel commands (Z needs two), with
    // room to spare; low enough that a switch which never reports cannot keep the
    // stage moving indefinitely. A leg that does not advance the counter ends the
    // drive before this is reached.
    const int kMaxLegs = 6;

    double lastRaw = 0.0;
    bool haveRaw = false;

    // One sensor sweep. NOT a cheap call: five blocking serial round trips -- four
    // sensors and a position -- each waiting up to the configured reply timeout.
    // The sleep at the bottom of the loop is 20 ms but the sweep dominates, so the
    // real interval between two checks of the end sensor is nearer 100-500 ms.
    // That is the window the stage keeps moving in after it reaches the switch,
    // before this notices, and it is why the firmware limit stop -- not this -- is
    // the real protection.
    const auto poll = [&](bool *targetHit) {
        const int s1 = triState(QString::fromLatin1(m.s1));
        const int s3 = triState(QString::fromLatin1(m.s3));
        const int mv = triState(QString::fromLatin1(m.mv));
        QString raw;
        const QString coord = coordinateOf(a, &raw);
        if (!raw.isEmpty()) {
            bool ok = false;
            const double v = raw.toDouble(&ok);
            if (ok) { lastRaw = v; haveRaw = true; }
        }
        *targetHit = (highSide ? s3 : s1) == 1;

        auto fb = std::make_shared<LimitMove::Feedback>();
        fb->sensor = highSide ? s3 : s1;
        fb->coordinate = coord.toStdString();
        fb->stage = *targetHit ? "on the switch" : "driving";
        gh->publish_feedback(fb);
        return mv;
    };

    RCLCPP_INFO(get_logger(), "[axis] %s -> %s end START: commanding %.1f, watching %s",
                qPrintable(a), highSide ? "high" : "low", travel, highSide ? m.s3 : m.s1);

    bool hit = false;
    poll(&hit);
    bool reached = hit;  // already sitting on it: nothing to command
    bool cancelled = false;
    const char *why = reached ? "already on the switch" : "";

    QElapsedTimer clock;
    clock.start();

    // Repeat the move until the switch trips. One command cannot necessarily
    // cross an axis: the protocol caps a linear move at 195000 steps (487.5 mm on
    // Z, 195 mm on X/Y) and Z's measured travel is ~212000 counts, so a single leg
    // stops the stage mid-rail having done exactly what it was asked -- which is
    // indistinguishable, to anyone watching, from the axis having arrived.
    //
    // A leg that ends without moving the counter ends the drive: the axis is
    // against something the switch is not reporting, and re-commanding into it is
    // the one thing that must not happen near the panels.
    for (int leg = 1; leg <= kMaxLegs && !reached && !cancelled; ++leg) {
        const double startRaw = lastRaw;
        const bool hadRaw = haveRaw;
        driveOneWay(ax, a, highSide, travel, speed);

        int idle = 0;
        bool everMoved = false, legDone = false;
        while (!legDone) {
            const int mv = poll(&hit);
            if (hit) { ax.stop(a); reached = true; why = "end switch reached"; break; }
            if (gh->is_canceling()) { ax.stop(a); cancelled = true; why = "cancelled"; break; }
            if (clock.elapsed() > kTimeoutMs) { ax.stop(a); why = "timed out"; break; }

            if (mv == 1) everMoved = true;
            idle = (mv == 0) ? idle + 1 : 0;
            if (idle >= kShortIdleNeeded && (everMoved || clock.elapsed() > kStartGraceMs))
                legDone = true;  // this leg is spent; the drive may continue
            else
                QThread::msleep(kMonitorIntervalMs);
        }
        if (reached || cancelled || !legDone) break;

        const double moved = (hadRaw && haveRaw) ? std::abs(lastRaw - startRaw) : 0.0;
        RCLCPP_INFO(get_logger(), "[axis] %s leg %d of at most %d done, counter moved %.0f",
                    qPrintable(a), leg, kMaxLegs, moved);
        if (!hadRaw || !haveRaw || moved < 1.0) {
            why = everMoved ? "axis stopped and the counter is not advancing"
                            : "axis never started moving";
            break;
        }
        if (leg == kMaxLegs) why = "gave up after the maximum number of legs";
    }

    // Always logged, not only on failure: "why did it stop there" is the first
    // question asked of a drive that ended somewhere unexpected.
    RCLCPP_INFO(get_logger(), "[axis] %s -> %s end ended after %lld ms: %s", qPrintable(a),
                highSide ? "high" : "low", static_cast<long long>(clock.elapsed()), why);

    result->success = true;
    result->reached_sensor = reached;
    result->cancelled = cancelled;
    result->coordinate = coordinateOf(a, nullptr).toStdString();
    result->message = why;
    if (cancelled) gh->canceled(result);
    else gh->succeed(result);
}

// ------------------------------------------------------------ blocking home ----

// Ports ControllerMain::waitAxisHomeBlocking plus the loop in allHome that drove
// it. The homing itself is one ORG per axis; the substance is the waiting.
void AxisNode::executeHome(const std::shared_ptr<rclcpp_action::ServerGoalHandle<Home>> gh) {
    const auto goal = gh->get_goal();
    auto result = std::make_shared<Home::Result>();
    const int idleNeeded = goal->idle_needed > 0 ? goal->idle_needed : 3;
    const double timeoutS = goal->timeout > 0.0 ? goal->timeout : 120.0;
    bool cancelled = false;

    for (const std::string &axisStd : goal->axes) {
        if (gh->is_canceling()) { cancelled = true; break; }
        const QString a = QString::fromStdString(axisStd).trimmed().toLower();
        const AxisMap m = axisMap(a);
        if (!m.valid) continue;

        ctx_.axis->home(a);

        QElapsedTimer clock;
        clock.start();
        int orgOk = 0, movOk = 0;
        bool arrived = false;
        while (true) {
            const int s2 = triState(QString::fromLatin1(m.s2));
            const int mv = triState(QString::fromLatin1(m.mv));

            auto fb = std::make_shared<Home::Feedback>();
            fb->axis = axisStd;
            fb->sensor_org = s2;
            fb->sensor_move = mv;
            fb->elapsed = clock.elapsed() / 1000.0;
            fb->stage = "homing";
            gh->publish_feedback(fb);

            orgOk = (s2 == 1) ? orgOk + 1 : 0;
            movOk = (mv != 1) ? movOk + 1 : 0;  // unreadable and false both count as "not moving"
            if (orgOk >= idleNeeded && movOk >= idleNeeded) { arrived = true; break; }
            if (gh->is_canceling()) { ctx_.axis->stop(a); cancelled = true; break; }
            if (clock.elapsed() > timeoutS * 1000) break;
            QThread::msleep(100);
        }

        if (cancelled) break;
        if (arrived) {
            result->homed.push_back(axisStd);
            // The ONLY moment it is safe to capture a zero automatically: the axis
            // is standing on its origin sensor by definition of having arrived.
            if (goal->capture_zero) {
                const QString text = ctx_.axis->read_position(a);
                bool ok = false;
                const double v = text.trimmed().toDouble(&ok);
                if (ok) {
                    ctx_.origins.setZero(a, v);
                    ctx_.origins.lastPos[a] = 0.0;
                } else {
                    RCLCPP_WARN(get_logger(),
                                "[positions] %s homed but its counter did not read -- "
                                "zero unchanged", qPrintable(a));
                }
            }
        } else {
            result->timed_out.push_back(axisStd);
        }
    }

    if (goal->capture_zero) ctx_.origins.save();
    result->success = result->timed_out.empty() && !cancelled;
    result->cancelled = cancelled;
    result->message = cancelled ? "cancelled" : (result->timed_out.empty() ? "" : "some axes timed out");
    if (cancelled) gh->canceled(result);
    else gh->succeed(result);
}
