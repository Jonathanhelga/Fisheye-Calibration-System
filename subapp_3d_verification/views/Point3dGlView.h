#pragma once

#include <array>
#include <string>
#include <vector>

#include <QOpenGLFunctions>
#include <QOpenGLWidget>
#include <QPoint>
#include <QPointF>
#include <QWidget>

#include <Eigen/Dense>

#include "Measure3dTypes.h"
#include "PlaneFit3dViz.h"

class GlTextOverlay;  // transparent child widget that draws the axis letters
class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;

// GPU canvas for the triangulated checkerboard points. Fixed-function OpenGL
// (compatibility profile), mouse navigation: drag = orbit, wheel = zoom.
//
// Every direction that has at least 3 points also gets its fitted plane drawn
// (translucent patch + grid + normal stub + residual drop lines), from the same
// PCA/SVD fit the angle and thickness metrics come from -- so whichever way the
// points were produced (anypoint Detect or the ORI_DET 4-point panels), the
// result shows the plane each direction was measured against.
//
// The scene carries no standing names: the X/Y/Z letters at the axis tips, plus
// the info box of whichever point is hovered or pinned (which has to stay next
// to that point to identify it). The fixed names -- cameras, baseline centre,
// origin, plane names -- are listed by the side panel of the Point3dGlView that
// owns this canvas.
//
// Navigation is unrestricted: the orientation is a quaternion turned by dragging
// (a trackball), so the scene can be rolled right over the top/bottom -- there is
// no pole to get stuck at. Left drag = orbit, right/middle drag = pan, wheel =
// zoom. Hover a point to describe it, double-click to pin it, double-click empty
// space to clear the pin. Pinning draws the green baseline-centre -> mid line,
// and with P/Q shown also the red Camera L -> P and blue Camera R -> Q lines.
class Point3dGlCanvas : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT
public:
    // Canned viewpoints. The axis ones put the eye on that world axis looking
    // back at the data, which is how a plane is checked for flatness: sight down
    // a plane's own normal and it fills the view, sight across it and it
    // collapses to a line.
    enum class ViewPreset {
        Isometric,  // the opening three-quarter view
        PlusX,      // eye on +X, looking -X (shows the Y-Z plane)
        MinusX,
        PlusY,
        MinusY,
        PlusZ,
        MinusZ,
        Cameras,  // from the baseline centre, i.e. roughly what the rig sees
    };

    // What the mouse is over: the triangulated mid point, or -- when the P/Q
    // display is on -- the closest point on the left (P) or right (Q) camera ray.
    enum class PickKind { Mid, P, Q };
    struct Highlight {
        int idx = -1;  // index into points(); -1 = nothing
        PickKind kind = PickKind::Mid;
    };

    explicit Point3dGlCanvas(QWidget *parent = nullptr);

    void setPoints(const std::vector<Measure3d::Point3d> &pts, const Eigen::Vector3d &camL,
                   const Eigen::Vector3d &camR);
    void resetView();
    void setPreset(ViewPreset preset);  // orientation + refit, keeps the data framed
    void fitToScene();                  // refit distance/target, keep the orientation
    void focusHighlighted();            // zoom onto the pinned/hovered point (else fit)
    void setPlanesVisible(bool on);
    bool planesVisible() const { return showPlanes_; }
    // P/Q = where each camera's ray came closest to the other's. Showing them
    // exposes the triangulation itself: the P--mid--Q segment is the shortest
    // inter-ray line, and its length is that point's ray gap.
    void setPqVisible(bool on);
    bool pqVisible() const { return showPq_; }

    // Field-of-view cones. The angles come from the loaded camera parameter
    // JSONs ("cameraFov"), which is also what the Moildev SDK enforces: a ray is
    // only defined while |alpha| <= fov/2, alpha being the angle off the optical
    // axis. The ring is that alpha limit drawn at the board's distance, so a
    // board sitting outside a camera's ring is a board that camera cannot see.
    void setCameraFov(double fovLeftDeg, double fovRightDeg);
    double parameterFovLeft() const { return fovParamL_; }
    double parameterFovRight() const { return fovParamR_; }
    void setFovRingsVisible(bool on);
    bool fovRingsVisible() const { return showFov_; }
    // Full cone angle to draw, in degrees; <= 0 means "each camera's own
    // parameter FOV".
    void setFovRingAngle(double deg);
    double fovRingAngle() const { return fovAngle_; }

    // Read-only view of the scene, for the information bar.
    const std::vector<Measure3d::Point3d> &points() const { return pts_; }
    const std::vector<PlaneFit3dViz::DirPlane> &planes() const { return planes_; }
    const Eigen::Vector3d &cameraL() const { return camL_; }
    const Eigen::Vector3d &cameraR() const { return camR_; }
    Highlight highlighted() const;  // pinned entity, else hovered entity, else idx -1

signals:
    void sceneChanged();  // new points/planes loaded (the side panel follows this)

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void resizeEvent(QResizeEvent *e) override;  // keep the overlay sized to us
    void paintGL() override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseDoubleClickEvent(QMouseEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;

private:
    void frameScene();
    bool projectToScreen(const Eigen::Vector3d &world, QPointF &out) const;
    Highlight pickEntity(const QPointF &pos) const;  // nearest mid/P/Q under the cursor
    Eigen::Vector3d entityPos(const Highlight &h) const;
    void rebuildOverlay();  // recompute the axis-letter screen positions after a paint
    void drawPlanes();      // translucent fitted plane per direction
    void drawPq();          // P/Q markers + the shortest inter-ray line through mid
    void drawFovRings();    // per-camera FOV cone + ring at the board distance
    // Deepest SIDE plane (every direction except "center") seen from a camera,
    // measured along the optical axis. That depth is the ring's radius, and both
    // cameras share the larger of the two so the two rings come out the same size.
    double sidePlaneMaxDepth(const Eigen::Vector3d &cam) const;

    std::vector<Measure3d::Point3d> pts_;
    std::vector<PlaneFit3dViz::DirPlane> planes_;  // one per direction, refit in setPoints
    Eigen::Vector3d dataCenter_{Eigen::Vector3d::Zero()};  // centroid of the points
    double fovRingDepth_ = 0;  // ring radius (deepest side plane), shared by both cameras
    bool showPlanes_ = true;
    bool showPq_ = false;
    bool showFov_ = false;
    double fovParamL_ = 0, fovParamR_ = 0;  // from the camera parameter JSONs
    double fovAngle_ = 0;                   // drawn cone angle; <= 0 = per-camera parameter

    Eigen::Vector3d camL_{Eigen::Vector3d::Zero()};
    Eigen::Vector3d camR_{Eigen::Vector3d::Zero()};

    Eigen::Vector3d center_{Eigen::Vector3d::Zero()};  // orbit target (moved by panning)
    double sceneRadius_ = 100.0;

    // Free orientation: no yaw/pitch angles, so no clamp and no gimbal pole.
    Eigen::Quaterniond rot_{Eigen::Quaterniond::Identity()};
    double dist_ = 400.0;

    QPoint lastMouse_;
    bool orbiting_ = false;
    bool panning_ = false;
    Highlight hovered_;
    Highlight selected_;

    GlTextOverlay *overlay_ = nullptr;

    // Transforms captured at the end of paintGL for projecting labels + picking.
    double mvCache_[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    double prCache_[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    int vpCache_[4] = {0, 0, 1, 1};
    double dprCache_ = 1.0;
};

// In-app 3D result view: an information panel on the LEFT and the GL canvas
// filling the rest. This is the widget the dialogs embed (both the anypoint
// "3D View" tab and the ORI_DET result tab), so both 3D methods get the same
// scene and the same panel.
//
// The panel holds the naming that would otherwise sit permanently on top of the
// geometry -- camera L/R, the baseline centre, the origin, one colour-keyed
// entry per fitted plane -- plus the view controls. Point details stay in the
// scene, beside the point being hovered or pinned.
class Point3dGlView : public QWidget {
    Q_OBJECT
public:
    explicit Point3dGlView(QWidget *parent = nullptr);

    void setPoints(const std::vector<Measure3d::Point3d> &pts, const Eigen::Vector3d &camL,
                   const Eigen::Vector3d &camR);
    // Camera FOV in degrees, straight from each camera's parameter JSON. Seeds
    // the FOV input (until the user types their own angle) and is reported in
    // the information panel.
    void setCameraFov(double fovLeftDeg, double fovRightDeg);
    void resetView();
    void setPlanesVisible(bool on);
    bool planesVisible() const;

private:
    void refreshSceneInfo();  // cameras / centre / origin / plane + FOV legend

    Point3dGlCanvas *canvas_ = nullptr;
    QCheckBox *planeToggle_ = nullptr;
    QPushButton *pqToggle_ = nullptr;   // checkable: show every P/Q pair
    QPushButton *fovToggle_ = nullptr;  // checkable: show the FOV rings
    QDoubleSpinBox *fovSpin_ = nullptr;
    bool fovEdited_ = false;  // user typed an angle: stop following the parameters
    QLabel *sceneInfo_ = nullptr;
};
