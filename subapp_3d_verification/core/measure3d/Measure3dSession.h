#pragma once

#include <array>
#include <map>
#include <string>
#include <vector>

#include <opencv2/core.hpp>

#include "Measure3dTypes.h"
#include "PanelCornerRecovery.h"

// Session cache for the 3D-measurement dialog: everything the user built up in
// one serialisable value, written as YAML (cv::FileStorage picks the format from
// the extension, so .json and .xml work too).
//
// What is NOT in here: the images. Only their paths, size and modified-time are
// stored -- embedding a pair of 3040x3040 frames would add ~25 MB per save, and
// the path plus a size/mtime check is enough to notice that the file moved or
// was replaced.
//
// What IS in here: every input (paths, camera positions, per-direction angles,
// pattern sizes), every measurement (detections, ORI_DET clicks and recovered
// corner grids, the whole-image survey), and the derived results (3D points and
// their metrics) so a restored session shows its numbers without recomputing.
namespace Measure3d {

// One (camera, direction) row of the anypoint tab's controls.
struct SessionDirection {
    std::string direction;
    double alpha = 0, beta = 0, zoom = 4;
    std::string pattern;  // the "poin" field text, e.g. "6x5"
    std::string reorder;
};

struct SessionCamera {
    std::string side;  // "left" / "right"
    std::string imagePath, parameterPath;
    long long imageBytes = 0, imageMtime = 0;  // to notice the file changed
    double posX = 0, posY = 0, posZ = 0;
    std::string preprocess;  // "standard" / "enhanced"
    std::vector<SessionDirection> directions;
};

struct SessionOriPlane {
    std::string plane;
    std::array<std::vector<cv::Point2f>, 2> clicks;          // the 4 corners, per camera
    std::array<PanelCorner::PanelDetection, 2> recovered;    // detected inner-corner grid
    std::array<std::vector<cv::Point2f>, 2> boundary;        // curved outline
};

struct SessionSurveyLayer {
    std::string name;
    std::vector<cv::Point2f> points;
    int boards = 0;
    double ms = 0;
    bool structured = false;
};

struct Session {
    int schemaVersion = 1;
    std::string savedAt;  // "yyyy-MM-dd hh:mm:ss", shown in the restore prompt

    bool twoCamera = true;  // the "2 camera" radio, which gates Start Calculation
    std::vector<SessionCamera> cameras;
    std::map<std::string, std::map<std::string, DetectionResult>> detections;  // [cam][dir]

    // ---- anypoint result ----
    std::vector<Point3d> points3d;
    Vec3 camL3d{Vec3::Zero()}, camR3d{Vec3::Zero()};
    std::map<std::string, double> angle, interRay, thickness, depth;
    double meanErrLeft = nan(), rmsLeft = nan(), meanErrRight = nan(), rmsRight = nan();

    // ---- ORI_DET ----
    int oriMethod = 0, oriRowsHint = 6, oriColsHint = 9, oriIndexMode = 0;
    int oriStepPlane = 0, oriStepCam = 0;  // where the click workflow had got to
    std::vector<SessionOriPlane> oriPlanes;
    std::array<std::vector<SessionSurveyLayer>, 2> survey;
    std::array<std::vector<ReprojRow>, 2> oriReproj;

    std::vector<Point3d> oriPoints3d;
    Vec3 oriCamL{Vec3::Zero()}, oriCamR{Vec3::Zero()};
    std::map<std::string, double> oriAngle, oriInterRay, oriThickness, oriDepth;
    double oriMeanErrLeft = nan(), oriRmsLeft = nan(), oriMeanErrRight = nan(),
           oriRmsRight = nan();

    bool empty() const {
        return cameras.empty() && detections.empty() && oriPlanes.empty() && points3d.empty();
    }
    // One line for the restore prompt, e.g. "2 images, 60 detected points, 5 ORI_DET planes".
    std::string summary() const;
};

bool saveSession(const Session &s, const std::string &path);
bool loadSession(Session &s, const std::string &path);

// image_cali/output_3D/session/measure3d_autosave.yaml -- the slot the dialog
// writes on close and after each calculation, and offers to restore on open.
std::string autosaveSessionPath();

}  // namespace Measure3d
