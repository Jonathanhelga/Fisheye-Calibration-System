#pragma once

#include <memory>

#include <QString>
#include <QStringList>
#include <QVector>

#include "server_node.h"

#include "moil_interfaces/action/cali_job.hpp"
#include "moil_interfaces/srv/cali_op.hpp"
#include "moil_interfaces/srv/cali_series.hpp"
#include "moil_interfaces/srv/detect_op.hpp"
#include "moil_interfaces/srv/render_pattern.hpp"
#include "moil_interfaces/srv/self_test.hpp"
#include "moil_interfaces/srv/xlsx_io.hpp"

struct ServerContext;

// /moil_compute -- the calibration maths.
//
// This node is the reason the split is worth doing. In v2.0 every one of these
// ran inside the operator's window: a distance search that recomputes eleven
// round tables 282 times, an eight-direction node detection over two 3040x3040
// frames, a degree-4 regression on every graph redraw. All of it is here now, on
// the machine that also holds the rig, and the client's copy of the arithmetic is
// gone rather than merely unused.
//
// The op dispatchers are unchanged from the engine -- ComputeOps::runDetectOp and
// runCaliOp are the same functions, called the same way. That is deliberate and
// it is the property the v2.0 design was built to keep: there is ONE
// implementation of each formula, and the service is a transport in front of it.
// Nothing here re-implements anything, so there is no second version to drift.
class ComputeNode : public ServerNode {
public:
    ComputeNode(ServerContext &ctx, const rclcpp::NodeOptions &options = rclcpp::NodeOptions());

private:
    using DetectOp = moil_interfaces::srv::DetectOp;
    using CaliOp = moil_interfaces::srv::CaliOp;
    using CaliSeries = moil_interfaces::srv::CaliSeries;
    using RenderPattern = moil_interfaces::srv::RenderPattern;
    using XlsxIoSrv = moil_interfaces::srv::XlsxIo;
    using SelfTest = moil_interfaces::srv::SelfTest;
    using CaliJob = moil_interfaces::action::CaliJob;

    void onDetect(const DetectOp::Request &req, DetectOp::Response &res);
    void onCali(const CaliOp::Request &req, CaliOp::Response &res);
    void onSeries(const CaliSeries::Request &req, CaliSeries::Response &res);
    void onRenderPattern(const RenderPattern::Request &req, RenderPattern::Response &res);
    void onXlsx(const XlsxIoSrv::Request &req, XlsxIoSrv::Response &res);
    void onSelfTest(const SelfTest::Request &req, SelfTest::Response &res);
    void executeCaliJob(const std::shared_ptr<rclcpp_action::ServerGoalHandle<CaliJob>> gh);
};

// Run the recorded-baseline checks. Shared with the supervisor, which calls it on
// a timer -- see SelfTest.srv for why a health check that only proves the
// transport works is not enough for a compute node.
namespace ComputeSelfTest {

struct SuiteResult {
    QString name;
    bool passed = false;
    double maxDeviation = 0.0;
    QStringList notes;
};

// `suite` is "", "all", or one of "cali" | "detect" | "pattern" | "measure3d".
QVector<SuiteResult> run(const QString &suite, double *durationSeconds);

}  // namespace ComputeSelfTest
