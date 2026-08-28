#pragma once

#include <array>
#include <utility>
#include <vector>

#include <Eigen/Dense>
#include <opencv2/core.hpp>

class Moildev;

// Port of mvc_model/moil_3d_algorithm.py (class Moil3dAlgorithm).
//
// Python keeps the whole two-view fisheye triangulation in one class. The C++
// port historically split it into two namespaces which are BOTH preserved here
// so nothing downstream breaks:
//   * namespace Moil3d          — the verified closed-form stereo math + blob
//                                 detection (offline-testable).
//   * namespace Moil3dAlgorithm — the enhanced least-squares / extrinsic
//                                 triangulation + reprojection engine (NEW,
//                                 beyond the Python original) AND the exact
//                                 Python-named snake_case API (1:1) below.

namespace Moil3d {

using Vec3 = std::array<double, 3>;

double betaCartesian(double beta);
Vec3 vectorFromAngles(double alphaDeg, double betaDeg);
Vec3 midPointOfTwoLines(double leftAlpha, double leftBeta, double rightAlpha,
                        double rightBeta, const Vec3 &leftCamCoord,
                        const Vec3 &rightCamCoord);
double quick3dMeasure(const Vec3 &leftCamCoord, double la1, double lb1, double la2, double lb2,
                      const Vec3 &rightCamCoord, double ra1, double rb1, double ra2, double rb2);
std::vector<cv::Point> getDetectionPoint(const cv::Mat &bgr);

}  // namespace Moil3d

namespace Moil3dAlgorithm {

using Vec3 = Eigen::Vector3d;
using Mat3 = Eigen::Matrix3d;

// ---- NEW enhanced engine (no Python counterpart) ----
double betaMoil2Cartesian(double betaMoil);
Vec3 alBa2Vector(double alpha, double beta);
double angleBetweenVectors(const Vec3 &v1, const Vec3 &v2);
Mat3 computeAlignmentRotation(const Vec3 &camL, const Vec3 &camR);

struct TriResult {
    Vec3 p{Vec3::Zero()}, q{Vec3::Zero()}, mid{Vec3::Zero()};
    double rayAngleDeg = 0, confidence = 0, rayGap = 0;
    bool hasGap = false;
};
TriResult triangulateLeastSquaresWithAngle(const Vec3 &cam1, const Vec3 &dir1, const Vec3 &cam2,
                                           const Vec3 &dir2);

struct TriFull {
    Vec3 p{Vec3::Zero()}, q{Vec3::Zero()}, mid{Vec3::Zero()};
    double rayAngleDeg = 0, confidence = 0, rayGap = 0, dMidToCenter = 0;
};
TriFull triangulateAlignedFromAlphaBeta(double alphaL, double betaL, double alphaR, double betaR,
                                        const Vec3 &camL, const Vec3 &camR);
TriFull triangulateWithExtrinsic(double alphaL, double betaL, double alphaR, double betaR,
                                 const Vec3 &camL, const Vec3 &camR, const Mat3 *Rrel);

class Reprojector3d {
public:
    explicit Reprojector3d(const Moildev *moil, const Vec3 &camCoord = Vec3::Zero(),
                           const Mat3 *camR = nullptr);
    std::pair<double, double> reprojectPoint(const Vec3 &pt3d, bool &ok) const;

private:
    std::pair<double, double> pt3dToAlphaBeta(const Vec3 &pt3d, bool &ok) const;
    std::pair<double, double> alphaBetaToPixel(double alpha, double beta) const;

    const Moildev *moil_;
    Vec3 camT_;
    Mat3 camR_;
};

// ---- Exact Python API (mvc_model/moil_3d_algorithm.py), snake_case 1:1 ----
// std::array carriers keep the Python list/tuple semantics without Eigen.
using Coord3 = std::array<double, 3>;

double get_beta_cartesian(double beta);
double get_vector_x(double alpha, double beta);
double get_vector_y(double alpha, double beta);
double get_vector_z(double alpha);
Coord3 get_vector(double alpha, double beta);
// Python: `pass` stub.
Coord3 get_vector_coord2coord(const Coord3 &coord1, const Coord3 &coord2);
// Numeric replacement for the sympy-parametric helper: returns (m, n), the ray
// parameters of the mutual-perpendicular feet for the two camera rays.
std::pair<double, double> solve_m_n(const Coord3 &left_vector, const Coord3 &right_vector,
                                    const Coord3 &left_camera_coord,
                                    const Coord3 &right_camera_coord);
std::pair<Coord3, Coord3> calculate_point(const Coord3 &left_vector, const Coord3 &right_vector,
                                          const Coord3 &left_camera_coord,
                                          const Coord3 &right_camera_coord, double n, double m);
Coord3 get_mid_point_of_p_q(const Coord3 &point_p, const Coord3 &point_q);
Coord3 get_3d_coord_mid_point_of_two_line(double left_alpha, double left_beta, double right_alpha,
                                          double right_beta, const Coord3 &left_camera_coord,
                                          const Coord3 &right_camera_coord);
double quick_3d_measure(const Coord3 &left_camera_3d_coord,
                        const std::pair<int, int> &left_point1_img_coord,
                        const std::pair<int, int> &left_point2_img_coord,
                        const std::string &left_parameter, const Coord3 &right_camera_3d_coord,
                        const std::pair<int, int> &right_point1_img_coord,
                        const std::pair<int, int> &right_point2_img_coord,
                        const std::string &right_parameter);
std::vector<cv::Point> get_detection_point(const cv::Mat &image);

}  // namespace Moil3dAlgorithm
