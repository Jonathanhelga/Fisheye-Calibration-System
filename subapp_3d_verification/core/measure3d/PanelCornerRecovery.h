#pragma once

#include <array>
#include <vector>

#include <Eigen/Core>
#include <opencv2/core.hpp>

class Moildev;

// Port of chessboard_grid.recover_panel_corners (single-image homography path).
//
// Given the four outermost-INNER corner clicks of a chessboard panel (in
// TL, TR, BR, BL order) and the panel's inner-corner grid size, recover every
// inner corner as a sub-pixel position on the ORIGINAL (fisheye) image. A plain
// 4-point homography only approximates the curved fisheye manifold, so the
// recovery runs: refine clicks -> homography prediction -> coarse cornerSubPix
// (adaptive window) -> RANSAC homography re-fit from the coarse corners ->
// fine cornerSubPix. This keeps sub-pixel accuracy in the original image (no
// anypoint round-trip), which is exactly what getAlphaBeta wants.
namespace PanelCorner {

struct Corner {
    int row = 0;
    int col = 0;
    cv::Point2f pt;  // sub-pixel position in the original image
};

struct PanelDetection {
    int rows = 0;  // detected inner-corner grid (0 = detection failed)
    int cols = 0;
    std::vector<Corner> corners;  // row-major, canonical (row,col) indices
};

// Detect the chessboard DIRECTLY on the original image, inside the region bounded
// by the 4 clicks (no rectification, no anypoint remap). Crops the padded
// bounding box of the clicks and runs findChessboardCornersSB there, auto-
// discovering the grid size. Corner (row,col) indices are assigned by a
// homography of the 4 clicks (labelling only -- detection stays on the original),
// so left/right pair reliably. Empty PanelDetection if no board is found.
PanelDetection detectPanelOriginal(const cv::Mat &image, const std::array<cv::Point2f, 4> &outer,
                                   int hintRows = 0, int hintCols = 0);

// Which corner finder runs inside the panel region.
//   *Sb -- OpenCV findChessboardCornersSB: fast, wants a globally regular grid.
//   *Cb -- the libcbdetect port (Geiger et al. 2012, see CbDetect.h): slower,
//          but grows the board from local predictions and so tolerates
//          fisheye distortion the SB detector will not accept.
//
// Both work on the pixels the camera produced. Nothing here resamples the image:
// every corner is measured once, on the data. (An earlier version could
// re-project a panel onto a flat tangent plane through the camera model before
// detecting. It was removed: it interpolates, so corners were measured on
// resampled pixels; it silently depends on the camera JSON matching the image;
// and on real rig frames it found one panel out of five where libcbdetect on the
// original found all five.)
enum class Method {
    Auto = 0,  // OriginalCb -> OriginalSb, first hit wins
    OriginalSb,
    OriginalCb,
};
const char *methodName(Method m);

// libcbdetect straight on the original image, inside the clicked region. Needs
// no camera model and no grid size.
//
// cropToQuad (default) keeps only the sub-grid the four corners span, which is
// what the click workflow wants: the quad is the user saying "this panel", and
// the crop trims a board that grew past it or a neighbour that got detected too.
// Pass false when the quad was derived from the board itself (the survey), where
// cropping can only discard corners the full-resolution pass legitimately found.
PanelDetection detectPanelOriginalCb(const cv::Mat &image, const std::array<cv::Point2f, 4> &outer,
                                     int hintRows = 0, int hintCols = 0, bool cropToQuad = true);

// Single entry point used by the UI: runs `method`, or the cascade above for
// Method::Auto. cropToQuad is only meaningful for the libcbdetect path (see
// above); the SB path never trims to the quad.
PanelDetection recoverPanel(const cv::Mat &image, const std::array<cv::Point2f, 4> &outer,
                            int hintRows, int hintCols, Method method, bool cropToQuad = true);

// ---------------------------------------------------------------------------
// Whole-image survey: what every method finds, with no clicking at all.
//
// libcbdetect needs neither a grid size nor a region, so it can be turned loose
// on the whole frame to find every board in it. Each board it finds then hands
// its four outer corners to every other method as if the user had clicked them,
// so all of them can be compared on the same panels, in one pass, over the
// whole field of view. The first layer is the raw corner candidates from before
// structure recovery -- the difference between that layer and the others is
// exactly the diagnosis when corners near a panel edge go missing: present in
// layer 0 means the board growth dropped them, absent means the corner itself
// was never measurable there.
// ---------------------------------------------------------------------------
struct SurveyLayer {
    std::string name;
    std::vector<cv::Point2f> points;  // every corner this layer produced
    int boards = 0;
    double ms = 0;
    bool structured = false;  // false for the raw-candidate layer
};

std::vector<SurveyLayer> surveyImage(const cv::Mat &image, int maxDetectPixels = 4000000);

// `image` may be gray or BGR. Returns the inner-corner grid row-major; may be a
// subset of rows*cols if some corners fall outside the image margin. Empty on
// hard failure (bad size, homography ill-conditioned, or all corners outside).
std::vector<Corner> recoverPanelCorners(const cv::Mat &image,
                                        const std::array<cv::Point2f, 4> &outer, int rows, int cols,
                                        bool refine = true, int subpixWindow = 7);

// Stereo, fisheye-correct panel recovery (port of
// recover_panel_corners_stereo_fisheye). A homography cannot flatten fisheye
// curvature, so instead: triangulate the 4 clicks to 3D (using both cameras),
// bilinear-interpolate the plane in 3D, then PROJECT every inner corner through
// each camera's fisheye model. The projected corners (and the sampled boundary)
// follow the real fisheye curvature. Needs the grid size (rows,cols) up front
// (get it from detectPanel) and correct camera positions (baseline).
struct StereoPanel {
    bool ok = false;
    PanelDetection left, right;                            // recovered corners per camera
    std::vector<cv::Point2f> boundaryLeft, boundaryRight;  // curved quad outline (orig px)
    std::array<Eigen::Vector3d, 4> corners3d{};           // TL,TR,BR,BL in 3D
};
StereoPanel recoverPanelStereoFisheye(const cv::Mat &imgL, const cv::Mat &imgR,
                                      const std::array<cv::Point2f, 4> &outerL,
                                      const std::array<cv::Point2f, 4> &outerR, int rows, int cols,
                                      const Moildev &moilL, const Moildev &moilR,
                                      const Eigen::Vector3d &camL, const Eigen::Vector3d &camR);

}  // namespace PanelCorner
