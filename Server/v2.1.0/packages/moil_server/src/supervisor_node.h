#pragma once

#include <memory>

#include <QString>
#include <QStringList>
#include <QtGlobal>

#include "server_node.h"

#include "moil_interfaces/msg/system_status.hpp"

struct ServerContext;

// /moil_supervisor -- is this server actually working?
//
// A ROS graph is bad at answering that question. `ros2 node list` finds nodes
// that are wedged; a service that replies proves the transport, not the answer.
// The two failures this rig can actually have are a device that stopped
// answering, and arithmetic that changed -- and neither shows up in discovery.
//
// So this node checks the things themselves, once a second, and prints one
// screen of text that says what it found. That screen IS the server's operator
// interface: run_server.bat opens a console, this fills it, and an operator can
// tell from across the room whether the rig is ready.
//
//   MoilCali server 2.1.0            up 00:41:12        clients: 1
//   -------------------------------------------------------------
//   axis      OK        yuanman: arduino=COM3 crux=COM4
//   camera    OK        id=0 mf 3040x3040 -- preview 10.0 Hz
//   monitor   DEGRADED  7 screens, 5 mapped (DDC/CI refused on DISPLAY3)
//   compute   OK        self-test passed 12 s ago, max drift 0
//   session   OK        3 sessions, none open
//   jobs      OK        idle
//
// The compute line is the one that is not obtainable any other way. It runs the
// recorded-baseline check described in SelfTest.srv, on a timer, so a build whose
// formulas moved is reported as a fault rather than discovered months later in a
// calibration that does not reproduce.
class SupervisorNode : public ServerNode {
public:
    SupervisorNode(ServerContext &ctx, const rclcpp::NodeOptions &options = rclcpp::NodeOptions());

    // What the console prints. Also written to the log, so the same text is
    // available after the fact.
    QString renderStatus() const;

private:
    enum State : uint8_t { kOk = 0, kDegraded = 1, kFailed = 2, kStarting = 3 };

    void tick();
    void runSelfTest();

    rclcpp::Publisher<moil_interfaces::msg::SystemStatus>::SharedPtr pub_;
    rclcpp::TimerBase::SharedPtr timer_;
    rclcpp::TimerBase::SharedPtr selfTestTimer_;

    bool selfTestPassed_ = false;
    bool selfTestRun_ = false;
    double selfTestMaxDeviation_ = 0.0;
    qint64 selfTestAtMs_ = 0;
    QStringList selfTestNotes_;

    // Printed only when it changes. A console that reprints an identical table
    // every second is a console nobody reads.
    QString lastPrinted_;
};
