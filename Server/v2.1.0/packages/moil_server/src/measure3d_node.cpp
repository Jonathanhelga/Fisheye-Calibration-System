// Loading a camera and looking through it. See measure3d_node_p.h for what is
// in the other half.

#include "measure3d_node.h"

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

#include "measure3d_node_p.h"

using measure3d_detail::decode;
using measure3d_detail::encodePng;

namespace measure3d_detail {

QByteArray encodePng(const cv::Mat &m, int maxSide) {
    if (m.empty()) return {};
    cv::Mat out = m;
    const int side = std::max(m.cols, m.rows);
    if (maxSide > 0 && side > maxSide) {
        const double s = static_cast<double>(maxSide) / side;
        cv::resize(m, out, cv::Size(), s, s, cv::INTER_AREA);
    }
    std::vector<unsigned char> buf;
    cv::imencode(".png", out, buf);
    return QByteArray(reinterpret_cast<const char *>(buf.data()), static_cast<int>(buf.size()));
}

cv::Mat decode(const sensor_msgs::msg::CompressedImage &img) {
    if (img.data.empty()) return {};
    const cv::Mat buf(1, static_cast<int>(img.data.size()), CV_8U,
                      const_cast<unsigned char *>(img.data.data()));
    return cv::imdecode(buf, cv::IMREAD_COLOR);
}

}  // namespace measure3d_detail

namespace {

std::string mapKey(double pitch, double yaw, double zoom, int mode, int w, int h) {
    char buf[128];
    std::snprintf(buf, sizeof(buf), "%.4f/%.4f/%.4f/%d/%dx%d", pitch, yaw, zoom, mode, w, h);
    return buf;
}

// Moildev loads from a file path, so the JSON the client sent is staged into the
// server's own working area. Kept rather than deleted: when a 3D result looks
// wrong, the first question is which optics produced it, and the answer should
// still be on disk.
QString stageParams(const std::string &position, const std::string &json) {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) +
                        QStringLiteral("/measure3d");
    QDir().mkpath(dir);
    const QString path = dir + QStringLiteral("/%1_params.json").arg(QString::fromStdString(position));
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return {};
    f.write(QByteArray::fromStdString(json));
    return path;
}

}  // namespace

Measure3dNode::Measure3dNode(ServerContext &ctx, const rclcpp::NodeOptions &options)
    : ServerNode("moil_measure3d", ctx, options) {
    addService<SourceSrv>("/measure3d/source", &Measure3dNode::onSource);
    addService<AnypointSrv>("/measure3d/anypoint", &Measure3dNode::onAnypoint);
    addService<DetectSrv>("/measure3d/detect", &Measure3dNode::onDetect);
    addService<VerifySrv>("/measure3d/verify", &Measure3dNode::onVerify);
    // Center Setup's two views. Served here rather than in a node of their own
    // because they are the same lens model, the same remap cache and the same
    // Moildev instances the 3D work uses.
    addService<PanoramaSrv>("/moildev/panorama", &Measure3dNode::onPanorama);
    addService<InfoSrv>("/moildev/info", &Measure3dNode::onInfo);

    addDetachedAction<AutoFrame>("/measure3d/auto_frame", &Measure3dNode::executeAutoFrame);

    RCLCPP_INFO(get_logger(), "measure3d: two-camera fisheye verification");
}

// Any non-empty id names a source. "left" and "right" are the 3D dialog's
// convention and the only ones Verify3d looks for; other windows use their own
// (Center Setup uses "center_setup"), which keeps their reloads from disturbing
// the two the 3D dialog is holding detections against.
Measure3dNode::Source *Measure3dNode::sourceFor(const std::string &position) {
    if (position.empty()) return nullptr;
    return &sources_[position];
}

Measure3dNode::Source *Measure3dNode::liveSource(const std::string &paramsJson,
                                                 const std::string &cameraType,
                                                 double resolutionRatio, bool hasCenter, int icx,
                                                 int icy, QString *err) {
    // Key the source on everything that changes the optics. Center Setup moves the
    // centre with a spin box, and every move rebuilds the lens model -- so a stale
    // remap cache would draw the previous centre's rings over the new one's image,
    // which is exactly the misalignment the operator is there to find.
    const std::string key = "live/" + cameraType + "/" +
                            std::to_string(resolutionRatio) + "/" +
                            (hasCenter ? std::to_string(icx) + "," + std::to_string(icy) : "-");

    if (key != liveKey_) {
        sources_.erase(liveKey_);
        liveKey_ = key;
    }
    Source &src = sources_[key];

    if (!src.moil.valid() || src.paramsJson != paramsJson) {
        const QString path = stageParams("live", paramsJson);
        if (path.isEmpty() || !src.moil.load(path.toStdString(), cameraType,
                                             resolutionRatio > 0 ? resolutionRatio : 1.0)) {
            if (err) *err = QStringLiteral("camera parameter JSON did not load");
            return nullptr;
        }
        src.paramsJson = paramsJson;
        src.maps.clear();
    }
    if (hasCenter) src.moil.setCenter(icx, icy);

    // The frame is grabbed fresh each call: this is a live view of the rig, and a
    // cached frame would show the operator the aim they had before they moved.
    if (ctx_.camera) {
        cv::Mat f = ctx_.camera->frame();
        if (!f.empty()) {
            const int w = src.moil.imageWidth(), h = src.moil.imageHeight();
            // The lens model is calibrated at a particular resolution; the frame
            // must be expressed in that same grid or every radius is wrong by the
            // ratio between them.
            if (f.cols != w || f.rows != h) cv::resize(f, f, cv::Size(w, h));
            src.fisheyeBgr = std::move(f);
        }
    }
    if (src.fisheyeBgr.empty()) {
        if (err) *err = QStringLiteral("no frame from the camera");
        return nullptr;
    }
    return &src;
}

void Measure3dNode::onInfo(const InfoSrv::Request &req, InfoSrv::Response &res) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Load into a scratch Moildev rather than the live one: this is a question
    // about a parameter file, and answering it should not disturb a view the
    // operator is currently looking at.
    Moildev moil;
    const QString path = stageParams("info", req.params_json);
    if (path.isEmpty() || !moil.load(path.toStdString(), req.camera_type, 1.0) || !moil.valid()) {
        res.success = false;
        res.message = "camera parameter JSON did not load";
        return;
    }
    res.native_width = moil.imageWidth();
    res.native_height = moil.imageHeight();

    const double ratio = req.resolution_ratio > 0 ? req.resolution_ratio : 1.0;
    if (ratio != 1.0) moil.load(path.toStdString(), req.camera_type, ratio);
    if (req.has_center) moil.setCenter(req.icx, req.icy);

    res.width = moil.imageWidth();
    res.height = moil.imageHeight();
    res.camera_fov = moil.cameraFov();
    res.icx = moil.icx();
    res.icy = moil.icy();

    std::vector<double> alphas(req.alphas.begin(), req.alphas.end());
    if (alphas.empty())
        for (double a = 10.0; a <= moil.cameraFov() / 2.0 + 1e-9; a += 10.0) alphas.push_back(a);

    for (double a : alphas) {
        const double rho = moil.getRhoFromAlpha(a);
        // Negative means "outside the lens model" -- the client draws nothing
        // rather than a ring at a radius the optics do not support.
        res.rho.push_back(rho > 0 ? rho : -1.0);
    }
    res.success = true;
    res.message = "";
}

void Measure3dNode::onPanorama(const PanoramaSrv::Request &req, PanoramaSrv::Response &res) {
    std::lock_guard<std::mutex> lock(mutex_);

    QString err;
    Source *src = nullptr;
    if (req.source_id == "capture" || req.source_id.empty())
        src = liveSource(req.params_json, req.camera_type, req.resolution_ratio,
                         req.has_center, req.icx, req.icy, &err);
    else
        src = sourceFor(req.source_id);

    if (!src || !src->loaded()) {
        res.success = false;
        res.message = err.isEmpty() ? "source is not loaded" : err.toStdString();
        return;
    }

    const double alphaMax =
        req.alpha_max > 0 ? req.alpha_max : std::round(src->moil.cameraFov() / 2.0);

    cv::Mat mapX, mapY, pano;
    src->moil.mapsPanoramaCar(alphaMax, 0, 0, false, mapX, mapY);
    cv::remap(src->fisheyeBgr, pano, mapX, mapY, cv::INTER_LINEAR);

    // The unwrap is tall and thin and the horizon is what is being inspected, so
    // it is drawn wide. Stretched here rather than by the client, so the image
    // that arrives is the one shown instead of being resampled a second time.
    const double stretch = req.x_stretch > 0 ? req.x_stretch : 2.0;
    if (stretch != 1.0)
        cv::resize(pano, pano, cv::Size(static_cast<int>(pano.cols * stretch), pano.rows));

    res.width = pano.cols;
    res.height = pano.rows;
    const QByteArray png = encodePng(pano, req.max_side);
    res.image.header.stamp = now();
    res.image.format = "png";
    res.image.data.assign(png.begin(), png.end());
    res.success = true;
    res.message = "";
}

// ------------------------------------------------------------------ sources ----

void Measure3dNode::onSource(const SourceSrv::Request &req, SourceSrv::Response &res) {
    std::lock_guard<std::mutex> lock(mutex_);
    Source *src = sourceFor(req.position);
    if (!src) {
        res.success = false;
        res.message = "position must not be empty";
        return;
    }

    if (!req.image.data.empty()) {
        cv::Mat img = decode(req.image);
        if (img.empty()) {
            res.success = false;
            res.message = "could not decode the fisheye image";
            return;
        }
        src->fisheyeBgr = std::move(img);
        src->imageName = req.image_name;
        // A different image means the cached remaps are for a different picture
        // only if the SIZE changed -- but the detections are of the old picture
        // either way, so they go.
        src->detections.clear();
    }

    int invalidated = 0;
    if (!req.params_json.empty()) {
        const QString path = stageParams(req.position, req.params_json);
        const double ratio = req.resolution_ratio > 0 ? req.resolution_ratio : 1.0;
        if (path.isEmpty() ||
            !src->moil.load(path.toStdString(), req.camera_type, ratio)) {
            res.success = false;
            res.message = "camera parameter JSON did not load";
            return;
        }
        // The lens model is calibrated at a particular resolution. If the image and
        // the model disagree about the grid, every radius and every alpha is wrong
        // by the ratio between them -- and nothing about the picture looks wrong.
        if (!src->fisheyeBgr.empty()) {
            const int w = src->moil.imageWidth(), h = src->moil.imageHeight();
            if (src->fisheyeBgr.cols != w || src->fisheyeBgr.rows != h)
                cv::resize(src->fisheyeBgr, src->fisheyeBgr, cv::Size(w, h));
        }
        src->paramsJson = req.params_json;
        src->paramsName = req.params_name;
        // alpha and beta are computed THROUGH the optics. Detections made under
        // the previous parameters describe a different camera, and mixing them
        // into one triangulation is the failure this dialog is most able to hide
        // -- so they are dropped, counted, and reported.
        invalidated = static_cast<int>(src->detections.size());
        src->detections.clear();
        src->maps.clear();
    }

    if (!src->loaded()) {
        res.success = false;
        res.message = "this position still needs both an image and a parameter file";
        return;
    }

    res.success = true;
    res.source_id = req.position;
    res.width = src->fisheyeBgr.cols;
    res.height = src->fisheyeBgr.rows;
    res.invalidated = invalidated;
    res.message = invalidated > 0
                       ? std::to_string(invalidated) +
                             " detection(s) dropped: they were made with the previous parameters"
                       : "";
}

cv::Mat Measure3dNode::anypointView(Source &src, double pitch, double yaw, double zoom, int mode,
                                    int outW, int outH, cv::Mat *gray, cv::Mat *mapXOut,
                                    cv::Mat *mapYOut) {
    if (!src.loaded()) return {};
    const int w = outW > 0 ? outW : src.fisheyeBgr.cols;
    const int h = outH > 0 ? outH : src.fisheyeBgr.rows;
    const std::string key = mapKey(pitch, yaw, zoom, mode, w, h);

    auto it = src.maps.find(key);
    if (it == src.maps.end()) {
        cv::Mat mx, my;
        if (mode == 1)
            src.moil.mapsAnypointMode1(pitch, yaw, zoom, mx, my);
        else
            src.moil.mapsAnypointMode2(pitch, yaw, zoom, mx, my, w, h);
        it = src.maps.emplace(key, std::make_pair(std::move(mx), std::move(my))).first;
    }
    const cv::Mat &mapX = it->second.first;
    const cv::Mat &mapY = it->second.second;
    if (mapXOut) *mapXOut = mapX;
    if (mapYOut) *mapYOut = mapY;

    cv::Mat bgr;
    cv::remap(src.fisheyeBgr, bgr, mapX, mapY, cv::INTER_CUBIC);
    if (gray) cv::cvtColor(bgr, *gray, cv::COLOR_BGR2GRAY);
    return bgr;
}

void Measure3dNode::onAnypoint(const AnypointSrv::Request &req, AnypointSrv::Response &res) {
    std::lock_guard<std::mutex> lock(mutex_);

    // "capture" draws from the live camera, which is what Center Setup wants: the
    // operator is aiming the real rig, not inspecting an old file. It goes through
    // liveSource so a moved optical centre rebuilds the lens model and drops the
    // stale remaps with it.
    QString err;
    Source *src = nullptr;
    if (req.source_id == "capture")
        src = liveSource(req.params_json, req.camera_type, req.resolution_ratio,
                         req.has_center, req.icx, req.icy, &err);
    else
        src = sourceFor(req.source_id);

    if (!src || !src->loaded()) {
        res.success = false;
        res.message = err.isEmpty() ? ("source \"" + req.source_id + "\" is not loaded")
                                     : err.toStdString();
        return;
    }
    const cv::Mat view = anypointView(*src, req.alpha, req.beta, req.zoom, req.mode,
                                      req.width, req.height, nullptr, nullptr, nullptr);
    res.width = view.cols;
    res.height = view.rows;
    const QByteArray png = encodePng(view, req.max_side);
    res.image.header.stamp = now();
    res.image.format = "png";
    res.image.data.assign(png.begin(), png.end());
    res.success = !view.empty();
    res.message = "";
}
