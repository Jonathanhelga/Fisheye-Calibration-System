#pragma once

#include <memory>

#include <QString>

#include "server_node.h"

#include "moil_interfaces/action/run_compute.hpp"
#include "moil_interfaces/msg/session_event.hpp"
#include "moil_interfaces/srv/session_capture.hpp"
#include "moil_interfaces/srv/session_command.hpp"
#include "moil_interfaces/srv/session_edit.hpp"
#include "moil_interfaces/srv/session_image.hpp"
#include "moil_interfaces/srv/session_state.hpp"

struct ServerContext;

// /moil_session -- calibration sessions, on the machine that holds the rig.
//
// v2.0's merge deliberately gave this up. Its own note said so: "What is gone is
// a second client seeing the first one's work. That was the one thing only two
// processes could do, and merging them is what was asked for." This version asks
// for it back, and it comes back with the three properties the store was built
// for intact -- because they were always properties of the STORE, not of the
// transport:
//
//   * work outlives the client -- sessions are directories under sessionsDir()
//   * captures are never uploaded -- the camera is on this machine
//   * progress and Cancel work -- runCompute takes a ProgressFn, which is now
//     action feedback again instead of a direct call
//
// The version number is what keeps several clients honest. Every mutation bumps
// it and publishes SessionEvent; a client re-fetches with SessionState, which
// answers "unchanged" for the one that caused the edit and sends the table to the
// others. Nobody polls a 4 MB table to discover nothing happened.
class SessionNode : public ServerNode {
public:
    SessionNode(ServerContext &ctx, const rclcpp::NodeOptions &options = rclcpp::NodeOptions());

private:
    using Command = moil_interfaces::srv::SessionCommand;
    using Edit = moil_interfaces::srv::SessionEdit;
    using State = moil_interfaces::srv::SessionState;
    using CaptureSrv = moil_interfaces::srv::SessionCapture;
    using ImageSrv = moil_interfaces::srv::SessionImage;
    using RunCompute = moil_interfaces::action::RunCompute;

    void onCommand(const Command::Request &req, Command::Response &res);
    void onEdit(const Edit::Request &req, Edit::Response &res);
    void onState(const State::Request &req, State::Response &res);
    void onCapture(const CaptureSrv::Request &req, CaptureSrv::Response &res);
    void onImage(const ImageSrv::Request &req, ImageSrv::Response &res);
    void executeRunCompute(const std::shared_ptr<rclcpp_action::ServerGoalHandle<RunCompute>> gh);

    void publishEvent(const QString &sessionId, quint64 version, const QString &kind,
                      const QString &detail);

    rclcpp::Publisher<moil_interfaces::msg::SessionEvent>::SharedPtr pubEvent_;
};
