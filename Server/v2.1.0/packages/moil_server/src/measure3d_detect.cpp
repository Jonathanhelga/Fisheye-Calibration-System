// Finding the board and measuring with it: detection, the auto-frame search, and
// the triangulation that turns two views into a plane fit. See
// measure3d_node_p.h for what is in the other half.

#include "measure3d_node.h"
#include "measure3d_node_p.h"

#include <algorithm>
#include <cmath>
#include <thread>

#include <opencv2/calib3d.hpp>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QString>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "AnypointChessboard.h"
#include "PlaneFit3dViz.h"
#include "server_context.h"

using measure3d_detail::encodePng;

namespace {

QJsonObject detectionToJson(const Measure3d::DetectionResult &d) {
    QJsonObject o;
    o["direction"] = QString::fromStdString(d.direction);
    o["found"] = d.found;
    o["expected"] = d.expected;
    o["detected"] = d.detected;
    o["fish_w"] = d.fishW;
    o["fish_h"] = d.fishH;
    QJsonArray pts;
    for (const Measure3d::DetPoint &p : d.points) {
        QJsonObject q;
        q["point_id"] = p.pointId;
        q["x_rect"] = p.xRect;
        q["y_rect"] = p.yRect;
        q["x_fish"] = p.xFish;
        q["y_fish"] = p.yFish;
        // NaN is not representable in JSON. It means "this corner did not map
        // back onto the fisheye", which is a real and distinct outcome from a
        // corner at zero, so it travels as null rather than as a number.
        if (p.hasAlphaBeta()) {
            q["alpha"] = p.alpha;
            q["beta"] = p.beta;
        } else {
            q["alpha"] = QJsonValue();
            q["beta"] = QJsonValue();
        }
        pts.append(q);
    }
    o["points"] = pts;
    return o;
}

QJsonObject mapToJson(const std::map<std::string, double> &m) {
    QJsonObject o;
    for (const auto &kv : m) o[QString::fromStdString(kv.first)] = kv.second;
    return o;
}

}  // namespace

// -------------------------------------------------------------- detection ----

bool Measure3dNode::detectInto(Source &src, const std::string &camKey,
                               const std::string &direction, double pitch, double yaw,
                               double zoom, int mode, int cols, int rows,
                               const std::string &reorder, const std::string &preprocess,
                               cv::Mat *overlay) {
    cv::Mat gray, mapX, mapY;
    const cv::Mat view = anypointView(src, pitch, yaw, zoom, mode, 0, 0, &gray, &mapX, &mapY);
    if (view.empty()) return false;

    cv::Mat overlayAny, overlayFish;
    // The remap tables just built are handed straight in: recomputing them inside
    // the detector is the single most expensive step in this pipeline, and it
    // would be the same transform twice.
    Measure3d::DetectionResult det = AnypointChessboard::procesAnypoint(
        gray, src.moil, src.fisheyeBgr, camKey, direction, cols, rows, pitch, yaw, zoom, reorder,
        preprocess.empty() ? std::string("standard") : preprocess, overlayAny, overlayFish,
        mapX, mapY);

    if (overlay) *overlay = overlayAny.empty() ? view : overlayAny;
    const bool found = det.found;
    src.detections[direction] = std::move(det);
    return found;
}

void Measure3dNode::onDetect(const DetectSrv::Request &req, DetectSrv::Response &res) {
    std::lock_guard<std::mutex> lock(mutex_);
    Source *src = sourceFor(req.camera);
    if (!src || !src->loaded()) {
        res.success = false;
        res.message = "camera \"" + req.camera + "\" is not loaded";
        return;
    }

    cv::Mat overlay;
    const bool found =
        detectInto(*src, req.camera, req.direction, req.alpha, req.beta, req.zoom, req.mode,
                   req.pattern_cols, req.pattern_rows, req.reorder, req.preprocess, &overlay);

    res.found = found;
    // Not finding the board is a normal outcome the operator resolves by
    // re-aiming, not an error -- success stays true.
    res.success = true;
    res.detection_json =
        QString::fromUtf8(
            QJsonDocument(detectionToJson(src->detections[req.direction])).toJson(
                QJsonDocument::Compact))
            .toStdString();
    const QByteArray png = encodePng(overlay, 900);
    res.overlay.header.stamp = now();
    res.overlay.format = "png";
    res.overlay.data.assign(png.begin(), png.end());
    res.message = found ? "" : "no checkerboard found in that view";
}

void Measure3dNode::executeAutoFrame(
    const std::shared_ptr<rclcpp_action::ServerGoalHandle<AutoFrame>> gh) {
    const auto goal = gh->get_goal();
    auto result = std::make_shared<AutoFrame::Result>();

    std::lock_guard<std::mutex> lock(mutex_);
    Source *src = sourceFor(goal->camera);
    if (!src || !src->loaded()) {
        result->success = false;
        result->message = "camera \"" + goal->camera + "\" is not loaded";
        gh->abort(result);
        return;
    }

    // Two stages, and the reasoning behind each is measured rather than guessed --
    // this is the search that was developed against these boards, moved here from
    // the client's dialog unchanged:
    //
    //   A: LOCATE the board by panning pitch/yaw on a fast 1024 canvas. The preset
    //      angle for a direction is a starting point, not the answer: an off-centre
    //      optical axis or an off-centre board puts the real one degrees away.
    //      Zoom 2.5 is a middle value that leaves margin for the whole board.
    //
    //   B: MAXIMISE the board size at that direction, validated at FULL resolution.
    //      An intermediate resolution can spuriously fail where full res succeeds,
    //      and full res is what the real detector uses -- so a view that passes
    //      here is one the detector can also use. Zooms go low to high because a
    //      tight zoom clips the outer ring and only a sub-block detects; keeping
    //      the detection with the MOST corners prefers the full 7x7 over a 6x6.
    //
    // NORMALIZE|EXHAUSTIVE, and findChessboardCornersSB rather than the older
    // detector: one detect per try, and it is the one that copes with these boards.
    const int flags = cv::CALIB_CB_NORMALIZE_IMAGE | cv::CALIB_CB_EXHAUSTIVE;

    // Candidate sizes: the typed size, its transpose, and count-1 in case the
    // operator typed SQUARES rather than inner corners. Ordered by descending
    // corner count so the full board wins over any sub-block.
    std::vector<std::pair<int, int>> sizes;
    const auto addSize = [&sizes](int c, int r) {
        if (c < 3 || r < 3) return;
        for (const auto &s : sizes)
            if (s.first == c && s.second == r) return;
        sizes.push_back({c, r});
    };
    if (goal->pattern_cols > 1 && goal->pattern_rows > 1) {
        addSize(goal->pattern_cols, goal->pattern_rows);
        addSize(goal->pattern_rows, goal->pattern_cols);
        addSize(goal->pattern_cols - 1, goal->pattern_rows - 1);
        addSize(goal->pattern_rows - 1, goal->pattern_cols - 1);
    } else {
        for (int c = 4; c <= 9; ++c) addSize(c, c);  // no hint: try square boards
    }
    std::sort(sizes.begin(), sizes.end(), [](const auto &a, const auto &b) {
        return a.first * a.second > b.first * b.second;
    });

    const auto detectSB = [&](const cv::Mat &gray, int c, int r) {
        if (c < 3 || r < 3) return false;
        std::vector<cv::Point2f> pts;
        try {
            return cv::findChessboardCornersSB(gray, cv::Size(c, r), pts, flags);
        } catch (const cv::Exception &) {
            return false;
        }
    };

    // ---- stage A: locate --------------------------------------------------
    constexpr int kSweep = 1024;
    const double off[] = {0, -15, 15, -30, 30};
    const int total = 25 + 5;  // 5x5 pan, then 5 zooms
    int done = 0;
    bool cancelled = false;

    double locPitch = 0, locYaw = 0;
    int locCorners = 0;
    for (double dp : off) {
        for (double dy : off) {
            if (gh->is_canceling()) { cancelled = true; break; }
            ++done;
            auto fb = std::make_shared<AutoFrame::Feedback>();
            fb->done = done;
            fb->total = total;
            fb->zoom = 2.5;
            fb->stage = "locating";
            gh->publish_feedback(fb);

            cv::Mat gray;
            const cv::Mat view = anypointView(*src, goal->base_pitch + dp, goal->base_yaw + dy,
                                              2.5, 2, kSweep, kSweep, &gray, nullptr, nullptr);
            if (gray.empty()) continue;
            for (const auto &s : sizes) {  // descending corner count
                if (!detectSB(gray, s.first, s.second)) continue;
                if (s.first * s.second > locCorners) {
                    locCorners = s.first * s.second;
                    locPitch = goal->base_pitch + dp;
                    locYaw = goal->base_yaw + dy;
                }
                break;  // largest size for this framing
            }
        }
        if (cancelled) break;
    }

    bool found = false;
    double bestZoom = goal->base_zoom > 0 ? goal->base_zoom : 4.0;
    int bestCols = 0, bestRows = 0;

    // ---- stage B: maximise, at full resolution ----------------------------
    if (locCorners > 0 && !cancelled) {
        const int maxCorners = sizes.front().first * sizes.front().second;
        int bestCorners = 0;
        for (double z : {1.5, 2.0, 2.5, 3.0, 4.0}) {
            if (gh->is_canceling()) { cancelled = true; break; }
            ++done;
            auto fb = std::make_shared<AutoFrame::Feedback>();
            fb->done = done;
            fb->total = total;
            fb->zoom = z;
            fb->stage = "sizing";
            gh->publish_feedback(fb);

            cv::Mat gray;
            const cv::Mat view =
                anypointView(*src, locPitch, locYaw, z, 2, 0, 0, &gray, nullptr, nullptr);
            if (gray.empty()) continue;
            for (const auto &s : sizes) {  // descending -> full board first
                if (!detectSB(gray, s.first, s.second)) continue;
                if (s.first * s.second > bestCorners) {
                    bestCorners = s.first * s.second;
                    found = true;
                    bestZoom = z;
                    bestCols = s.first;
                    bestRows = s.second;
                }
                break;
            }
            if (bestCorners >= maxCorners) break;  // full board -> cannot do better
        }
    }

    RCLCPP_INFO(get_logger(), "[auto_frame] %s/%s located=%d pitch=%.1f yaw=%.1f -> %dx%d z=%.1f",
                goal->camera.c_str(), goal->direction.c_str(), locCorners, locPitch, locYaw,
                bestCols, bestRows, bestZoom);

    result->cancelled = cancelled;
    result->found = found;
    result->alpha = locPitch;
    result->beta = locYaw;
    result->theta = 0.0;
    result->zoom = bestZoom;
    // The size that actually detected -- which may be the transpose or count-1 of
    // what was asked for. The client writes it back, or a later manual Detect at
    // these angles with the old size finds nothing and looks like this lied.
    result->pattern_cols = bestCols;
    result->pattern_rows = bestRows;

    if (found && !cancelled) {
        // A successful auto-frame leaves the direction DETECTED, not merely aimed:
        // the operator's next action is to look at the result, and re-running the
        // detection by hand at the angles just found would be a step that only
        // exists because the sweep stopped short.
        cv::Mat overlay;
        detectInto(*src, goal->camera, goal->direction, locPitch, locYaw, bestZoom, 2, bestCols,
                   bestRows, /*reorder=*/"default", /*preprocess=*/"standard", &overlay);
        result->detection_json =
            QString::fromUtf8(QJsonDocument(detectionToJson(src->detections[goal->direction]))
                                  .toJson(QJsonDocument::Compact))
                .toStdString();
        const QByteArray png = encodePng(overlay, 900);
        result->overlay.format = "png";
        result->overlay.data.assign(png.begin(), png.end());
    }

    result->success = true;
    result->message = found ? "" : (cancelled ? "cancelled" : "no view in the sweep found the board");
    if (cancelled) gh->canceled(result);
    else gh->succeed(result);
}

// ----------------------------------------------------------- triangulation ----

void Measure3dNode::onVerify(const VerifySrv::Request &req, VerifySrv::Response &res) {
    std::lock_guard<std::mutex> lock(mutex_);
    Source *L = sourceFor("left");
    Source *R = sourceFor("right");
    if (!L || !R || !L->loaded() || !R->loaded()) {
        res.success = false;
        res.message = "both cameras must be loaded before a 3D result can be computed";
        return;
    }

    // Only directions BOTH cameras detected can be triangulated. A direction seen
    // by one camera is not an error, but it silently moves nothing and must be
    // named -- a quietly missing direction changes the plane fit without ever
    // looking wrong.
    std::vector<Measure3d::DetPoint> dfL, dfR;
    for (const auto &kv : L->detections) {
        const std::string &dir = kv.first;
        auto rit = R->detections.find(dir);
        if (rit == R->detections.end() || !rit->second.found || !kv.second.found) {
            res.skipped.push_back(dir);
            continue;
        }
        for (const Measure3d::DetPoint &p : kv.second.points)
            if (p.hasAlphaBeta()) dfL.push_back(p);
        for (const Measure3d::DetPoint &p : rit->second.points)
            if (p.hasAlphaBeta()) dfR.push_back(p);
    }
    for (const auto &kv : R->detections)
        if (L->detections.find(kv.first) == L->detections.end())
            res.skipped.push_back(kv.first);

    if (dfL.empty() || dfR.empty()) {
        res.success = false;
        res.message = "no direction was detected by both cameras";
        return;
    }

    // Camera centres in millimetres, as the operator entered them. Nothing on the
    // rig measures these, and the baseline between the two is the one quantity the
    // whole triangulation scales with -- so it is an input, and it arrives with
    // the request rather than being guessed here.
    const Measure3d::Vec3 camL(req.cam_left[0], req.cam_left[1], req.cam_left[2]);
    const Measure3d::Vec3 camR(req.cam_right[0], req.cam_right[1], req.cam_right[2]);

    std::vector<Measure3d::Point3d> pts3d =
        AnypointChessboard::compute3dPoints(dfL, dfR, camL, camR, "verify3d", nullptr);

    // Pattern sizes are (cols, rows) on the wire and the visualiser wants them
    // swapped -- the same swap _pattern_size_for_show_from did in the dialog. It
    // is a real transposition, not a naming quirk: the grid edges come out
    // crossed without it.
    std::map<std::string, std::pair<int, int>> patternForShow;
    for (size_t i = 0; i < req.directions.size(); ++i) {
        const int cols = i < req.pattern_cols.size() ? req.pattern_cols[i] : 0;
        const int rows = i < req.pattern_rows.size() ? req.pattern_rows[i] : 0;
        patternForShow[req.directions[i]] = {rows, cols};
    }

    PlaneFit3dViz::VizResult viz = PlaneFit3dViz::show3dPoint2camOriVisualization(
        pts3d, camL, camR, patternForShow.empty() ? nullptr : &patternForShow, "verify3d");

    // Group the points by direction for the client's GL view. It draws them; it
    // does not derive anything from them.
    QJsonObject byDir;
    for (const Measure3d::Point3d &p : pts3d) {
        const QString d = QString::fromStdString(p.direction);
        QJsonArray arr = byDir.value(d).toArray();
        arr.append(QJsonArray{p.mid.x(), p.mid.y(), p.mid.z()});
        byDir[d] = arr;
    }
    QJsonArray dirs;
    for (auto it = byDir.begin(); it != byDir.end(); ++it)
        dirs.append(QJsonObject{{"dir", it.key()}, {"pts", it.value()}});

    QJsonObject points;
    points["cam_left"] = QJsonArray{camL.x(), camL.y(), camL.z()};
    points["cam_right"] = QJsonArray{camR.x(), camR.y(), camR.z()};
    points["directions"] = dirs;

    QJsonObject metrics;
    metrics["angle"] = mapToJson(viz.angleMap);
    metrics["mean_dist"] = mapToJson(viz.meanMaps);
    metrics["plane_dist"] = mapToJson(viz.meanPlaneDist);
    metrics["depth"] = mapToJson(viz.depthOriginPerDir);

    const PlaneFit3dViz::ReprojCompare cmp = PlaneFit3dViz::compareReprojectionWithOriginal(
        pts3d, dfL, dfR, camL, camR, &L->moil, &R->moil, nullptr);

    QJsonArray reproj;
    const auto addRows = [&reproj](const char *cam, const std::vector<Measure3d::ReprojRow> &rows) {
        for (const Measure3d::ReprojRow &r : rows) {
            QJsonObject o;
            o["camera"] = QString::fromLatin1(cam);
            o["direction"] = QString::fromStdString(r.direction);
            o["point_id"] = r.pointId;
            o["error"] = std::isnan(r.error) ? QJsonValue() : QJsonValue(r.error);
            reproj.append(o);
        }
    };
    addRows("left", cmp.left);
    addRows("right", cmp.right);
    metrics["reprojection"] = reproj;

    res.points_json =
        QString::fromUtf8(QJsonDocument(points).toJson(QJsonDocument::Compact)).toStdString();
    res.metrics_json =
        QString::fromUtf8(QJsonDocument(metrics).toJson(QJsonDocument::Compact)).toStdString();
    res.success = true;
    res.message = "";
}

