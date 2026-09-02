#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <string>

#include <QString>

#include "server_node.h"

#include <opencv2/core.hpp>

#include "Measure3dTypes.h"
#include "Moildev.h"
#include "moil_interfaces/action/auto_frame.hpp"
#include "moil_interfaces/srv/anypoint.hpp"
#include "moil_interfaces/srv/detect_checkerboard.hpp"
#include "moil_interfaces/srv/measure3d_source.hpp"
#include "moil_interfaces/srv/moildev_info.hpp"
#include "moil_interfaces/srv/panorama.hpp"
#include "moil_interfaces/srv/verify3d.hpp"

struct ServerContext;

// /moil_measure3d -- the two-camera fisheye 3D verification.
//
// The whole pipeline, which in v2.0 ran inside a QDialog on the operator's
// machine: build the anypoint remap for a (camera, direction), detect the
// checkerboard on it, map every corner back through the remap to its (alpha,
// beta) on the fisheye, triangulate the matched left/right corners, fit a plane
// per direction, and reproject to measure the error.
//
// The expensive part is mapsAnypointMode2 -- a dense per-pixel transform of a
// 3040x3040 image, per direction, per camera. It was already cached in the client
// because recomputing it for the same angles was the difference between a
// responsive dialog and an unusable one. The cache moves here with it, keyed the
// same way, so asking twice for the same view costs an encode and nothing else.
//
// State is per source ("left"/"right") and lives for as long as the server does
// or until the operator loads a different image. Replacing the camera PARAMETERS
// invalidates every detection made under the old ones: alpha and beta are
// computed through the optics, so keeping detections across a parameter change
// would silently mix two calibrations into one 3D result.
class Measure3dNode : public ServerNode {
public:
    Measure3dNode(ServerContext &ctx, const rclcpp::NodeOptions &options = rclcpp::NodeOptions());

private:
    using SourceSrv = moil_interfaces::srv::Measure3dSource;
    using AnypointSrv = moil_interfaces::srv::Anypoint;
    using DetectSrv = moil_interfaces::srv::DetectCheckerboard;
    using VerifySrv = moil_interfaces::srv::Verify3d;
    using PanoramaSrv = moil_interfaces::srv::Panorama;
    using InfoSrv = moil_interfaces::srv::MoildevInfo;
    using AutoFrame = moil_interfaces::action::AutoFrame;

    // One loaded camera: its fisheye image, its optics, its remap cache and what
    // has been detected on it.
    struct Source {
        cv::Mat fisheyeBgr;
        Moildev moil;
        std::string paramsJson;
        std::string imageName, paramsName;

        // key -> (mapX, mapY), key built from the angles + zoom + mode. This is
        // the cache that makes the dialog usable.
        std::map<std::string, std::pair<cv::Mat, cv::Mat>> maps;

        // direction -> detection
        std::map<std::string, Measure3d::DetectionResult> detections;

        bool loaded() const { return !fisheyeBgr.empty() && moil.valid(); }
    };

    void onSource(const SourceSrv::Request &req, SourceSrv::Response &res);
    void onAnypoint(const AnypointSrv::Request &req, AnypointSrv::Response &res);
    void onDetect(const DetectSrv::Request &req, DetectSrv::Response &res);
    void onVerify(const VerifySrv::Request &req, VerifySrv::Response &res);
    void onPanorama(const PanoramaSrv::Request &req, PanoramaSrv::Response &res);
    void onInfo(const InfoSrv::Request &req, InfoSrv::Response &res);
    void executeAutoFrame(const std::shared_ptr<rclcpp_action::ServerGoalHandle<AutoFrame>> gh);

    Source *sourceFor(const std::string &position);

    // Center Setup works on the LIVE camera rather than a loaded file, and it
    // reloads its Moildev whenever the operator moves the centre or changes the
    // working resolution. Kept as its own source so those reloads cannot disturb
    // the two the 3D dialog is holding detections against.
    //
    // Returns null when the parameter JSON does not load.
    Source *liveSource(const std::string &paramsJson, const std::string &cameraType,
                       double resolutionRatio, bool hasCenter, int icx, int icy, QString *err);

    // Build (or fetch from the cache) the remap for one view, and apply it.
    // `gray` receives the grayscale the detector wants; the BGR return is for
    // display. Empty when the source is not loaded.
    cv::Mat anypointView(Source &src, double pitch, double yaw, double zoom, int mode, int outW,
                         int outH, cv::Mat *gray, cv::Mat *mapX, cv::Mat *mapY);

    // Run the detector for one (camera, direction) at these angles, store the
    // result on the source and fill the overlay. Returns whether a board was found.
    bool detectInto(Source &src, const std::string &camKey, const std::string &direction,
                    double pitch, double yaw, double zoom, int mode, int cols, int rows,
                    const std::string &reorder, const std::string &preprocess,
                    cv::Mat *overlay);

    // "left" and "right". Guarded because a detect and a verify can arrive at
    // once, and both walk the same detections map.
    std::map<std::string, Source> sources_;
    std::mutex mutex_;

    // The live-camera source Center Setup drives. See liveSource().
    std::string liveKey_;
};
