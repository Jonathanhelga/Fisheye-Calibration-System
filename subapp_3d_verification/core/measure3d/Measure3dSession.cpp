#include "Measure3dSession.h"

#include <cmath>
#include <sstream>

#include <opencv2/core/persistence.hpp>

namespace Measure3d {
namespace {

// cv::FileStorage cannot round-trip a NaN (MSVC prints "-nan(ind)", which the
// parser then rejects), so NaN travels as a sentinel far outside any real
// measurement and is decoded back on read.
constexpr double kNanSentinel = -1.0e308;
double enc(double v) { return std::isnan(v) ? kNanSentinel : v; }
double dec(double v) { return v <= -1.0e307 ? nan() : v; }

double readD(const cv::FileNode &n, const char *key, double def = 0.0) {
    const cv::FileNode c = n[key];
    return c.empty() ? def : static_cast<double>(c);
}
int readI(const cv::FileNode &n, const char *key, int def = 0) {
    const cv::FileNode c = n[key];
    return c.empty() ? def : static_cast<int>(c);
}
std::string readS(const cv::FileNode &n, const char *key, const std::string &def = std::string()) {
    const cv::FileNode c = n[key];
    return c.empty() ? def : static_cast<std::string>(c);
}
std::vector<double> readVec(const cv::FileNode &n, const char *key) {
    std::vector<double> v;
    const cv::FileNode c = n[key];
    if (!c.empty()) c >> v;
    return v;
}

// Points travel as ONE flat number list per block instead of a mapping per
// point: a 1000-corner panel is a few lines of YAML this way and a few thousand
// the other, and the stride is documented next to each writer.
void writeFlat(cv::FileStorage &fs, const char *key, const std::vector<double> &v) {
    fs << key << v;
}

void writePoints2f(cv::FileStorage &fs, const char *key, const std::vector<cv::Point2f> &pts) {
    std::vector<double> flat;
    flat.reserve(pts.size() * 2);
    for (const auto &p : pts) {
        flat.push_back(p.x);
        flat.push_back(p.y);
    }
    writeFlat(fs, key, flat);
}

std::vector<cv::Point2f> readPoints2f(const cv::FileNode &n, const char *key) {
    const std::vector<double> flat = readVec(n, key);
    std::vector<cv::Point2f> pts;
    for (size_t i = 0; i + 1 < flat.size(); i += 2)
        pts.emplace_back(static_cast<float>(flat[i]), static_cast<float>(flat[i + 1]));
    return pts;
}

void writeMetricMap(cv::FileStorage &fs, const char *key, const std::map<std::string, double> &m) {
    fs << key << "{";
    for (const auto &kv : m) fs << kv.first << enc(kv.second);
    fs << "}";
}

std::map<std::string, double> readMetricMap(const cv::FileNode &n, const char *key) {
    std::map<std::string, double> m;
    const cv::FileNode c = n[key];
    if (c.empty() || !c.isMap()) return m;
    for (cv::FileNodeIterator it = c.begin(); it != c.end(); ++it)
        m[(*it).name()] = dec(static_cast<double>(*it));
    return m;
}

// 3D points: parallel arrays. "dirs" holds one direction per point, "vals" holds
// 12 numbers per point: id, mid xyz, p xyz, q xyz, ray_gap, d_mid_to_center.
void writePoints3d(cv::FileStorage &fs, const char *key, const std::vector<Point3d> &pts) {
    fs << key << "{";
    std::vector<std::string> dirs;
    std::vector<double> vals;
    dirs.reserve(pts.size());
    vals.reserve(pts.size() * 12);
    for (const auto &p : pts) {
        dirs.push_back(p.direction);
        vals.push_back(p.pointId);
        for (int i = 0; i < 3; ++i) vals.push_back(p.mid[i]);
        for (int i = 0; i < 3; ++i) vals.push_back(p.p[i]);
        for (int i = 0; i < 3; ++i) vals.push_back(p.q[i]);
        vals.push_back(enc(p.rayGap));
        vals.push_back(enc(p.dMidToCenter));
    }
    fs << "dirs" << dirs;
    writeFlat(fs, "vals", vals);
    fs << "}";
}

std::vector<Point3d> readPoints3d(const cv::FileNode &n, const char *key) {
    std::vector<Point3d> out;
    const cv::FileNode c = n[key];
    if (c.empty()) return out;
    std::vector<std::string> dirs;
    if (!c["dirs"].empty()) c["dirs"] >> dirs;
    const std::vector<double> vals = readVec(c, "vals");
    const size_t stride = 12;
    for (size_t i = 0; i < dirs.size() && (i + 1) * stride <= vals.size(); ++i) {
        const double *v = &vals[i * stride];
        Point3d p;
        p.direction = dirs[i];
        p.pointId = static_cast<int>(v[0]);
        p.mid = Vec3(v[1], v[2], v[3]);
        p.p = Vec3(v[4], v[5], v[6]);
        p.q = Vec3(v[7], v[8], v[9]);
        p.rayGap = dec(v[10]);
        p.dMidToCenter = dec(v[11]);
        out.push_back(p);
    }
    return out;
}

// Reprojection rows: "dirs" plus 6 numbers per row -- id, u, v, u_gt, v_gt, error.
void writeReproj(cv::FileStorage &fs, const char *key, const std::vector<ReprojRow> &rows) {
    fs << key << "{";
    std::vector<std::string> dirs;
    std::vector<double> vals;
    for (const auto &r : rows) {
        dirs.push_back(r.direction);
        vals.push_back(r.pointId);
        vals.push_back(enc(r.u));
        vals.push_back(enc(r.v));
        vals.push_back(enc(r.uGt));
        vals.push_back(enc(r.vGt));
        vals.push_back(enc(r.error));
    }
    fs << "dirs" << dirs;
    writeFlat(fs, "vals", vals);
    fs << "}";
}

std::vector<ReprojRow> readReproj(const cv::FileNode &n, const char *key) {
    std::vector<ReprojRow> out;
    const cv::FileNode c = n[key];
    if (c.empty()) return out;
    std::vector<std::string> dirs;
    if (!c["dirs"].empty()) c["dirs"] >> dirs;
    const std::vector<double> vals = readVec(c, "vals");
    const size_t stride = 6;
    for (size_t i = 0; i < dirs.size() && (i + 1) * stride <= vals.size(); ++i) {
        const double *v = &vals[i * stride];
        ReprojRow r;
        r.direction = dirs[i];
        r.pointId = static_cast<int>(v[0]);
        r.u = dec(v[1]);
        r.v = dec(v[2]);
        r.uGt = dec(v[3]);
        r.vGt = dec(v[4]);
        r.error = dec(v[5]);
        out.push_back(r);
    }
    return out;
}

// Detections: 7 numbers per corner -- id, x_rect, y_rect, x_fish, y_fish, alpha, beta.
void writeDetPoints(cv::FileStorage &fs, const std::vector<DetPoint> &pts) {
    std::vector<double> vals;
    vals.reserve(pts.size() * 7);
    for (const auto &p : pts) {
        vals.push_back(p.pointId);
        vals.push_back(p.xRect);
        vals.push_back(p.yRect);
        vals.push_back(p.xFish);
        vals.push_back(p.yFish);
        vals.push_back(enc(p.alpha));
        vals.push_back(enc(p.beta));
    }
    writeFlat(fs, "points", vals);
}

std::vector<DetPoint> readDetPoints(const cv::FileNode &n, const std::string &direction) {
    std::vector<DetPoint> out;
    const std::vector<double> vals = readVec(n, "points");
    const size_t stride = 7;
    for (size_t i = 0; (i + 1) * stride <= vals.size(); ++i) {
        const double *v = &vals[i * stride];
        DetPoint p;
        p.direction = direction;
        p.pointId = static_cast<int>(v[0]);
        p.xRect = v[1];
        p.yRect = v[2];
        p.xFish = static_cast<int>(v[3]);
        p.yFish = static_cast<int>(v[4]);
        p.alpha = dec(v[5]);
        p.beta = dec(v[6]);
        out.push_back(p);
    }
    return out;
}

// Recovered ORI_DET grid: 4 numbers per corner -- row, col, x, y.
void writePanelDetection(cv::FileStorage &fs, const char *key,
                         const PanelCorner::PanelDetection &det) {
    fs << key << "{";
    fs << "rows" << det.rows << "cols" << det.cols;
    std::vector<double> vals;
    vals.reserve(det.corners.size() * 4);
    for (const auto &c : det.corners) {
        vals.push_back(c.row);
        vals.push_back(c.col);
        vals.push_back(c.pt.x);
        vals.push_back(c.pt.y);
    }
    writeFlat(fs, "corners", vals);
    fs << "}";
}

PanelCorner::PanelDetection readPanelDetection(const cv::FileNode &n, const char *key) {
    PanelCorner::PanelDetection det;
    const cv::FileNode c = n[key];
    if (c.empty()) return det;
    det.rows = readI(c, "rows");
    det.cols = readI(c, "cols");
    const std::vector<double> vals = readVec(c, "corners");
    for (size_t i = 0; i + 3 < vals.size(); i += 4) {
        PanelCorner::Corner cn;
        cn.row = static_cast<int>(vals[i]);
        cn.col = static_cast<int>(vals[i + 1]);
        cn.pt = cv::Point2f(static_cast<float>(vals[i + 2]), static_cast<float>(vals[i + 3]));
        det.corners.push_back(cn);
    }
    return det;
}

}  // namespace

std::string Session::summary() const {
    int detected = 0;
    for (const auto &cam : detections)
        for (const auto &dir : cam.second) detected += static_cast<int>(dir.second.points.size());
    int oriPlanesDone = 0;
    for (const auto &p : oriPlanes)
        if (p.clicks[0].size() == 4 && p.clicks[1].size() == 4) ++oriPlanesDone;

    std::ostringstream os;
    os << cameras.size() << " camera(s)";
    if (detected) os << ", " << detected << " detected point(s)";
    if (oriPlanesDone) os << ", " << oriPlanesDone << " ORI_DET plane(s)";
    if (!points3d.empty()) os << ", " << points3d.size() << " 3D point(s)";
    if (!oriPoints3d.empty()) os << ", " << oriPoints3d.size() << " ORI_DET 3D point(s)";
    return os.str();
}

std::string autosaveSessionPath() {
    return getOutputDir("session") + "/measure3d_autosave.yaml";
}

namespace {

// The writer proper. Kept apart from saveSession() so the public entry point can
// be a plain try/catch: cv::FileStorage answers a path it cannot make sense of --
// an extension outside .yaml/.json/.xml, which the save dialog's "All Files"
// filter lets the user type -- by throwing, not by staying closed, and that
// exception would otherwise unwind through the Qt slot that asked for the save.
bool writeSession(const Session &s, const std::string &path) {
    cv::FileStorage fs(path, cv::FileStorage::WRITE);
    if (!fs.isOpened()) return false;

    fs << "schema_version" << s.schemaVersion;
    fs << "saved_at" << s.savedAt;
    fs << "two_camera" << (s.twoCamera ? 1 : 0);

    // ---- inputs ----
    fs << "cameras" << "[";
    for (const auto &c : s.cameras) {
        fs << "{";
        fs << "side" << c.side;
        fs << "image_path" << c.imagePath;
        fs << "parameter_path" << c.parameterPath;
        fs << "image_bytes" << static_cast<double>(c.imageBytes);
        fs << "image_mtime" << static_cast<double>(c.imageMtime);
        fs << "position" << std::vector<double>{c.posX, c.posY, c.posZ};
        fs << "preprocess" << c.preprocess;
        fs << "directions" << "[";
        for (const auto &d : c.directions) {
            fs << "{";
            fs << "name" << d.direction << "alpha" << d.alpha << "beta" << d.beta << "zoom"
               << d.zoom << "pattern" << d.pattern << "reorder" << d.reorder;
            fs << "}";
        }
        fs << "]";
        fs << "}";
    }
    fs << "]";

    // ---- detections ----
    fs << "detections" << "[";
    for (const auto &cam : s.detections)
        for (const auto &dir : cam.second) {
            const DetectionResult &d = dir.second;
            fs << "{";
            fs << "camera" << cam.first << "direction" << dir.first;
            fs << "found" << (d.found ? 1 : 0) << "expected" << d.expected << "detected"
               << d.detected << "fish_w" << d.fishW << "fish_h" << d.fishH;
            writeDetPoints(fs, d.points);
            fs << "}";
        }
    fs << "]";

    // ---- anypoint result ----
    fs << "result_anypoint" << "{";
    writePoints3d(fs, "points", s.points3d);
    fs << "camera_left" << std::vector<double>{s.camL3d[0], s.camL3d[1], s.camL3d[2]};
    fs << "camera_right" << std::vector<double>{s.camR3d[0], s.camR3d[1], s.camR3d[2]};
    writeMetricMap(fs, "angle", s.angle);
    writeMetricMap(fs, "inter_ray", s.interRay);
    writeMetricMap(fs, "thickness", s.thickness);
    writeMetricMap(fs, "depth", s.depth);
    fs << "reproj_mean_left" << enc(s.meanErrLeft) << "reproj_rms_left" << enc(s.rmsLeft)
       << "reproj_mean_right" << enc(s.meanErrRight) << "reproj_rms_right" << enc(s.rmsRight);
    fs << "}";

    // ---- ORI_DET ----
    fs << "ori_det" << "{";
    fs << "method" << s.oriMethod << "rows_hint" << s.oriRowsHint << "cols_hint" << s.oriColsHint
       << "index_mode" << s.oriIndexMode << "step_plane" << s.oriStepPlane << "step_camera"
       << s.oriStepCam;
    fs << "planes" << "[";
    for (const auto &p : s.oriPlanes) {
        fs << "{";
        fs << "plane" << p.plane;
        writePoints2f(fs, "clicks_left", p.clicks[0]);
        writePoints2f(fs, "clicks_right", p.clicks[1]);
        writePanelDetection(fs, "recovered_left", p.recovered[0]);
        writePanelDetection(fs, "recovered_right", p.recovered[1]);
        writePoints2f(fs, "boundary_left", p.boundary[0]);
        writePoints2f(fs, "boundary_right", p.boundary[1]);
        fs << "}";
    }
    fs << "]";

    // Whole-image survey layers, kept so the comparison is still on screen after
    // a restore (they are pure diagnostics, but re-running costs seconds).
    static const char *kSurveyKey[2] = {"survey_left", "survey_right"};
    for (int side = 0; side < 2; ++side) {
        fs << kSurveyKey[side] << "[";
        for (const auto &l : s.survey[side]) {
            fs << "{";
            fs << "name" << l.name << "boards" << l.boards << "ms" << l.ms << "structured"
               << (l.structured ? 1 : 0);
            writePoints2f(fs, "points", l.points);
            fs << "}";
        }
        fs << "]";
    }
    writeReproj(fs, "reproj_left", s.oriReproj[0]);
    writeReproj(fs, "reproj_right", s.oriReproj[1]);

    writePoints3d(fs, "points_3d", s.oriPoints3d);
    fs << "camera_left" << std::vector<double>{s.oriCamL[0], s.oriCamL[1], s.oriCamL[2]};
    fs << "camera_right" << std::vector<double>{s.oriCamR[0], s.oriCamR[1], s.oriCamR[2]};
    writeMetricMap(fs, "angle", s.oriAngle);
    writeMetricMap(fs, "inter_ray", s.oriInterRay);
    writeMetricMap(fs, "thickness", s.oriThickness);
    writeMetricMap(fs, "depth", s.oriDepth);
    fs << "reproj_mean_left" << enc(s.oriMeanErrLeft) << "reproj_rms_left" << enc(s.oriRmsLeft)
       << "reproj_mean_right" << enc(s.oriMeanErrRight) << "reproj_rms_right"
       << enc(s.oriRmsRight);
    fs << "}";

    fs.release();
    return true;
}

}  // namespace

bool saveSession(const Session &s, const std::string &path) {
    try {
        return writeSession(s, path);
    } catch (const cv::Exception &) {
        return false;  // unusable path, full disk: the caller reports it, nothing throws
    }
}

bool loadSession(Session &s, const std::string &path) {
    cv::FileStorage fs;
    try {
        if (!fs.open(path, cv::FileStorage::READ)) return false;
    } catch (const cv::Exception &) {
        return false;  // corrupt or truncated file: leave `s` untouched
    }

    s = Session();
    try {
        const cv::FileNode root = fs.root();
        s.schemaVersion = readI(root, "schema_version", 1);
        s.savedAt = readS(root, "saved_at");
        s.twoCamera = readI(root, "two_camera", 1) != 0;

        for (const auto &n : root["cameras"]) {
            SessionCamera c;
            c.side = readS(n, "side");
            c.imagePath = readS(n, "image_path");
            c.parameterPath = readS(n, "parameter_path");
            c.imageBytes = static_cast<long long>(readD(n, "image_bytes"));
            c.imageMtime = static_cast<long long>(readD(n, "image_mtime"));
            const std::vector<double> pos = readVec(n, "position");
            if (pos.size() == 3) {
                c.posX = pos[0];
                c.posY = pos[1];
                c.posZ = pos[2];
            }
            c.preprocess = readS(n, "preprocess", "standard");
            for (const auto &dn : n["directions"]) {
                SessionDirection d;
                d.direction = readS(dn, "name");
                d.alpha = readD(dn, "alpha");
                d.beta = readD(dn, "beta");
                d.zoom = readD(dn, "zoom", 4.0);
                d.pattern = readS(dn, "pattern");
                d.reorder = readS(dn, "reorder", "default");
                c.directions.push_back(d);
            }
            s.cameras.push_back(c);
        }

        for (const auto &n : root["detections"]) {
            const std::string cam = readS(n, "camera");
            const std::string dir = readS(n, "direction");
            if (cam.empty() || dir.empty()) continue;
            DetectionResult d;
            d.direction = dir;
            d.found = readI(n, "found") != 0;
            d.expected = readI(n, "expected");
            d.detected = readI(n, "detected");
            d.fishW = readI(n, "fish_w");
            d.fishH = readI(n, "fish_h");
            d.points = readDetPoints(n, dir);
            s.detections[cam][dir] = d;
        }

        const cv::FileNode ap = root["result_anypoint"];
        if (!ap.empty()) {
            s.points3d = readPoints3d(ap, "points");
            const std::vector<double> l = readVec(ap, "camera_left");
            const std::vector<double> r = readVec(ap, "camera_right");
            if (l.size() == 3) s.camL3d = Vec3(l[0], l[1], l[2]);
            if (r.size() == 3) s.camR3d = Vec3(r[0], r[1], r[2]);
            s.angle = readMetricMap(ap, "angle");
            s.interRay = readMetricMap(ap, "inter_ray");
            s.thickness = readMetricMap(ap, "thickness");
            s.depth = readMetricMap(ap, "depth");
            s.meanErrLeft = dec(readD(ap, "reproj_mean_left", kNanSentinel));
            s.rmsLeft = dec(readD(ap, "reproj_rms_left", kNanSentinel));
            s.meanErrRight = dec(readD(ap, "reproj_mean_right", kNanSentinel));
            s.rmsRight = dec(readD(ap, "reproj_rms_right", kNanSentinel));
        }

        const cv::FileNode od = root["ori_det"];
        if (!od.empty()) {
            s.oriMethod = readI(od, "method");
            s.oriRowsHint = readI(od, "rows_hint", 6);
            s.oriColsHint = readI(od, "cols_hint", 9);
            s.oriIndexMode = readI(od, "index_mode");
            s.oriStepPlane = readI(od, "step_plane");
            s.oriStepCam = readI(od, "step_camera");
            for (const auto &n : od["planes"]) {
                SessionOriPlane p;
                p.plane = readS(n, "plane");
                if (p.plane.empty()) continue;
                p.clicks[0] = readPoints2f(n, "clicks_left");
                p.clicks[1] = readPoints2f(n, "clicks_right");
                p.recovered[0] = readPanelDetection(n, "recovered_left");
                p.recovered[1] = readPanelDetection(n, "recovered_right");
                p.boundary[0] = readPoints2f(n, "boundary_left");
                p.boundary[1] = readPoints2f(n, "boundary_right");
                s.oriPlanes.push_back(p);
            }
            static const char *kSurveyKey[2] = {"survey_left", "survey_right"};
            for (int side = 0; side < 2; ++side)
                for (const auto &n : od[kSurveyKey[side]]) {
                    SessionSurveyLayer l;
                    l.name = readS(n, "name");
                    l.boards = readI(n, "boards");
                    l.ms = readD(n, "ms");
                    l.structured = readI(n, "structured") != 0;
                    l.points = readPoints2f(n, "points");
                    s.survey[side].push_back(l);
                }
            s.oriReproj[0] = readReproj(od, "reproj_left");
            s.oriReproj[1] = readReproj(od, "reproj_right");
            s.oriPoints3d = readPoints3d(od, "points_3d");
            const std::vector<double> l = readVec(od, "camera_left");
            const std::vector<double> r = readVec(od, "camera_right");
            if (l.size() == 3) s.oriCamL = Vec3(l[0], l[1], l[2]);
            if (r.size() == 3) s.oriCamR = Vec3(r[0], r[1], r[2]);
            s.oriAngle = readMetricMap(od, "angle");
            s.oriInterRay = readMetricMap(od, "inter_ray");
            s.oriThickness = readMetricMap(od, "thickness");
            s.oriDepth = readMetricMap(od, "depth");
            s.oriMeanErrLeft = dec(readD(od, "reproj_mean_left", kNanSentinel));
            s.oriRmsLeft = dec(readD(od, "reproj_rms_left", kNanSentinel));
            s.oriMeanErrRight = dec(readD(od, "reproj_mean_right", kNanSentinel));
            s.oriRmsRight = dec(readD(od, "reproj_rms_right", kNanSentinel));
        }
    } catch (const cv::Exception &) {
        // A malformed section must not lose the sections already read.
    }
    fs.release();
    return true;
}

}  // namespace Measure3d
