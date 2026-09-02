#pragma once

#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include <Eigen/Dense>

// Shared data model for the 3D-measurement subsystem (ported collectively from
// mvc_model/anypoint_detection_chessboard.py + overlay_compare_point.py). These
// stand in for the pandas DataFrames the Python code passes around.
namespace Measure3d {

using Vec3 = Eigen::Vector3d;

inline double nan() { return std::numeric_limits<double>::quiet_NaN(); }

// One detected checkerboard corner (row of proces_anypoint's DataFrame).
struct DetPoint {
    std::string direction;
    int pointId = 0;
    double xRect = 0, yRect = 0;
    int xFish = -1, yFish = -1;
    double alpha = nan();  // NaN == Python None
    double beta = nan();
    bool hasAlphaBeta() const { return !std::isnan(alpha) && !std::isnan(beta); }
};

// proces_anypoint result for one (camera, direction).
struct DetectionResult {
    std::string direction;
    bool found = false;
    int expected = 0, detected = 0;
    int fishW = 0, fishH = 0;
    std::vector<DetPoint> points;
    bool empty() const { return points.empty(); }
};

// One triangulated 3D point (row of compute_3d_points' output).
struct Point3d {
    std::string direction;
    int pointId = 0;
    Vec3 mid{Vec3::Zero()};
    Vec3 p{Vec3::Zero()};
    Vec3 q{Vec3::Zero()};
    double rayAngleDeg = 0, confidence = 0, rayGap = 0, dMidToCenter = 0;
};

// Reprojection-comparison row (compare_reprojection_with_original output).
struct ReprojRow {
    std::string direction;
    int pointId = 0;
    double u = nan(), v = nan();      // reprojected pixel
    double uGt = nan(), vGt = nan();  // ground-truth (initial detection)
    double error = nan();
};

// image_cali/output_3D/<prefix> (created if missing).
std::string getOutputDir(const std::string &prefix);

}  // namespace Measure3d
