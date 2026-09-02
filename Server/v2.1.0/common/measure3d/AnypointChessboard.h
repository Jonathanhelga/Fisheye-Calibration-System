#pragma once

#include <string>
#include <utility>

#include <opencv2/core.hpp>

#include "Measure3dTypes.h"

class Moildev;

// Port of mvc_model/anypoint_detection_chessboard.py.
//
// Detects a checkerboard on an anypoint (rectilinear) view, maps every corner
// back to the source fisheye through the anypoint remap tables, reads its
// (alpha, beta), and (compute3dPoints) triangulates matched left/right corners.
namespace AnypointChessboard {

// enhance_for_checkerboard: CLAHE + bilateral + boost + normalize + unsharp.
cv::Mat enhanceForCheckerboard(const cv::Mat &gray);

// apply_reorder_mode: reshuffle corners for one of the reorder modes
// (default / flip_horizontal / flip_vertical / rotate_90/180/270).
std::vector<cv::Point2f> applyReorderMode(const std::vector<cv::Point2f> &corners, int cols,
                                          int rows, const std::string &mode);

// proces_anypoint. `anyGray` is the rectilinear anypoint view (grayscale);
// `fisheyeBgr` is the original fisheye used to build overlayFish. `patternCols`
// / `patternRows` == pattern_size (cols, rows); angles == (pitch, yaw, zoom).
// Writes the per-direction CSVs + overlay PNGs into image_cali/output_3D/
// <outputPrefix> and appends to the combined buffer, exactly like Python.
//
// `mapXIn`/`mapYIn`: the anypoint remap tables for (pitch,yaw,zoom). If provided
// (non-empty) they are reused directly; if empty they are recomputed from `moil`
// (mapsAnypointMode2 is the single most expensive step, so the caller normally
// passes the map it already built for the anypoint display).
Measure3d::DetectionResult procesAnypoint(const cv::Mat &anyGray, const Moildev &moil,
                                          const cv::Mat &fisheyeBgr, const std::string &outputPrefix,
                                          const std::string &direction, int patternCols,
                                          int patternRows, double pitch, double yaw, double zoom,
                                          const std::string &reorderMode,
                                          const std::string &preprocessMode, cv::Mat &overlayAny,
                                          cv::Mat &overlayFish, const cv::Mat &mapXIn = cv::Mat(),
                                          const cv::Mat &mapYIn = cv::Mat());

// save_combined_alpha_beta: flush the buffer for `outputPrefix` to
// <prefix>_all_directions_alpha_beta.csv (dedup by direction+point_id, last).
void saveCombinedAlphaBeta(const std::string &outputPrefix);

// compute_3d_points. `dfL`/`dfR` are the already-validated (alpha,beta present)
// detections. Rrel = nullptr -> aligned triangulation; else full extrinsic.
// Writes triangulated_3d_points.{json,csv} into output_3D/<outputPrefix>.
std::vector<Measure3d::Point3d> compute3dPoints(const std::vector<Measure3d::DetPoint> &dfL,
                                                const std::vector<Measure3d::DetPoint> &dfR,
                                                const Measure3d::Vec3 &camL,
                                                const Measure3d::Vec3 &camR,
                                                const std::string &outputPrefix,
                                                const Eigen::Matrix3d *Rrel);

}  // namespace AnypointChessboard
