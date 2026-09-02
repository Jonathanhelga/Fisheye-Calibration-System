#pragma once

#include <memory>

#include <QString>

#include "server_node.h"

#include "moil_interfaces/action/auto_calibrate.hpp"

struct ServerContext;

// /moil_jobs -- the closed-loop calibration run.
//
// In v2.0 this was a chain of QTimer::singleShot calls in the main window that
// clicked its own buttons: Positive Shot five times, Negative Shot five times,
// Update Table, Save, Z back 20 mm, next round. Driving the rig by synthesising
// clicks worked, and it tied the run to the window being open, on a machine that
// might not be the rig's.
//
// Rebuilt here as a straight sequential procedure on its own thread. Two things
// change for the better and both are consequences of where it now runs:
//
//   * closing the client no longer abandons a moving axis. The loop is on the
//     same machine as the stage; a client that disappears loses the feedback, not
//     the run, and reconnecting picks it back up.
//
//   * the ten shots per round no longer cross the network. Each one was a
//     3040x3040 frame that went to the client to be analysed and came back as a
//     table; now the capture, the eight-direction detection and the table update
//     all happen without the frame leaving this process.
//
// What has NOT changed is the sequence, the intervals or the reasoning behind
// them -- the chessboard step before the rounds because every pattern shot paints
// over all five screens, the X excursion undone on abort while the Z moves are
// left alone, and noise cleaning deliberately not forced on. Those were decided
// against this hardware and are ported as they are.
class JobsNode : public ServerNode {
public:
    JobsNode(ServerContext &ctx, const rclcpp::NodeOptions &options = rclcpp::NodeOptions());

private:
    using AutoCalibrate = moil_interfaces::action::AutoCalibrate;
    using GoalHandle = rclcpp_action::ServerGoalHandle<AutoCalibrate>;

    void executeAutoCalibrate(const std::shared_ptr<GoalHandle> gh);

    // Put one pattern on the glass: top gets its spec, the four sides get theirs.
    // Returns false when a panel refused, which stops the run -- a round shot
    // against a screen still showing the previous pattern is not a bad
    // measurement, it is a measurement of the wrong thing.
    bool pushRoundPatterns(const QString &topSpec, const QString &sideSpec);

    // Capture into a file under `dir`, and give the caller the bytes for the
    // preview and for the analysis. Empty on failure.
    QByteArray captureTo(const QString &path);

    // Wait for an axis to stop, then settle. Ports autoCalibWaitAxis/WaitZ: the
    // settle exists so the rig is not still ringing when the shutter opens.
    bool waitAxisIdle(const QString &axis, int maxMs, int settleMs,
                      const std::shared_ptr<GoalHandle> &gh);
};
