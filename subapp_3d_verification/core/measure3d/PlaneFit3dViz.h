#pragma once

#include <map>
#include <string>
#include <utility>
#include <vector>

#include <Eigen/Dense>

#include "Measure3dTypes.h"

class Moildev;

// Port of mvc_model/overlay_compare_point.py.
//
// Given the triangulated 3D points, fit a plane per checkerboard direction
// (PCA / SVD), derive the inter-plane angles, mean ray gaps, plane thickness
// and depth-to-origin, render two interactive Plotly HTML views, and compare
// the reprojected pixels against the original detections.
namespace PlaneFit3dViz {

using Measure3d::Point3d;
using Measure3d::ReprojRow;
using Measure3d::Vec3;

struct VizResult {
    std::string pathBasic;                       // 3d_points_only.html
    std::string pathFull;                        // 3d_points_with_pq.html
    std::map<std::string, double> angleMap;      // "wc","wn",... inter-plane angles
    std::map<std::string, double> meanMaps;      // per-direction + "all" mean ray_gap
    std::map<std::string, double> meanPlaneDist; // per-direction + "all" thickness (mm)
    std::map<std::string, double> depthOriginPerDir;  // |signed offset| per direction
};

// One fitted plane for a direction, in the form a renderer needs. The patch is
// the TIGHTEST rectangle around the points once they are projected onto the
// plane: basis1/basis2 follow the board's own directions and the four corners
// (centroid ± half1·basis1 ± half2·basis2) land on the outermost points, so the
// drawn quad follows the data instead of floating beside it. `normal` faces
// `faceTo`; thickness/rms are the orthogonal residual spread the metrics report.
struct DirPlane {
    std::string direction;
    Vec3 normal{Vec3::Zero()};
    Vec3 centroid{Vec3::Zero()};  // centre of the patch (a point on the plane)
    Vec3 basis1{Vec3::Zero()}, basis2{Vec3::Zero()};
    double half1 = 0, half2 = 0;   // patch half-extent along basis1 / basis2 (mm)
    double thickness = 0, rms = 0; // orthogonal residual spread / RMS (mm)
    int pointCount = 0;
};

// Fit one plane per direction present in df3d (same PCA/SVD fit the angle and
// thickness metrics use, so the drawn plane is the one being measured).
// Directions with fewer than 3 points are skipped.
std::vector<DirPlane> fitPlanesPerDirection(const std::vector<Point3d> &df3d, const Vec3 &faceTo);

// show_3d_point_2cam_ori_visualization. patternSize maps direction ->
// (cols, rows); pass nullptr to skip grid-edge drawing.
VizResult show3dPoint2camOriVisualization(
    std::vector<Point3d> &df3d, const Vec3 &camL, const Vec3 &camR,
    const std::map<std::string, std::pair<int, int>> *patternSize,
    const std::string &outputPrefix = "triangulation_3d");

struct ReprojCompare {
    std::vector<ReprojRow> left;
    std::vector<ReprojRow> right;
};

// compare_reprojection_with_original. Ground-truth pixels come straight from the
// in-memory detections `dfL`/`dfR` (the same points feeding triangulation), so it
// no longer round-trips through the *_all_directions_alpha_beta.csv files and no
// longer depends on the "Overlay result" button having flushed them first.
// Reprojects every 3D point and returns per-point pixel errors.
ReprojCompare compareReprojectionWithOriginal(const std::vector<Point3d> &df3d,
                                              const std::vector<Measure3d::DetPoint> &dfL,
                                              const std::vector<Measure3d::DetPoint> &dfR,
                                              const Vec3 &camL, const Vec3 &camR,
                                              const Moildev *moilL, const Moildev *moilR,
                                              const Eigen::Matrix3d *camRmat);

}  // namespace PlaneFit3dViz
