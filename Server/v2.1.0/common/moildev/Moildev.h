#pragma once

#include <array>
#include <string>
#include <vector>

#include <opencv2/core.hpp>

// Native C++ port of mvc_model/moildev/Moildev.py (the MOIL fisheye SDK).
//
// The Python wrapper builds two lookup tables (alpha<->rho) in pure Python and
// delegates the heavy anypoint-map generation to the native MoilCV library
// (moildev/dll/MoilCV.cpp). Both pieces are ported here verbatim so the whole
// pixel -> (alpha, beta) fisheye mapping runs with no Python and no external
// .so: initAlphaRhoTable() + getAlphaBeta() reproduce Moildev.py, and
// mapsAnypointMode2() reproduces MoilCV::AnyPointM2.
//
// A camera-parameter JSON (the MOIL calibration result) either has the fields
// at the top level (with a "cameraName" key) or nested under a camera_type key.
class Moildev {
public:
    Moildev() = default;
    // Loads the *.json camera parameter file. camera_type selects a nested
    // block when the JSON holds several cameras; empty => top-level. Returns
    // false (and leaves valid()==false) when the file is missing/incomplete.
    explicit Moildev(const std::string &fileCameraParameter,
                     const std::string &cameraType = std::string(),
                     double resolutionRatio = 1.0);

    bool load(const std::string &fileCameraParameter,
              const std::string &cameraType = std::string(),
              double resolutionRatio = 1.0);

    bool valid() const { return valid_; }

    // --- calibration accessors (post resolution-ratio scaling) ---
    int icx() const { return icx_; }
    int icy() const { return icy_; }
    // Override the fisheye centre at runtime (same resolution as icx()/icy()).
    // Only anypoint-map generation and getAlphaBeta depend on the centre, so no
    // alpha/rho table rebuild is needed. Used by the Setup-Center tool.
    void setCenter(int icx, int icy) { icx_ = icx; icy_ = icy; }
    int imageWidth() const { return imageWidth_; }
    int imageHeight() const { return imageHeight_; }
    double cameraFov() const { return cameraFov_; }
    const std::string &cameraName() const { return cameraName_; }

    // get_alpha_from_rho / get_rho_from_alpha (Moildev.py). rho table is float
    // (polynomial * calibration_ratio); alpha in degrees.
    double getAlphaFromRho(int rho) const;
    double getRhoFromAlpha(double alpha) const;

    // get_alpha_beta(x, y, mode). Returns {alpha, beta} in degrees; sets ok
    // false when the coordinate is outside the covered fisheye range (Python
    // returns None, None).
    std::pair<double, double> getAlphaBeta(double x, double y, int mode, bool &ok) const;

    // get_coordinate_from_alpha_beta(alpha, beta, mode). Sets ok false when out
    // of range (Python returns None, None).
    std::pair<int, int> getCoordinateFromAlphaBeta(double alpha, double beta, int mode,
                                                   bool &ok) const;

    // maps_anypoint_mode1(alpha, beta, zoom): tube-mode anypoint. Ported from
    // MoilCV::AnyPointM. Fills mapX/mapY (CV_32F, imageHeight x imageWidth).
    void mapsAnypointMode1(double alpha, double beta, double zoom, cv::Mat &mapX,
                           cv::Mat &mapY) const;

    // maps_anypoint_mode2(pitch, yaw, zoom): fills mapX/mapY (CV_32F, size
    // imageHeight x imageWidth) for cv::remap. Ported from MoilCV::AnyPointM2.
    void mapsAnypointMode2(double pitch, double yaw, double zoom, cv::Mat &mapX,
                           cv::Mat &mapY) const;

    // Same anypoint view but rendered onto a smaller outW x outH grid (the
    // destination is subsampled, the field of view is unchanged). The map values
    // still index into the full-resolution fisheye source, so remap the FULL
    // source. Used by the auto-framing search to try many zoom/angle candidates
    // cheaply before committing to a full-resolution detection.
    void mapsAnypointMode2(double pitch, double yaw, double zoom, cv::Mat &mapX, cv::Mat &mapY,
                           int outW, int outH) const;

    // maps_panorama_car(alphaMax, icAlphaDeg, icBetaDeg, flip): equirectangular
    // panorama. Ported from MoilCV::PanoramaCar. Map size imageHeight x imageWidth.
    void mapsPanoramaCar(double alphaMax, double icAlphaDeg, double icBetaDeg, bool flip,
                         cv::Mat &mapX, cv::Mat &mapY) const;

    // Convenience: remap `image` to an anypoint (mode 2) view.
    cv::Mat anypointMode2(const cv::Mat &image, double pitch, double yaw, double zoom) const;

private:
    void initAlphaRhoTable();

    bool valid_ = false;

    std::string cameraName_;
    double cameraFov_ = 220.0;
    double sensorWidth_ = 0, sensorHeight_ = 0;
    int icx_ = 0, icy_ = 0;
    double ratio_ = 1.0;
    int imageWidth_ = 0, imageHeight_ = 0;
    double calibrationRatio_ = 1.0;
    double p0_ = 0, p1_ = 0, p2_ = 0, p3_ = 0, p4_ = 0, p5_ = 0;
    double resolutionRatio_ = 1.0;

    // Python builds these at 1800*3 / 3600*3 resolution (see Moildev.py).
    std::vector<double> alphaToRho_;  // size 5400, indexed by alpha*10 (deg)
    std::vector<int> rhoToAlpha_;     // size >= 10800, indexed by rho
};
