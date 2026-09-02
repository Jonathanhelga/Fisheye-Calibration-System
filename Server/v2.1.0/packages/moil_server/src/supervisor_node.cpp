#include "supervisor_node.h"

#include <cstdio>

#include <QDateTime>
#include <QString>
#include <QStringList>

#include "compute_node.h"
#include "server_context.h"

namespace {

const char *stateName(uint8_t s) {
    switch (s) {
        case 0: return "OK";
        case 1: return "DEGRADED";
        case 2: return "FAILED";
        default: return "STARTING";
    }
}

QString hms(double seconds) {
    const int t = static_cast<int>(seconds);
    return QString::asprintf("%02d:%02d:%02d", t / 3600, (t / 60) % 60, t % 60);
}

}  // namespace

SupervisorNode::SupervisorNode(ServerContext &ctx, const rclcpp::NodeOptions &options)
    // MutuallyExclusive: tick() reads every subsystem and publishes one status
    // row, and two overlapping sweeps would publish a mixture of both.
    : ServerNode("moil_supervisor", ctx, options,
                 rclcpp::CallbackGroupType::MutuallyExclusive) {
    // TRANSIENT_LOCAL so a client that connects mid-run immediately learns the
    // state instead of showing "unknown" until the next tick.
    pub_ = create_publisher<moil_interfaces::msg::SystemStatus>(
        "/system/status", rclcpp::QoS(rclcpp::KeepLast(1)).transient_local());

    timer_ = create_wall_timer(std::chrono::seconds(1), [this] { tick(); }, group_);

    // The interval is a compromise and worth naming: the check costs real CPU
    // (it renders a pattern and runs the pipeline), and a formula does not drift
    // between two ticks -- it drifts between two BUILDS. Five minutes is often
    // enough to notice within one working session and rare enough to be free.
    const double period = declare_parameter<double>("self_test_period_s", 300.0);
    selfTestTimer_ = create_wall_timer(
        std::chrono::milliseconds(static_cast<int>(period * 1000)), [this] { runSelfTest(); },
        group_);

    // Once at start-up, so the banner the operator reads on launch has already
    // been checked rather than saying "not yet run".
    runSelfTest();
}

void SupervisorNode::runSelfTest() {
    double seconds = 0.0;
    const auto suites = ComputeSelfTest::run(QStringLiteral("all"), &seconds);

    selfTestPassed_ = !suites.isEmpty();
    selfTestMaxDeviation_ = 0.0;
    selfTestNotes_.clear();
    for (const auto &s : suites) {
        if (!s.passed) selfTestPassed_ = false;
        selfTestMaxDeviation_ = std::max(selfTestMaxDeviation_, s.maxDeviation);
        selfTestNotes_ += s.notes;
    }
    selfTestRun_ = true;
    selfTestAtMs_ = QDateTime::currentMSecsSinceEpoch();

    if (!selfTestPassed_)
        for (const QString &n : selfTestNotes_)
            RCLCPP_ERROR(get_logger(), "[self-test] %s", qPrintable(n));
    else if (!selfTestNotes_.isEmpty())
        for (const QString &n : selfTestNotes_)
            RCLCPP_INFO(get_logger(), "[self-test] %s", qPrintable(n));
}

void SupervisorNode::tick() {
    moil_interfaces::msg::SystemStatus m;
    m.header.stamp = now();
    m.uptime = (QDateTime::currentMSecsSinceEpoch() - ctx_.startedAtMs) / 1000.0;

    const auto add = [&m](const char *name, uint8_t state, const QString &detail) {
        m.subsystems.push_back(name);
        m.states.push_back(state);
        m.details.push_back(detail.toStdString());
    };

    // axis -- open is the whole question. A closed port is not degraded, it is the
    // stage being unavailable, and every motion control should be disabled.
    add("axis", ctx_.axis && ctx_.axis->isOpen() ? kOk : kFailed,
        ctx_.axis && ctx_.axis->isOpen() ? ctx_.axisUrl() : ctx_.axis->lastError());

    // camera -- the grab thread reopens on its own, so "not open right now" is
    // degraded rather than failed: it may well be back before anyone captures.
    add("camera", ctx_.camera && ctx_.camera->isOpen() ? kOk : kDegraded, ctx_.cameraUrl());

    // monitor -- an incomplete mapping is the failure mode that hides. Every
    // service call succeeds and the patterns land on arbitrary screens.
    const QString mon = ctx_.monitorUrl();
    add("monitor", mon.contains(QStringLiteral("UNSET")) ? kDegraded : kOk, mon);

    // compute -- the only line here that says the MATHS is right rather than
    // merely reachable.
    QString computeDetail;
    uint8_t computeState = kStarting;
    if (selfTestRun_) {
        const double ageS = (QDateTime::currentMSecsSinceEpoch() - selfTestAtMs_) / 1000.0;
        computeState = selfTestPassed_ ? kOk : kFailed;
        computeDetail = selfTestPassed_
                            ? QStringLiteral("self-test passed %1s ago, max drift %2")
                                  .arg(static_cast<int>(ageS))
                                  .arg(selfTestMaxDeviation_, 0, 'g', 3)
                            : QStringLiteral("SELF-TEST FAILED: %1")
                                  .arg(selfTestNotes_.value(0));
    } else {
        computeDetail = QStringLiteral("self-test has not run yet");
    }
    add("compute", computeState, computeDetail);

    QString err;
    const int sessionCount = ctx_.sessions ? ctx_.sessions->list(&err).size() : 0;
    add("session", ctx_.sessions ? kOk : kFailed,
        QStringLiteral("%1 sessions, %2")
            .arg(sessionCount)
            .arg(ctx_.sessions && ctx_.sessions->hasSession()
                     ? QStringLiteral("open: %1").arg(ctx_.sessions->sessionId())
                     : QStringLiteral("none open")));

    add("jobs", kOk, QStringLiteral("idle"));

    m.compute_selftest_passed = selfTestPassed_;
    m.compute_selftest_age =
        selfTestRun_ ? (QDateTime::currentMSecsSinceEpoch() - selfTestAtMs_) / 1000.0 : -1.0;
    m.compute_selftest_max_deviation = selfTestMaxDeviation_;

    // Who is connected. Counted from the graph rather than from a handshake of
    // our own: a client that crashed does not get to un-register itself, and the
    // graph notices when its participant goes away.
    for (const std::string &n : get_node_names())
        if (n.find("moilcali") != std::string::npos) m.clients.push_back(n);

    pub_->publish(m);

    // The console. Reprinted only when something CHANGES.
    //
    // The comparison is over `signature` -- the subsystem states and details, and
    // the client count -- and deliberately NOT over the header or the self-test
    // age, both of which change every second. Including them was the first
    // version's bug: the table reprinted once a second, 525 times in eight
    // minutes, which is exactly the console nobody reads that this guard exists to
    // prevent. Anything put in the comparison must be a thing that STAYS PUT while
    // the rig is idle.
    QString signature = QString::number(m.clients.size());
    QString table;
    for (size_t i = 0; i < m.subsystems.size(); ++i) {
        const QString name = QString::fromStdString(m.subsystems[i]);
        const QString detail = QString::fromStdString(m.details[i]);
        table += QStringLiteral("%1 %2 %3\n")
                     .arg(name, -10)
                     .arg(QLatin1String(stateName(m.states[i])), -9)
                     .arg(detail);
        // The compute line's detail carries "passed 12s ago", which ticks. Compare
        // its STATE only; a self-test that starts failing still changes the state.
        signature += QStringLiteral("|%1=%2").arg(name).arg(m.states[i]);
        if (name != QLatin1String("compute")) signature += QLatin1Char(':') + detail;
    }

    if (signature != lastPrinted_) {
        lastPrinted_ = signature;
        const QString stamped =
            QStringLiteral("\n[%1]  MoilCali server 2.1.0   up %2   clients: %3\n%4%5")
                .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")))
                .arg(hms(m.uptime))
                .arg(m.clients.size())
                .arg(QString(70, QLatin1Char('-')) + QLatin1Char('\n'))
                .arg(table);
        std::fputs(qPrintable(stamped), stdout);
        std::fflush(stdout);
    }
}

QString SupervisorNode::renderStatus() const { return lastPrinted_; }
