#include "moil_3d_algorithm.h"

#include <algorithm>
#include <cmath>
#include <string>

#include <opencv2/imgproc.hpp>

#include "Moildev.h"

// ==========================================================================
//  namespace Moil3d — verified closed-form stereo math + blob detection.
// ==========================================================================
namespace Moil3d {

namespace {
double deg2rad(double d) { return d * M_PI / 180.0; }
double dot(const Vec3 &a, const Vec3 &b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
}  // namespace

double betaCartesian(double beta) {
    if (beta < 0) beta += 360;
    if (beta > 360) beta = std::fmod(beta, 360.0);
    if (beta <= 90) return 90 - beta;
    return 360 - (beta - 90);
}

Vec3 vectorFromAngles(double alphaDeg, double betaDeg) {
    const double b = betaCartesian(betaDeg);
    return {std::sin(deg2rad(alphaDeg)) * std::cos(deg2rad(b)),
            std::sin(deg2rad(alphaDeg)) * std::sin(deg2rad(b)),
            std::cos(deg2rad(alphaDeg))};
}

Vec3 midPointOfTwoLines(double leftAlpha, double leftBeta, double rightAlpha, double rightBeta,
                        const Vec3 &Lc, const Vec3 &Rc) {
    const Vec3 L = vectorFromAngles(leftAlpha, leftBeta);
    const Vec3 R = vectorFromAngles(rightAlpha, rightBeta);

    const Vec3 d = {Rc[0] - Lc[0], Rc[1] - Lc[1], Rc[2] - Lc[2]};
    const double a11 = -dot(L, L), a12 = dot(R, L), c1 = -dot(d, L);
    const double a21 = -dot(L, R), a22 = dot(R, R), c2 = -dot(d, R);
    const double det = a11 * a22 - a12 * a21;

    double m = 0, n = 0;
    if (std::abs(det) > 1e-12) {
        m = (c1 * a22 - a12 * c2) / det;
        n = (a11 * c2 - c1 * a21) / det;
    }

    const Vec3 P = {L[0] * m + Lc[0], L[1] * m + Lc[1], L[2] * m + Lc[2]};
    const Vec3 Q = {R[0] * n + Rc[0], R[1] * n + Rc[1], R[2] * n + Rc[2]};
    return {(P[0] + Q[0]) / 2, (P[1] + Q[1]) / 2, (P[2] + Q[2]) / 2};
}

double quick3dMeasure(const Vec3 &Lc, double la1, double lb1, double la2, double lb2,
                      const Vec3 &Rc, double ra1, double rb1, double ra2, double rb2) {
    const Vec3 p = midPointOfTwoLines(la1, lb1, ra1, rb1, Lc, Rc);
    const Vec3 q = midPointOfTwoLines(la2, lb2, ra2, rb2, Lc, Rc);
    return std::sqrt(std::pow(p[0] - q[0], 2) + std::pow(p[1] - q[1], 2) + std::pow(p[2] - q[2], 2));
}

std::vector<cv::Point> getDetectionPoint(const cv::Mat &bgr) {
    cv::Mat gray, mask;
    cv::cvtColor(bgr, gray, cv::COLOR_BGR2GRAY);
    cv::threshold(gray, mask, 150, 255, cv::THRESH_BINARY);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask, contours, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);
    std::sort(contours.begin(), contours.end(),
              [](const auto &a, const auto &b) { return cv::contourArea(a) > cv::contourArea(b); });

    std::vector<cv::Point> centers;
    for (size_t i = 0; i < contours.size() && i < 2; ++i) {
        const cv::Rect r = cv::boundingRect(contours[i]);
        centers.emplace_back(r.x + r.width / 2, r.y + r.height / 2);
    }
    return centers;
}

}  // namespace Moil3d

// ==========================================================================
//  namespace Moil3dAlgorithm — enhanced engine (NEW) + exact Python API.
// ==========================================================================
namespace Moil3dAlgorithm {

namespace {
double deg2rad(double d) { return d * M_PI / 180.0; }
double rad2deg(double r) { return r * 180.0 / M_PI; }
}  // namespace

double betaMoil2Cartesian(double betaMoil) {
    double betaCart = std::fmod(betaMoil, 360.0);
    if (betaCart < 0) betaCart += 360.0;  // Python % always non-negative
    betaCart = 90.0 - betaCart;
    if (betaCart < 0) return betaCart + 360.0;
    return betaCart;
}

Vec3 alBa2Vector(double alpha, double beta) {
    const double b = betaMoil2Cartesian(beta);
    return {std::sin(deg2rad(alpha)) * std::cos(deg2rad(b)),
            std::sin(deg2rad(alpha)) * std::sin(deg2rad(b)), std::cos(deg2rad(alpha))};
}

double angleBetweenVectors(const Vec3 &v1, const Vec3 &v2) {
    const double norms = v1.norm() * v2.norm();
    if (norms == 0) return 0.0;
    double cosA = v1.dot(v2) / norms;
    cosA = std::max(-1.0, std::min(1.0, cosA));
    return rad2deg(std::acos(cosA));
}

TriResult triangulateLeastSquaresWithAngle(const Vec3 &cam1, const Vec3 &dir1, const Vec3 &cam2,
                                           const Vec3 &dir2) {
    TriResult out;
    const Vec3 p1 = cam1;
    const Vec3 p2 = cam2;
    const Vec3 d1 = dir1.normalized();
    const Vec3 d2 = dir2.normalized();

    const Vec3 cross = d1.cross(d2);
    const double denom = cross.squaredNorm();

    out.rayAngleDeg = angleBetweenVectors(d1, d2);
    out.confidence = std::max(0.0, std::min(1.0, out.rayAngleDeg / 90.0));

    if (denom < 1e-8) {
        out.p = p1;
        out.q = p2;
        out.mid = (p1 + p2) / 2.0;
        out.hasGap = false;
        return out;
    }

    const double t = (p2 - p1).cross(d2).dot(cross) / denom;
    const double s = (p2 - p1).cross(d1).dot(cross) / denom;

    const Vec3 c1 = p1 + d1 * t;
    const Vec3 c2 = p2 + d2 * s;
    out.p = c1;
    out.q = c2;
    out.mid = (c1 + c2) / 2.0;
    out.rayGap = (c1 - c2).norm();
    out.hasGap = true;
    return out;
}

Mat3 computeAlignmentRotation(const Vec3 &camL, const Vec3 &camR) {
    Vec3 xAxis = (camR - camL);
    xAxis.normalize();
    Vec3 zAxis(0, 0, 1);
    Vec3 yAxis = zAxis.cross(xAxis);
    yAxis.normalize();
    zAxis = xAxis.cross(yAxis);
    Mat3 R;
    R.col(0) = xAxis;
    R.col(1) = yAxis;
    R.col(2) = zAxis;
    return R;
}

TriFull triangulateAlignedFromAlphaBeta(double alphaL, double betaL, double alphaR, double betaR,
                                        const Vec3 &camL, const Vec3 &camR) {
    const Vec3 dirL = alBa2Vector(alphaL, betaL);
    const Vec3 dirR = alBa2Vector(alphaR, betaR);

    const Mat3 Ralign = computeAlignmentRotation(camL, camR);
    const Mat3 Rt = Ralign.transpose();

    const Vec3 camLa = Rt * (camL - camL);  // [0,0,0]
    const Vec3 camRa = Rt * (camR - camL);
    const Vec3 dirLa = Rt * dirL;
    const Vec3 dirRa = Rt * dirR;

    const TriResult r = triangulateLeastSquaresWithAngle(camLa, dirLa, camRa, dirRa);

    TriFull out;
    out.mid = Ralign * r.mid + camL;
    out.p = Ralign * r.p + camL;
    out.q = Ralign * r.q + camL;
    out.rayAngleDeg = r.rayAngleDeg;
    out.confidence = r.confidence;
    out.rayGap = r.rayGap;

    const Vec3 baselineCenter = (camL + camR) / 2.0;
    out.dMidToCenter = (out.mid - baselineCenter).norm();
    return out;
}

TriFull triangulateWithExtrinsic(double alphaL, double betaL, double alphaR, double betaR,
                                 const Vec3 &camL, const Vec3 &camR, const Mat3 *Rrel) {
    const Vec3 dirL = alBa2Vector(alphaL, betaL);
    Vec3 dirR = alBa2Vector(alphaR, betaR);
    if (Rrel != nullptr) dirR = Rrel->transpose() * dirR;

    const TriResult r = triangulateLeastSquaresWithAngle(camL, dirL, camR, dirR);

    TriFull out;
    out.p = r.p;
    out.q = r.q;
    out.mid = r.mid;
    out.rayAngleDeg = r.rayAngleDeg;
    out.confidence = r.confidence;
    out.rayGap = r.hasGap ? r.rayGap : 0.0;

    const Vec3 baselineCenter = (camL + camR) / 2.0;
    out.dMidToCenter = (out.mid - baselineCenter).norm();
    return out;
}

// ------------------------------- Reprojector -------------------------------

Reprojector3d::Reprojector3d(const Moildev *moil, const Vec3 &camCoord, const Mat3 *camR)
    : moil_(moil), camT_(camCoord), camR_(camR ? *camR : Mat3::Identity()) {}

std::pair<double, double> Reprojector3d::pt3dToAlphaBeta(const Vec3 &pt3d, bool &ok) const {
    ok = false;
    const Vec3 p = pt3d - camT_;
    const Vec3 cam = camR_ * p;
    const double x = cam[0], y = cam[1], z = cam[2];
    const double r = std::sqrt(x * x + y * y + z * z);
    if (r == 0 || std::isnan(r)) return {0, 0};

    const double vz = z / r;
    if (vz < -1.0 || vz > 1.0) return {0, 0};
    const double alpha = std::acos(vz) * 180.0 / M_PI;
    const double betaCart = std::atan2(y / r, x / r) * 180.0 / M_PI;
    double betaMoil = 90.0 - betaCart;
    betaMoil = std::fmod(betaMoil, 360.0);
    if (betaMoil < 0) betaMoil += 360.0;
    ok = true;
    return {alpha, betaMoil};
}

std::pair<double, double> Reprojector3d::alphaBetaToPixel(double alpha, double beta) const {
    const double rho = moil_->getRhoFromAlpha(alpha);
    const double betaCart = 90.0 - beta;
    const double u = moil_->icx() + rho * std::cos(betaCart * M_PI / 180.0);
    const double v = moil_->icy() - rho * std::sin(betaCart * M_PI / 180.0);
    return {u, v};
}

std::pair<double, double> Reprojector3d::reprojectPoint(const Vec3 &pt3d, bool &ok) const {
    const auto ab = pt3dToAlphaBeta(pt3d, ok);
    if (!ok) return {0, 0};
    return alphaBetaToPixel(ab.first, ab.second);
}

// ---- Exact Python API (moil_3d_algorithm.py), snake_case 1:1 --------------

double get_beta_cartesian(double beta) {
    if (beta < 0) beta += 360;
    if (beta > 360) beta = std::fmod(beta, 360.0);
    if (beta <= 90) return 90 - beta;
    return 360 - (beta - 90);
}

double get_vector_x(double alpha, double beta) {
    beta = get_beta_cartesian(beta);
    return std::sin(deg2rad(alpha)) * std::cos(deg2rad(beta));
}
double get_vector_y(double alpha, double beta) {
    beta = get_beta_cartesian(beta);
    return std::sin(deg2rad(alpha)) * std::sin(deg2rad(beta));
}
double get_vector_z(double alpha) { return std::cos(deg2rad(alpha)); }

Coord3 get_vector(double alpha, double beta) {
    return {get_vector_x(alpha, beta), get_vector_y(alpha, beta), get_vector_z(alpha)};
}

Coord3 get_vector_coord2coord(const Coord3 &, const Coord3 &) {
    // Python original is a `pass` stub.
    return {0, 0, 0};
}

std::pair<double, double> solve_m_n(const Coord3 &left_vector, const Coord3 &right_vector,
                                    const Coord3 &left_camera_coord,
                                    const Coord3 &right_camera_coord) {
    // Closed-form of the two sympy orthogonality equations
    //   PQ . left_vector = 0,  PQ . right_vector = 0
    // with P = m*left_vector + left_cam, Q = n*right_vector + right_cam.
    const auto d = [&](const Coord3 &a, const Coord3 &b) {
        return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
    };
    const Coord3 diff = {right_camera_coord[0] - left_camera_coord[0],
                         right_camera_coord[1] - left_camera_coord[1],
                         right_camera_coord[2] - left_camera_coord[2]};
    const double a11 = -d(left_vector, left_vector), a12 = d(right_vector, left_vector);
    const double a21 = -d(left_vector, right_vector), a22 = d(right_vector, right_vector);
    const double c1 = -d(diff, left_vector), c2 = -d(diff, right_vector);
    const double det = a11 * a22 - a12 * a21;
    double m = 0, n = 0;
    if (std::abs(det) > 1e-12) {
        m = (c1 * a22 - a12 * c2) / det;
        n = (a11 * c2 - c1 * a21) / det;
    }
    return {m, n};
}

std::pair<Coord3, Coord3> calculate_point(const Coord3 &left_vector, const Coord3 &right_vector,
                                          const Coord3 &left_camera_coord,
                                          const Coord3 &right_camera_coord, double n, double m) {
    const Coord3 point_p = {left_vector[0] * n + left_camera_coord[0],
                            left_vector[1] * n + left_camera_coord[1],
                            left_vector[2] * n + left_camera_coord[2]};
    const Coord3 point_q = {right_vector[0] * m + right_camera_coord[0],
                            right_vector[1] * m + right_camera_coord[1],
                            right_vector[2] * m + right_camera_coord[2]};
    return {point_p, point_q};
}

Coord3 get_mid_point_of_p_q(const Coord3 &point_p, const Coord3 &point_q) {
    return {(point_p[0] + point_q[0]) / 2, (point_p[1] + point_q[1]) / 2,
            (point_p[2] + point_q[2]) / 2};
}

Coord3 get_3d_coord_mid_point_of_two_line(double left_alpha, double left_beta, double right_alpha,
                                          double right_beta, const Coord3 &left_camera_coord,
                                          const Coord3 &right_camera_coord) {
    const Coord3 left_vec = get_vector(left_alpha, left_beta);
    const Coord3 right_vec = get_vector(right_alpha, right_beta);
    const auto mn = solve_m_n(left_vec, right_vec, left_camera_coord, right_camera_coord);
    // Python calls calculate_point(..., m, n) — first param is m, second n.
    const auto pq = calculate_point(left_vec, right_vec, left_camera_coord, right_camera_coord,
                                    mn.first, mn.second);
    return get_mid_point_of_p_q(pq.first, pq.second);
}

double quick_3d_measure(const Coord3 &left_camera_3d_coord,
                        const std::pair<int, int> &left_point1_img_coord,
                        const std::pair<int, int> &left_point2_img_coord,
                        const std::string &left_parameter, const Coord3 &right_camera_3d_coord,
                        const std::pair<int, int> &right_point1_img_coord,
                        const std::pair<int, int> &right_point2_img_coord,
                        const std::string &right_parameter) {
    Moildev left_moildev(left_parameter);
    Moildev right_moildev(right_parameter);

    const auto round1 = [](double v) { return std::round(v * 10.0) / 10.0; };
    bool ok = false;

    auto ab = left_moildev.getAlphaBeta(left_point1_img_coord.first, left_point1_img_coord.second, 1,
                                        ok);
    double la = round1(ab.first), lb = round1(ab.second);
    ab = right_moildev.getAlphaBeta(right_point1_img_coord.first, right_point1_img_coord.second, 1,
                                    ok);
    double ra = round1(ab.first), rb = round1(ab.second);
    const Coord3 point_p = get_3d_coord_mid_point_of_two_line(la, lb, ra, rb, left_camera_3d_coord,
                                                              right_camera_3d_coord);

    ab = left_moildev.getAlphaBeta(left_point2_img_coord.first, left_point2_img_coord.second, 1, ok);
    la = round1(ab.first);
    lb = round1(ab.second);
    ab = right_moildev.getAlphaBeta(right_point2_img_coord.first, right_point2_img_coord.second, 1,
                                    ok);
    ra = round1(ab.first);
    rb = round1(ab.second);
    const Coord3 point_q = get_3d_coord_mid_point_of_two_line(la, lb, ra, rb, left_camera_3d_coord,
                                                              right_camera_3d_coord);

    return std::sqrt(std::pow(point_p[0] - point_q[0], 2) + std::pow(point_p[1] - point_q[1], 2) +
                     std::pow(point_p[2] - point_q[2], 2));
}

std::vector<cv::Point> get_detection_point(const cv::Mat &image) {
    return Moil3d::getDetectionPoint(image);
}

}  // namespace Moil3dAlgorithm
