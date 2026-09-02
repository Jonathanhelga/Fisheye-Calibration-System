#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <QString>

#include "server_node.h"

#include "moil_interfaces/action/axis_home.hpp"
#include "moil_interfaces/action/axis_limit_move.hpp"
#include "moil_interfaces/msg/axis_sensors.hpp"
#include "moil_interfaces/msg/axis_state.hpp"
#include "moil_interfaces/srv/axis_command.hpp"
#include "moil_interfaces/srv/axis_move.hpp"
#include "moil_interfaces/srv/axis_position.hpp"
#include "moil_interfaces/srv/axis_sensor.hpp"
#include "moil_interfaces/srv/axis_watch.hpp"
#include "moil_interfaces/srv/axis_zero.hpp"
#include "moil_interfaces/srv/rig_info.hpp"

struct ServerContext;

// /moil_axis -- the motion stage.
//
// The four V2.0.0 interfaces (move, command, sensor, and the sensors topic) keep
// their exact shapes so this node answers a v2.0.0 client. What is new is
// everything the client used to do BETWEEN those calls:
//
//   * the counter-to-coordinate conversion and the software zero it needs
//     (AxisPosition / AxisZero) -- arithmetic, and it belongs with the stage
//   * drive-to-a-limit-switch (AxisLimitMove) -- a control loop whose whole job
//     is to stop an axis before it reaches a hard stop, which must not depend on
//     a network link staying up
//   * homing that WAITS (AxisHome) -- the All Home flow needs to know when
//     homing finished before it may do anything else
//
// The sensors topic stays opt-in for the reason AxisSensors.msg gives: polling
// all 20 sensors on a timer puts continuous traffic on the very serial ports the
// motion commands share, and on this rig that is exactly the interleaving the
// hardware module warns about. Clients that want lamps subscribe; clients that
// want one sensor call the service.
class AxisNode : public ServerNode {
public:
    AxisNode(ServerContext &ctx, const rclcpp::NodeOptions &options = rclcpp::NodeOptions());

private:
    using AxisMove = moil_interfaces::srv::AxisMove;
    using AxisCommand = moil_interfaces::srv::AxisCommand;
    using AxisSensor = moil_interfaces::srv::AxisSensor;
    using AxisPosition = moil_interfaces::srv::AxisPosition;
    using AxisZero = moil_interfaces::srv::AxisZero;
    using RigInfo = moil_interfaces::srv::RigInfo;
    using AxisWatch = moil_interfaces::srv::AxisWatch;
    using LimitMove = moil_interfaces::action::AxisLimitMove;
    using Home = moil_interfaces::action::AxisHome;

    void onMove(const AxisMove::Request &req, AxisMove::Response &res);
    void onCommand(const AxisCommand::Request &req, AxisCommand::Response &res);
    void onSensor(const AxisSensor::Request &req, AxisSensor::Response &res);
    void onPosition(const AxisPosition::Request &req, AxisPosition::Response &res);
    void onZero(const AxisZero::Request &req, AxisZero::Response &res);
    void onRigInfo(const RigInfo::Request &req, RigInfo::Response &res);

    void publishSensors();

    // Poll every watched axis once and publish an AxisState for each. One
    // sweep is five blocking serial round trips PER AXIS, so what is watched
    // and how often is the operator's choice, not a constant -- see
    // AxisWatch.srv.
    void publishWatchedStates();
    void onWatch(const AxisWatch::Request &req, AxisWatch::Response &res);

    // Build (or rebuild) the poll timer for `hz`, clamped, and return the period
    // it actually got. Call this instead of assigning watchHz_: the rate a client
    // asks for is only real once the TIMER carries it.
    std::chrono::milliseconds applyWatchTimer(double hz);

    void executeLimitMove(const std::shared_ptr<rclcpp_action::ServerGoalHandle<LimitMove>> gh);
    void executeHome(const std::shared_ptr<rclcpp_action::ServerGoalHandle<Home>> gh);

    // The coordinate an axis is standing on, as text, applying the software zero
    // and units_per_count. `raw` out is the unshifted counter. Both empty when
    // the read failed -- which is not the same as 0.
    QString coordinateOf(const QString &axis, QString *raw) const;

    // Sensor tri-state: 1 triggered, 0 clear, -1 unreadable. The device answers
    // "true"/"false"/"" and the third case must not collapse into false: an
    // unreadable limit switch that reads as "not triggered" is how a drive-to-end
    // keeps driving.
    int triState(const QString &endpoint) const;

    rclcpp::Publisher<moil_interfaces::msg::AxisSensors>::SharedPtr pubSensors_;
    rclcpp::TimerBase::SharedPtr sensorTimer_;
    rclcpp::Publisher<moil_interfaces::msg::AxisState>::SharedPtr pubState_;
    rclcpp::TimerBase::SharedPtr stateTimer_;

    // Its own MutuallyExclusive group, NOT jobGroup_. A sweep is 25 blocking
    // serial round trips with all five axes watched, so it can easily outlast its
    // own period -- and on the reentrant group under a MultiThreadedExecutor the
    // next tick then STARTED ANYWAY, on another thread, on top of the sweep still
    // running. Overlapping sweeps queue behind the one thread that owns the ports
    // and the backlog grows without bound: services that need the port stop
    // answering within any client's timeout. Mutually exclusive means a tick that
    // arrives mid-sweep is dropped instead, which is the correct response to
    // "the rig cannot go this fast".
    rclcpp::CallbackGroup::SharedPtr watchGroup_;

    // Which axes are being polled, and how fast. Empty means no serial traffic
    // at all, which is the resting state of a rig nobody is watching.
    //
    // watchHz_ is the rate IN FORCE (post-clamp), not the rate last requested,
    // and watchPeriod_ is what stateTimer_ was actually built with -- they are
    // reported back and logged, so both have to be the truth.
    std::mutex watchMutex_;
    std::vector<std::string> watched_;
    double watchHz_ = 0.0;
    std::chrono::milliseconds watchPeriod_{0};

    // Retry a serial port that would not open. The camera has always reopened
    // itself every 2 s and the axis never did, which mattered more once this
    // became a server: a rig that came up while something else held COM3/COM4
    // stayed FAILED until somebody noticed and called /axis/command reconnect.
    rclcpp::TimerBase::SharedPtr reopenTimer_;
    int reopenAttempts_ = 0;
    double reopenSec_ = 0.0;

    void retryReopen();

    // Callback group for the long jobs, so a drive-to-limit does not block the
    // Stop button's service call behind it in the executor.
    rclcpp::CallbackGroup::SharedPtr jobGroup_;
};
