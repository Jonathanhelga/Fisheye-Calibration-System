#pragma once

#include <functional>
#include <string>
#include <vector>

#include <QString>
#include <opencv2/core.hpp>

class CaliTableData;

// The calibration computations, addressed by name, with JSON parameters and JSON
// results.
//
// Both sides of the ROS boundary call THESE functions. The rig's compute node is
// a shell that decodes a service request and calls runDetectOp/runCaliOp; the
// client, when the node is not there, calls the same two functions in-process.
// So "the client falls back to computing locally" does not mean there is a second
// implementation to keep in step -- there is one, and the service is a transport
// in front of it.
//
// That is the mistake this design is avoiding. The pattern renderer was moved to
// the rig by porting it to Python, which left two implementations of the same
// drawing and a test (test_mirror_cpp.py) whose only job is to notice when they
// drift. Nothing here needs such a test, because there is nothing to compare.
namespace ComputeOps {

// ---- image analysis (ComputeDetect) ---------------------------------------
// Op names. Chosen to match the function they wrap so a name in a log line leads
// straight to the code.
namespace detect {
inline constexpr auto kNodes8Dir = "nodes_8dir";              // pos+neg -> per-direction node lists
inline constexpr auto kHistogram8Dir = "histogram_8dir";      // curves + nodes for the plot panel
inline constexpr auto kFisheyeEdge = "fisheye_edge";          // image -> centre + radius
inline constexpr auto kRoi = "roi";                           // image + seed -> refined point
inline constexpr auto kRingCenter = "ring_center";            // image + ring count -> centre
inline constexpr auto kPatternRingRadii = "pattern_ring_radii";  // pattern PNG -> ring radii

// New in v2.1.0. Both came out of the client's main window, where they were
// private methods over a cv::Mat -- which the client no longer has.
inline constexpr auto kRoiExact = "roi_exact";          // seed -> settled point (recursed here)
inline constexpr auto kPatternCenter = "pattern_center";  // image -> centre by gradient fit

// The cascade: pattern_center, VALIDATED against the ring cost, escalating to
// ring_center when the cheap fit cannot be trusted. One op rather than the client
// chaining the four above, for three reasons that are not about round trips:
//
//   * the validation needs the ring cost at five points, and there is no op for
//     that -- driven from a client it would be five more calls;
//   * computeMutex is taken PER OP, so a client-driven chain releases it between
//     stages and lets another client's request flip the process-wide noise and
//     raw-node flags halfway through the cascade;
//   * the ring count comes from the prepared pattern, which only this side has.
inline constexpr auto kAutoCenter = "auto_center";  // image -> centre + method + confidence
}  // namespace detect

// `images` holds the decoded captures the op needs, in the order its params
// document (pos then neg for the two-image ops). Returns the result JSON, or an
// empty string with *err set.
//
// Every op takes noise_cleaning in its params and applies it around the call:
// it is a process-wide flag inside MoilCali, and a node serving several clients
// must not answer with whatever the last request happened to set.
QString runDetectOp(const QString &op, const std::vector<cv::Mat> &images,
                    const QString &paramsJson, QString *err);

// How many images the op needs, so a caller can reject a malformed request before
// decoding anything. -1 for an unknown op.
int detectImageCount(const QString &op);

// Where PreparePatterns left the rendered patterns, so auto_center can count the
// rings of the picture that was actually on the glass.
//
// A path rather than a MonitorDevice on purpose. runDetectOp is called from both
// sides of the ROS boundary and from standalone test binaries, and giving it a
// server type would drag the whole device layer into every one of them. A string
// set once at start-up is also the seam the tests use to point the op at a
// directory they built themselves.
//
// Set from ServerContext::start(); empty until then, which simply means the
// PNG step of the fallback chain is skipped. Written once before any request is
// served and only read afterwards (and every detect op runs under computeMutex),
// so it needs no lock of its own.
void setPreparedDir(const QString &dir);
QString preparedDir();

// ---- calibration pipeline (ComputeCali) -----------------------------------
namespace cali {
inline constexpr auto kComputeAll = "compute_all";
inline constexpr auto kCalculateResult = "calculate_result";
inline constexpr auto kCalculateResultSingleRound = "calculate_result_single_round";
inline constexpr auto kCalculateResultWithBaseDistance = "calculate_result_with_base_distance";
inline constexpr auto kAggregationByDistance = "aggregation_by_distance";
inline constexpr auto kAggregationAllRoundsByDistance = "aggregation_all_rounds_by_distance";
inline constexpr auto kFindMinAggrSingleRound = "find_min_aggr_single_round";
inline constexpr auto kFindMinAggregationInWindow = "find_min_aggregation_in_window";
inline constexpr auto kFindDistanceForTargetAggregation = "find_distance_for_target_aggregation";
inline constexpr auto kAutoDetectNoiseBands = "auto_detect_noise_bands";
inline constexpr auto kRemoveNodesInBand = "remove_nodes_in_band";
inline constexpr auto kUpdateTableFromCapture = "update_table_from_capture";
}  // namespace cali

// Runs `op` against `table`, IN PLACE -- the derived columns and the aggregation
// read-outs are written back into it, and the caller ships the mutated table home.
// `resultJson` receives the op's scalar answers (best distance, removed count,
// detected bands, ...); it is "{}" for the ops whose only output is the table.
//
// The plot/series getters (ictZflSeries, ihAlphaRegression, alphaPolynomial,
// globalIctAlpha) are deliberately NOT here. They only read columns the pipeline
// has already written, so the client computes them from the table it just got
// back -- routing them would add a round trip per graph redraw and per cursor
// move for no arithmetic worth moving.
// Progress from inside the pipeline: (done, total, stage). Return false to CANCEL,
// which unwinds the search wherever it is and leaves the table at whatever the last
// probe wrote -- the same thing the client's old progress dialog did when the
// operator hit Cancel.
//
// total is 0 for ops that do not report progress; a caller should show an
// indeterminate indicator rather than a 0% bar.
using ProgressFn = std::function<bool(int done, int total, const QString &stage)>;

bool runCaliOp(const QString &op, CaliTableData &table, const QString &paramsJson,
               QString *resultJson, QString *err, const ProgressFn &progress = nullptr);

// ---- pattern rendering (RenderPattern) ------------------------------------
// Renders the pattern JSON (the format the Pattern Generator's Json button
// writes) at `width` x `height`, or at the JSON's own size when either is 0.
// Empty Mat with *err set on a malformed spec.
cv::Mat renderPattern(const QString &patternJson, int width, int height, QString *err);

}  // namespace ComputeOps
