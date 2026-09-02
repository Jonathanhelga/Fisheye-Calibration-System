// MoilCali server 2.1.0 -- the rig, and everything that computes.
//
// One process, several ROS nodes. The nodes are separate so `ros2 node list`
// still shows /moil_axis, /moil_camera and /moil_monitor where V2.0.0 put them,
// and so a client can be written against one subsystem. The PROCESS is one
// because the devices are single-instance -- the axis holds both COM ports -- and
// because the job orchestration drives the axis, the camera, the compute pipeline
// and the session in one loop tight enough to stop a stage on a sensor reading.
// Split across processes, that loop would run over DDS: the server talking to
// itself, with the panels' safety backstop depending on the round trip.
//
// Threading. Two event loops, and which is which matters:
//
//   the Qt GUI thread (this one) owns the pattern windows. The five calibration
//   screens are wired to THIS machine and the patterns are Qt windows on its
//   desktop, which is why the server is a QApplication rather than a console
//   program.
//
//   the ROS executor runs on its own thread, multi-threaded, so a Stop can be
//   served while a drive-to-limit is running.
//
// MonitorDevice marshals every call to the GUI thread and waits, so monitor
// services may be called from any executor thread. What must never come back is
// a call that blocks the GUI thread while the GUI thread waits for it: that
// deadlock is what monitorconcurrency_test in the v2.0 tree exists to catch, and
// the shape it catches is still present here.

#include <csignal>
#include <cstdio>
#include <memory>
#include <thread>

#include <QApplication>
#include <QCommandLineParser>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QTimer>

#include <rclcpp/rclcpp.hpp>

#include "axis_node.h"
#include "camera_node.h"
#include "compute_node.h"
#include "jobs_node.h"
#include "measure3d_node.h"
#include "monitor_node.h"
#include "server_context.h"
#include "session_node.h"
#include "supervisor_node.h"

namespace {

// The rig's launchers deliberately blank these. A leftover discovery-server or
// Fast DDS profile setting makes discovery silently find nothing -- the client
// instructions record a whole day lost to exactly that -- so the server refuses
// to inherit them rather than starting in a mode nobody chose.
void clearDiscoveryOverrides() {
    for (const char *v : {"ROS_DISCOVERY_SERVER", "FASTDDS_DEFAULT_PROFILES_FILE",
                          "FASTRTPS_DEFAULT_PROFILES_FILE", "ROS_LOCALHOST_ONLY"}) {
        if (qEnvironmentVariableIsSet(v)) {
            std::fprintf(stderr,
                         "[server] %s was set in the environment and has been cleared: this rig "
                         "runs in plain LAN multicast mode\n",
                         v);
            qunsetenv(v);
        }
    }
}

}  // namespace

int main(int argc, char **argv) {
    clearDiscoveryOverrides();

    // Fixed by the rig and matched by every client. Set before rclcpp::init, and
    // set here rather than relied on from the shell, so double-clicking a
    // launcher and running from a prompt land on the same domain.
    if (!qEnvironmentVariableIsSet("ROS_DOMAIN_ID")) qputenv("ROS_DOMAIN_ID", "42");

    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("MoilLab"));
    QApplication::setApplicationName(QStringLiteral("FisheyeCalisys"));
    QApplication::setApplicationVersion(QStringLiteral("2.1.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("MoilCali server: rig control, capture, calibration maths and 3D "
                       "verification, served over ROS 2."));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption checkOpt(
        QStringLiteral("check"),
        QStringLiteral("Open every device, run the calculation self-test, print the report and "
                       "exit. Exit code 0 when everything is healthy."));
    const QCommandLineOption noPreviewOpt(
        QStringLiteral("no-preview"),
        QStringLiteral("Do not publish the camera preview topic. Use when the link is tight: the "
                       "preview is the only thing here that can starve the control path."));
    parser.addOption(checkOpt);
    parser.addOption(noPreviewOpt);
    parser.process(app);

    ServerContext ctx;
    ctx.start();

    if (parser.isSet(checkOpt)) {
        double seconds = 0.0;
        const auto suites = ComputeSelfTest::run(QStringLiteral("all"), &seconds);
        bool passed = !suites.isEmpty();
        std::printf("\naxis     %s\ncamera   %s\nmonitor  %s\n",
                    qPrintable(ctx.axisUrl()), qPrintable(ctx.cameraUrl()),
                    qPrintable(ctx.monitorUrl()));
        for (const auto &s : suites) {
            std::printf("%-8s %s  (max drift %g)\n", qPrintable(s.name),
                        s.passed ? "OK" : "FAILED", s.maxDeviation);
            for (const QString &n : s.notes) std::printf("         %s\n", qPrintable(n));
            if (!s.passed) passed = false;
        }
        std::printf("\nself-test %s in %.2fs\n", passed ? "PASSED" : "FAILED", seconds);
        const bool healthy = passed && ctx.axis->isOpen();
        std::printf("SERVER CHECK %s\n", healthy ? "OK" : "FAILED");
        return healthy ? 0 : 1;
    }

    rclcpp::init(argc, argv);

    rclcpp::NodeOptions opts;
    if (parser.isSet(noPreviewOpt))
        opts.parameter_overrides({rclcpp::Parameter("preview_hz", 0.0)});

    auto axisNode = std::make_shared<AxisNode>(ctx);
    auto cameraNode = std::make_shared<CameraNode>(ctx, opts);
    auto monitorNode = std::make_shared<MonitorNode>(ctx);
    auto computeNode = std::make_shared<ComputeNode>(ctx);
    auto sessionNode = std::make_shared<SessionNode>(ctx);
    auto measure3dNode = std::make_shared<Measure3dNode>(ctx);
    auto jobsNode = std::make_shared<JobsNode>(ctx);
    auto supervisorNode = std::make_shared<SupervisorNode>(ctx);

    // Multi-threaded on purpose. A single-threaded executor would put a Stop
    // behind whatever long call is in front of it, and the one command that must
    // never queue behind a drive-to-limit is the one that ends it.
    auto executor = std::make_shared<rclcpp::executors::MultiThreadedExecutor>();
    executor->add_node(axisNode);
    executor->add_node(cameraNode);
    executor->add_node(monitorNode);
    executor->add_node(computeNode);
    executor->add_node(sessionNode);
    executor->add_node(measure3dNode);
    executor->add_node(jobsNode);
    executor->add_node(supervisorNode);

    std::thread spinner([executor] { executor->spin(); });

    std::printf(
        "\nMoilCali server 2.1.0 listening on ROS_DOMAIN_ID=%s\n"
        "Nodes: /moil_axis /moil_camera /moil_monitor /moil_compute /moil_session "
        "/moil_measure3d /moil_jobs /moil_supervisor\n"
        "Ctrl+C to stop. Closing this window force-kills the process, which skips the serial\n"
        "port close -- the next start then reads sensor values that are garbage but plausible.\n\n",
        qgetenv("ROS_DOMAIN_ID").constData());
    std::fflush(stdout);

    // Ctrl+C has to reach the Qt loop, not only rclcpp: the pattern windows and
    // the serial ports are Qt-side, and a shutdown that stops spinning without
    // unwinding those is the force-kill the banner warns about.
    std::signal(SIGINT, [](int) { QCoreApplication::quit(); });

    const int rc = app.exec();

    executor->cancel();
    if (spinner.joinable()) spinner.join();
    rclcpp::shutdown();
    return rc;
}
