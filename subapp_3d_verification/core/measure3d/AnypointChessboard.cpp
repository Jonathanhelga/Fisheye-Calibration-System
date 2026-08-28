#include "AnypointChessboard.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <map>
#include <sstream>

#include <opencv2/calib3d.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "moil_3d_algorithm.h"
#include "Moildev.h"

namespace AnypointChessboard {

using Measure3d::DetPoint;
using Measure3d::DetectionResult;
using Measure3d::Point3d;
using Measure3d::Vec3;

namespace {
// Combined alpha-beta buffer (Python module-level _alpha_beta_buffer).
std::map<std::string, std::vector<DetPoint>> g_buffer;

std::string fmtNum(double v) {
    if (std::isnan(v)) return "";  // pandas writes empty for NaN
    std::ostringstream os;
    os << v;
    return os.str();
}
}  // namespace

cv::Mat enhanceForCheckerboard(const cv::Mat &gray) {
    cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(3.0, cv::Size(6, 6));
    cv::Mat imgEq;
    clahe->apply(gray, imgEq);
    cv::Mat filtered;
    cv::bilateralFilter(imgEq, filtered, 7, 50, 50);
    cv::Mat boosted;
    cv::convertScaleAbs(filtered, boosted, 1.4, 15);
    cv::Mat norm;
    cv::normalize(boosted, norm, 0, 255, cv::NORM_MINMAX);
    cv::Mat gauss;
    cv::GaussianBlur(norm, gauss, cv::Size(5, 5), 1.0);
    cv::Mat out;
    cv::addWeighted(norm, 1.5, gauss, -0.5, 0, out);
    return out;
}

std::vector<cv::Point2f> applyReorderMode(const std::vector<cv::Point2f> &corners, int cols,
                                          int rows, const std::string &mode) {
    auto at = [&](int r, int c) -> const cv::Point2f & { return corners[r * cols + c]; };
    std::vector<cv::Point2f> out;
    out.reserve(corners.size());

    if (mode == "flip_horizontal") {
        for (int r = 0; r < rows; ++r)
            for (int c = 0; c < cols; ++c) out.push_back(at(r, cols - 1 - c));
    } else if (mode == "flip_vertical") {
        for (int r = 0; r < rows; ++r)
            for (int c = 0; c < cols; ++c) out.push_back(at(rows - 1 - r, c));
    } else if (mode == "rotate_90") {
        // shape (cols, rows): out[i][j] = A[j][cols-1-i]
        for (int i = 0; i < cols; ++i)
            for (int j = 0; j < rows; ++j) out.push_back(at(j, cols - 1 - i));
    } else if (mode == "rotate_180") {
        for (int i = 0; i < rows; ++i)
            for (int j = 0; j < cols; ++j) out.push_back(at(rows - 1 - i, cols - 1 - j));
    } else if (mode == "rotate_270") {
        // shape (cols, rows): out[i][j] = A[rows-1-j][i]
        for (int i = 0; i < cols; ++i)
            for (int j = 0; j < rows; ++j) out.push_back(at(rows - 1 - j, i));
    } else {
        out = corners;  // default / adaptive (no reshuffle)
    }
    return out;
}

DetectionResult procesAnypoint(const cv::Mat &anyGray, const Moildev &moil,
                               const cv::Mat &fisheyeBgr, const std::string &outputPrefix,
                               const std::string &direction, int patternCols, int patternRows,
                               double pitch, double yaw, double zoom, const std::string &reorderMode,
                               const std::string &preprocessMode, cv::Mat &overlayAny,
                               cv::Mat &overlayFish, const cv::Mat &mapXIn, const cv::Mat &mapYIn) {
    DetectionResult res;
    res.direction = direction;
    res.expected = patternCols * patternRows;

    cv::Mat gray = anyGray;
    if (gray.channels() == 3) cv::cvtColor(anyGray, gray, cv::COLOR_BGR2GRAY);
    cv::Mat workingGray = (preprocessMode == "enhanced") ? enhanceForCheckerboard(gray) : gray;

    const cv::Size pattern(patternCols, patternRows);
    // findChessboardCornersSB only accepts the SB flag set; passing the classic
    // detector's flags (ADAPTIVE_THRESH / FILTER_QUADS) makes it throw cv::Exception.
    // Try ACCURACY (subpixel-refined corners) first, but fall back to the plain
    // exhaustive detector if it rejects the board: ACCURACY is stricter and
    // otherwise silently fails on slightly warped / lower-contrast boards that are
    // still perfectly usable (this was the main "cannot detect" cause).
    std::vector<cv::Point2f> corners;
    bool found = false;
    if (patternCols >= 2 && patternRows >= 2 && !workingGray.empty()) {
        for (int flags : {cv::CALIB_CB_NORMALIZE_IMAGE | cv::CALIB_CB_EXHAUSTIVE |
                              cv::CALIB_CB_ACCURACY,
                          cv::CALIB_CB_NORMALIZE_IMAGE | cv::CALIB_CB_EXHAUSTIVE}) {
            try {
                found = cv::findChessboardCornersSB(workingGray, pattern, corners, flags);
            } catch (const cv::Exception &) {
                // Bad input (odd pattern, degenerate image): treat as "not found"
                // rather than crashing the UI.
                found = false;
                corners.clear();
            }
            if (found) break;
        }
    }

    cv::cvtColor(gray, overlayAny, cv::COLOR_GRAY2BGR);
    overlayFish = fisheyeBgr.clone();
    res.found = found;
    res.detected = found ? static_cast<int>(corners.size()) : 0;

    if (!found) return res;  // empty DataFrame

    corners = applyReorderMode(corners, patternCols, patternRows, reorderMode);
    std::vector<cv::Point> rectPts;
    rectPts.reserve(corners.size());
    for (const auto &p : corners) rectPts.emplace_back(static_cast<int>(p.x), static_cast<int>(p.y));

    for (size_t idx = 0; idx < rectPts.size(); ++idx) {
        cv::drawMarker(overlayAny, rectPts[idx], cv::Scalar(0, 0, 255), cv::MARKER_TILTED_CROSS, 20,
                       2);
        cv::putText(overlayAny, std::to_string(idx),
                    cv::Point(rectPts[idx].x + 5, rectPts[idx].y - 5), cv::FONT_HERSHEY_SIMPLEX, 0.4,
                    cv::Scalar(0, 0, 255), 1);
    }

    // Reuse the caller's precomputed remap tables; only rebuild them (the single
    // most expensive step) when none were supplied.
    cv::Mat mapX = mapXIn, mapY = mapYIn;
    if (mapX.empty() || mapY.empty()) moil.mapsAnypointMode2(pitch, yaw, zoom, mapX, mapY);

    res.fishW = fisheyeBgr.cols;
    res.fishH = fisheyeBgr.rows;

    for (size_t idx = 0; idx < rectPts.size(); ++idx) {
        const int rx = rectPts[idx].x;
        const int ry = rectPts[idx].y;
        int xIdx = static_cast<int>(std::lround(rx));
        int yIdx = static_cast<int>(std::lround(ry));

        int xf = -1, yf = -1;
        if (xIdx >= 0 && xIdx < mapX.cols && yIdx >= 0 && yIdx < mapX.rows) {
            xf = static_cast<int>(mapX.at<float>(yIdx, xIdx));
            yf = static_cast<int>(mapY.at<float>(yIdx, xIdx));
        }

        DetPoint dp;
        dp.direction = direction;
        dp.pointId = static_cast<int>(idx);
        dp.xRect = rx;
        dp.yRect = ry;
        dp.xFish = xf;
        dp.yFish = yf;

        if (xf >= 0 && yf >= 0) {
            cv::drawMarker(overlayFish, cv::Point(xf, yf), cv::Scalar(0, 0, 255),
                           cv::MARKER_TILTED_CROSS, 30, 2);
            const std::string text = std::to_string(idx);
            cv::putText(overlayFish, text, cv::Point(xf + 6, yf - 6), cv::FONT_HERSHEY_SIMPLEX, 0.5,
                        cv::Scalar(255, 255, 255), 3, cv::LINE_AA);
            cv::putText(overlayFish, text, cv::Point(xf + 6, yf - 6), cv::FONT_HERSHEY_SIMPLEX, 0.5,
                        cv::Scalar(0, 0, 255), 1, cv::LINE_AA);
            bool ok = false;
            const auto ab = moil.getAlphaBeta(xf, yf, 1, ok);
            if (ok) {
                dp.alpha = ab.first;
                dp.beta = ab.second;
            }
        }
        res.points.push_back(dp);
    }

    // ---- persist CSVs + overlays (image_cali/output_3D/<outputPrefix>) ----
    const std::string outDir = Measure3d::getOutputDir(outputPrefix);

    {
        std::ofstream rect(outDir + "/" + outputPrefix + "_" + direction + "_alpha_beta.csv");
        rect << "direction,point_id,x_rect,y_rect\n";
        for (const auto &p : res.points)
            rect << p.direction << ',' << p.pointId << ',' << p.xRect << ',' << p.yRect << '\n';
    }
    {
        std::ofstream fish(outDir + "/" + outputPrefix + "_" + direction + "_alpha_beta_fish.csv");
        fish << "direction,point_id,x_fish,y_fish,alpha,beta\n";
        for (const auto &p : res.points)
            fish << p.direction << ',' << p.pointId << ',' << p.xFish << ',' << p.yFish << ','
                 << fmtNum(p.alpha) << ',' << fmtNum(p.beta) << '\n';
    }
    cv::imwrite(outDir + "/overlay_any_" + direction + ".png", overlayAny);
    cv::imwrite(outDir + "/overlay_fish_" + direction + ".png", overlayFish);

    // Buffer for the combined CSV (fisheye columns only).
    auto &buf = g_buffer[outputPrefix];
    buf.insert(buf.end(), res.points.begin(), res.points.end());

    return res;
}

void saveCombinedAlphaBeta(const std::string &outputPrefix) {
    auto it = g_buffer.find(outputPrefix);
    if (it == g_buffer.end()) return;

    // drop_duplicates(subset=[direction, point_id], keep="last") + sort.
    std::map<std::pair<std::string, int>, DetPoint> keepLast;
    for (const auto &p : it->second) keepLast[{p.direction, p.pointId}] = p;

    std::vector<DetPoint> rows;
    rows.reserve(keepLast.size());
    for (auto &kv : keepLast) rows.push_back(kv.second);
    std::sort(rows.begin(), rows.end(), [](const DetPoint &a, const DetPoint &b) {
        if (a.direction != b.direction) return a.direction < b.direction;
        return a.pointId < b.pointId;
    });

    const std::string outDir = Measure3d::getOutputDir(outputPrefix);
    std::ofstream csv(outDir + "/" + outputPrefix + "_all_directions_alpha_beta.csv");
    csv << "direction,point_id,x_fish,y_fish,alpha,beta\n";
    for (const auto &p : rows)
        csv << p.direction << ',' << p.pointId << ',' << p.xFish << ',' << p.yFish << ','
            << fmtNum(p.alpha) << ',' << fmtNum(p.beta) << '\n';

    g_buffer.erase(it);
}

std::vector<Point3d> compute3dPoints(const std::vector<DetPoint> &dfL,
                                     const std::vector<DetPoint> &dfR, const Vec3 &camL,
                                     const Vec3 &camR, const std::string &outputPrefix,
                                     const Eigen::Matrix3d *Rrel) {
    // Merge on (direction, point_id).
    std::map<std::pair<std::string, int>, const DetPoint *> rIndex;
    for (const auto &r : dfR) rIndex[{r.direction, r.pointId}] = &r;

    std::vector<Point3d> out;
    for (const auto &l : dfL) {
        auto it = rIndex.find({l.direction, l.pointId});
        if (it == rIndex.end()) continue;
        const DetPoint &r = *it->second;
        if (!l.hasAlphaBeta() || !r.hasAlphaBeta()) continue;

        Moil3dAlgorithm::TriFull t =
            Rrel ? Moil3dAlgorithm::triangulateWithExtrinsic(l.alpha, l.beta, r.alpha, r.beta, camL,
                                                             camR, Rrel)
                 : Moil3dAlgorithm::triangulateAlignedFromAlphaBeta(l.alpha, l.beta, r.alpha, r.beta,
                                                                    camL, camR);
        Point3d p;
        p.direction = l.direction;
        p.pointId = l.pointId;
        p.mid = t.mid;
        p.p = t.p;
        p.q = t.q;
        p.rayAngleDeg = t.rayAngleDeg;
        p.confidence = t.confidence;
        p.rayGap = t.rayGap;
        p.dMidToCenter = t.dMidToCenter;
        out.push_back(p);
    }

    // Persist JSON + CSV (Python writes .json + .xlsx; CSV stands in for xlsx).
    const std::string outDir = Measure3d::getOutputDir(outputPrefix);
    {
        std::ofstream js(outDir + "/triangulated_3d_points.json");
        js << "[\n";
        for (size_t i = 0; i < out.size(); ++i) {
            const Point3d &p = out[i];
            js << "  {\"direction\": \"" << p.direction << "\", \"point_id\": " << p.pointId
               << ", \"x\": " << p.mid[0] << ", \"y\": " << p.mid[1] << ", \"z\": " << p.mid[2]
               << ", \"point_p\": [" << p.p[0] << ", " << p.p[1] << ", " << p.p[2] << "]"
               << ", \"point_q\": [" << p.q[0] << ", " << p.q[1] << ", " << p.q[2] << "]"
               << ", \"ray_angle_deg\": " << p.rayAngleDeg << ", \"confidence\": " << p.confidence
               << ", \"ray_gap\": " << p.rayGap << ", \"d_mid_to_center\": " << p.dMidToCenter
               << "}" << (i + 1 < out.size() ? "," : "") << "\n";
        }
        js << "]\n";
    }
    {
        std::ofstream csv(outDir + "/triangulated_3d_points.csv");
        csv << "direction,point_id,x,y,z,ray_angle_deg,confidence,ray_gap,d_mid_to_center\n";
        for (const auto &p : out)
            csv << p.direction << ',' << p.pointId << ',' << p.mid[0] << ',' << p.mid[1] << ','
                << p.mid[2] << ',' << p.rayAngleDeg << ',' << p.confidence << ',' << p.rayGap << ','
                << p.dMidToCenter << '\n';
    }
    return out;
}

}  // namespace AnypointChessboard
