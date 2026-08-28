#include "Point3dGlView.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <utility>

#include <QCheckBox>
#include <QColor>
#include <QDoubleSpinBox>
#include <QFont>
#include <QFontMetrics>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QString>
#include <QStringList>
#include <QSurfaceFormat>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QWidget>

// ======================================================================
// Transparent overlay that paints the axis letters and the hovered/pinned
// point's info box on top of the GL scene. A real QWidget's paintEvent renders
// text reliably on all drivers, unlike QPainter-over-QOpenGLWidget which was
// silently dropping the labels. The fixed names (cameras, baseline centre,
// origin, plane names) live in the side panel instead of here, so only the box
// that has to be next to its point is drawn over the geometry.
// ======================================================================
class GlTextOverlay : public QWidget {
public:
    struct Item {
        QPointF pos;
        QString text;
        QColor color;
        bool boxed = false;  // multi-line info box vs. a single-line label
    };

    explicit GlTextOverlay(QWidget *parent) : QWidget(parent) {
        setAttribute(Qt::WA_TransparentForMouseEvents);  // let the GL view get the mouse
        setAttribute(Qt::WA_NoSystemBackground);
        setAttribute(Qt::WA_TranslucentBackground);
    }

    void setItems(std::vector<Item> items) {
        items_ = std::move(items);
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        QFont lf = p.font();
        lf.setBold(true);
        lf.setPointSize(11);

        for (const auto &it : items_) {
            if (!it.boxed) {
                p.setFont(lf);
                p.setPen(it.color);
                p.drawText(it.pos, it.text);
                continue;
            }
            // Boxed multi-line info, kept next to the point it describes.
            QFont bf = p.font();
            bf.setBold(false);
            bf.setPointSize(10);
            p.setFont(bf);
            const QStringList lines = it.text.split('\n');
            const QFontMetrics fm(bf);
            int tw = 0;
            for (const auto &l : lines) tw = std::max(tw, fm.horizontalAdvance(l));
            const int th = fm.height() * static_cast<int>(lines.size());
            QRectF box(it.pos.x() + 12, it.pos.y() - th - 12, tw + 14, th + 10);
            // Keep the box inside the widget.
            if (box.right() > width()) box.moveRight(it.pos.x() - 12);
            if (box.top() < 0) box.moveTop(it.pos.y() + 12);
            p.fillRect(box, QColor(255, 255, 255, 235));
            p.setPen(QColor(110, 110, 110));
            p.drawRect(box);
            p.setPen(it.color);
            p.drawText(box.adjusted(7, 5, -5, -5), Qt::AlignLeft | Qt::AlignTop, it.text);
        }
    }

private:
    std::vector<Item> items_;
};

// ======================================================================

namespace {

std::array<float, 3> colorForDir(const std::string &dir) {
    if (dir == "center") return {0.0f, 0.0f, 0.0f};  // black
    if (dir == "north") return {0.85f, 0.6f, 0.0f};  // dark yellow
    if (dir == "south") return {0.9f, 0.35f, 0.0f};  // orange
    if (dir == "west") return {0.0f, 0.6f, 0.0f};    // green
    if (dir == "east") return {0.55f, 0.15f, 0.8f};  // purple
    return {0.4f, 0.4f, 0.4f};
}

QColor qColorForDir(const std::string &dir) {
    const auto c = colorForDir(dir);
    return QColor(static_cast<int>(c[0] * 255), static_cast<int>(c[1] * 255),
                  static_cast<int>(c[2] * 255));
}

constexpr double kFovDeg = 45.0;
constexpr double kDegToRad = M_PI / 180.0;

// The opening orientation (the old yaw 35 / pitch 20 view), as a quaternion.
Eigen::Quaterniond kDefaultRotation() {
    return Eigen::Quaterniond(Eigen::AngleAxisd(20.0 * kDegToRad, Eigen::Vector3d::UnitX()) *
                              Eigen::AngleAxisd(35.0 * kDegToRad, Eigen::Vector3d::UnitY()));
}

QString fmtVec(const Eigen::Vector3d &v) {
    return QString("(%1, %2, %3)")
        .arg(v.x(), 0, 'f', 1)
        .arg(v.y(), 0, 'f', 1)
        .arg(v.z(), 0, 'f', 1);
}

// "<b><span …>name</span></b> : value" -- one entry of the information panel.
// The colon keeps a name and its number from reading as one phrase ("center
// 14.64 mm"). `value` is our own formatted markup (numbers, an optional <br>);
// only the name is escaped.
QString infoEntry(const QString &name, const QColor &color, const QString &value) {
    return QString("<b><span style='color:%1'>%2 :</span></b> %3")
        .arg(color.name(), name.toHtmlEscaped(), value);
}

}  // namespace

// ======================================================================
// Point3dGlCanvas -- the GL scene
// ======================================================================

Point3dGlCanvas::Point3dGlCanvas(QWidget *parent) : QOpenGLWidget(parent) {
    QSurfaceFormat fmt;
    fmt.setProfile(QSurfaceFormat::CompatibilityProfile);
    fmt.setDepthBufferSize(24);
    setFormat(fmt);
    setMouseTracking(true);
    setMinimumSize(320, 240);
    setCursor(Qt::OpenHandCursor);
    overlay_ = new GlTextOverlay(this);
    overlay_->setGeometry(rect());
    overlay_->raise();
    rot_ = kDefaultRotation();
}

void Point3dGlCanvas::setPoints(const std::vector<Measure3d::Point3d> &pts,
                                const Eigen::Vector3d &camL, const Eigen::Vector3d &camR) {
    pts_ = pts;
    camL_ = camL;
    camR_ = camR;
    hovered_ = {};
    selected_ = {};
    // Refit here rather than at draw time: the fit is per result, not per frame.
    // Normals face the baseline centre, matching the metrics/HTML convention.
    planes_ = PlaneFit3dViz::fitPlanesPerDirection(pts_, (camL_ + camR_) * 0.5);
    dataCenter_ = Eigen::Vector3d::Zero();
    if (!pts_.empty()) {
        for (const auto &p : pts_) dataCenter_ += p.mid;
        dataCenter_ /= static_cast<double>(pts_.size());
    }
    frameScene();
    // ONE ring radius for both cameras, computed once per result rather than per
    // frame. Measuring each camera separately gave the two rings different radii,
    // which reads as "the cameras differ" when it is only the geometry of the
    // measurement; the deeper of the two still reaches every side plane.
    fovRingDepth_ = std::max(sidePlaneMaxDepth(camL_), sidePlaneMaxDepth(camR_));
    update();
    emit sceneChanged();
}

void Point3dGlCanvas::setPlanesVisible(bool on) {
    if (showPlanes_ == on) return;
    showPlanes_ = on;
    update();
}

void Point3dGlCanvas::setPqVisible(bool on) {
    if (showPq_ == on) return;
    showPq_ = on;
    if (!showPq_) {  // P/Q are gone: any highlight on one of them must go too
        if (hovered_.kind != PickKind::Mid) hovered_ = {};
        if (selected_.kind != PickKind::Mid) selected_ = {};
    }
    update();
}

void Point3dGlCanvas::setCameraFov(double fovLeftDeg, double fovRightDeg) {
    fovParamL_ = fovLeftDeg > 0 ? fovLeftDeg : 0;
    fovParamR_ = fovRightDeg > 0 ? fovRightDeg : 0;
    if (showFov_) update();
    emit sceneChanged();  // the panel prints the parameter FOVs
}

void Point3dGlCanvas::setFovRingsVisible(bool on) {
    if (showFov_ == on) return;
    showFov_ = on;
    update();
}

void Point3dGlCanvas::setFovRingAngle(double deg) {
    if (std::abs(fovAngle_ - deg) < 1e-9) return;
    fovAngle_ = deg;
    if (showFov_) update();
}

Point3dGlCanvas::Highlight Point3dGlCanvas::highlighted() const {
    const Highlight h = (selected_.idx >= 0) ? selected_ : hovered_;
    return (h.idx >= 0 && h.idx < static_cast<int>(pts_.size())) ? h : Highlight{};
}

Eigen::Vector3d Point3dGlCanvas::entityPos(const Highlight &h) const {
    if (h.idx < 0 || h.idx >= static_cast<int>(pts_.size())) return Eigen::Vector3d::Zero();
    const Measure3d::Point3d &p = pts_[h.idx];
    switch (h.kind) {
        case PickKind::P: return p.p;
        case PickKind::Q: return p.q;
        default: return p.mid;
    }
}

void Point3dGlCanvas::frameScene() {
    Eigen::Vector3d lo(1e300, 1e300, 1e300), hi(-1e300, -1e300, -1e300);
    auto grow = [&](const Eigen::Vector3d &p) {
        lo = lo.cwiseMin(p);
        hi = hi.cwiseMax(p);
    };
    for (const auto &p : pts_) grow(p.mid);
    grow(camL_);
    grow(camR_);
    grow(Eigen::Vector3d::Zero());  // keep the origin in view (axes live there)
    if (lo.x() > hi.x()) {
        center_ = Eigen::Vector3d::Zero();
        sceneRadius_ = 100.0;
    } else {
        center_ = (lo + hi) * 0.5;
        sceneRadius_ = std::max(1.0, (hi - lo).norm() * 0.5);
    }
    dist_ = sceneRadius_ / std::tan(kFovDeg * kDegToRad / 2.0) * 1.4;
}

void Point3dGlCanvas::resetView() {
    rot_ = kDefaultRotation();
    frameScene();  // also undoes any panning (center_ back on the data)
    update();
}

void Point3dGlCanvas::fitToScene() {
    frameScene();  // orientation untouched: re-frame what is already on screen
    update();
}

void Point3dGlCanvas::focusHighlighted() {
    const Highlight h = highlighted();
    if (h.idx < 0) {  // nothing pinned or hovered -> behave like Fit
        fitToScene();
        return;
    }
    center_ = entityPos(h);                      // orbit around the point being studied
    dist_ = std::max(1.0, sceneRadius_ * 0.25);  // close enough to read its neighbours
    update();
}

void Point3dGlCanvas::setPreset(ViewPreset preset) {
    frameScene();  // re-centre first: the presets aim at the data, not at a pan

    Eigen::Vector3d forward;  // eye -> target
    switch (preset) {
        case ViewPreset::PlusX: forward = -Eigen::Vector3d::UnitX(); break;
        case ViewPreset::MinusX: forward = Eigen::Vector3d::UnitX(); break;
        case ViewPreset::PlusY: forward = -Eigen::Vector3d::UnitY(); break;
        case ViewPreset::MinusY: forward = Eigen::Vector3d::UnitY(); break;
        case ViewPreset::PlusZ: forward = -Eigen::Vector3d::UnitZ(); break;
        case ViewPreset::MinusZ: forward = Eigen::Vector3d::UnitZ(); break;
        case ViewPreset::Cameras: {
            const Eigen::Vector3d eye = (camL_ + camR_) * 0.5;
            forward = center_ - eye;
            if (forward.norm() < 1e-9) forward = -Eigen::Vector3d::UnitZ();
            break;
        }
        case ViewPreset::Isometric:
        default:
            rot_ = kDefaultRotation();
            update();
            return;
    }
    forward.normalize();

    // lookAt basis: rows of the world->camera rotation are (right, up, -forward).
    // Z is the screen-up axis, so the ±X views stand the scene up the same way
    // the ±Y views do (Z up, depth across) instead of laying Z on its side. Only
    // the ±Z views, where Z IS the view direction, fall back to Y up.
    Eigen::Vector3d up = Eigen::Vector3d::UnitZ();
    if (std::abs(forward.dot(up)) > 0.99) up = Eigen::Vector3d::UnitY();
    const Eigen::Vector3d rightAxis = forward.cross(up).normalized();
    const Eigen::Vector3d upAxis = rightAxis.cross(forward);
    Eigen::Matrix3d R;
    R.row(0) = rightAxis.transpose();
    R.row(1) = upAxis.transpose();
    R.row(2) = -forward.transpose();
    rot_ = Eigen::Quaterniond(R).normalized();
    update();
}

void Point3dGlCanvas::initializeGL() {
    initializeOpenGLFunctions();
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_POINT_SMOOTH);
    glEnable(GL_LINE_SMOOTH);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void Point3dGlCanvas::resizeGL(int w, int h) { glViewport(0, 0, w, std::max(1, h)); }

void Point3dGlCanvas::resizeEvent(QResizeEvent *e) {
    QOpenGLWidget::resizeEvent(e);
    if (overlay_) overlay_->setGeometry(rect());
}

void Point3dGlCanvas::paintGL() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    const double aspect = static_cast<double>(width()) / std::max(1, height());
    // Generous near/far: the user can pan the target away from the data, and the
    // clip planes must not start slicing the scene when they do.
    const double nearP = std::max(dist_ * 0.005, dist_ - sceneRadius_ * 4.0);
    const double farP = dist_ + sceneRadius_ * 12.0 + 1000.0;
    const double top = nearP * std::tan(kFovDeg * kDegToRad / 2.0);
    const double right = top * aspect;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-right, right, -top, top, nearP, farP);

    // view = translate(-dist on Z) * rot_ * translate(-center_). rot_ is a free
    // quaternion, so there is no pitch clamp and no pole to jam against.
    Eigen::Matrix4d rot = Eigen::Matrix4d::Identity();
    rot.topLeftCorner<3, 3>() = rot_.toRotationMatrix();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslated(0.0, 0.0, -dist_);
    glMultMatrixd(rot.data());  // Eigen is column-major, same as OpenGL
    glTranslated(-center_.x(), -center_.y(), -center_.z());

    // ---- axes through the WORLD ORIGIN (0,0,0) ----
    const double L = center_.norm() + sceneRadius_ * 1.2;  // long enough to cross the scene
    glLineWidth(1.5f);
    glBegin(GL_LINES);
    glColor3f(0.85f, 0.1f, 0.1f);  // X red
    glVertex3d(-L, 0, 0);
    glVertex3d(L, 0, 0);
    glColor3f(0.1f, 0.6f, 0.1f);  // Y green
    glVertex3d(0, -L, 0);
    glVertex3d(0, L, 0);
    glColor3f(0.1f, 0.2f, 0.9f);  // Z blue
    glVertex3d(0, 0, -L);
    glVertex3d(0, 0, L);
    glEnd();

    // ---- connector lines from a double-clicked point ----
    // Each line ends where it physically belongs: the LEFT camera's ray ends at
    // P, the RIGHT camera's at Q, and the baseline centre connects to the mid
    // point. With P/Q hidden there is no P or Q to point at, so only the green
    // centre -> mid line is drawn. Pinning a P or a Q shows the same set: the
    // lines describe the whole triangulation of that point, not one end of it.
    const Eigen::Vector3d baseCenter = (camL_ + camR_) * 0.5;
    if (selected_.idx >= 0 && selected_.idx < static_cast<int>(pts_.size())) {
        const Measure3d::Point3d &sp = pts_[selected_.idx];
        glLineWidth(2.0f);
        glBegin(GL_LINES);
        if (showPq_) {
            glColor3f(0.9f, 0.1f, 0.1f);  // Camera L -> P
            glVertex3d(camL_.x(), camL_.y(), camL_.z());
            glVertex3d(sp.p.x(), sp.p.y(), sp.p.z());
            glColor3f(0.1f, 0.3f, 0.9f);  // Camera R -> Q
            glVertex3d(camR_.x(), camR_.y(), camR_.z());
            glVertex3d(sp.q.x(), sp.q.y(), sp.q.z());
        }
        glColor3f(0.1f, 0.6f, 0.1f);  // baseline centre -> mid
        glVertex3d(baseCenter.x(), baseCenter.y(), baseCenter.z());
        glVertex3d(sp.mid.x(), sp.mid.y(), sp.mid.z());
        glEnd();
    }

    // ---- points ----
    glPointSize(7.0f);
    glBegin(GL_POINTS);
    for (const auto &p : pts_) {
        const auto c = colorForDir(p.direction);
        glColor3f(c[0], c[1], c[2]);
        glVertex3d(p.mid.x(), p.mid.y(), p.mid.z());
    }
    glEnd();

    // ---- P/Q pairs + FOV cones (before the highlight, so markers draw on top) ----
    drawPq();
    drawFovRings();

    auto bigMarker = [&](const Highlight &h, float r, float g, float b) {
        if (h.idx < 0 || h.idx >= static_cast<int>(pts_.size())) return;
        const Eigen::Vector3d w = entityPos(h);
        glPointSize(13.0f);
        glBegin(GL_POINTS);
        glColor3f(r, g, b);
        glVertex3d(w.x(), w.y(), w.z());
        glEnd();
    };
    bigMarker(selected_, 0.0f, 0.0f, 0.0f);
    if (selected_.idx < 0) bigMarker(hovered_, 0.1f, 0.6f, 1.0f);

    // ---- camera centres, baseline centre, origin ----
    glPointSize(12.0f);
    glBegin(GL_POINTS);
    glColor3f(0.9f, 0.1f, 0.1f);  // Camera L
    glVertex3d(camL_.x(), camL_.y(), camL_.z());
    glColor3f(0.1f, 0.3f, 0.9f);  // Camera R
    glVertex3d(camR_.x(), camR_.y(), camR_.z());
    glColor3f(0.1f, 0.6f, 0.1f);  // baseline centre
    glVertex3d(baseCenter.x(), baseCenter.y(), baseCenter.z());
    glColor3f(0.5f, 0.5f, 0.5f);  // origin
    glVertex3d(0, 0, 0);
    glEnd();

    // ---- fitted planes last: translucent, so they blend over the geometry ----
    drawPlanes();

    glGetDoublev(GL_MODELVIEW_MATRIX, mvCache_);
    glGetDoublev(GL_PROJECTION_MATRIX, prCache_);
    glGetIntegerv(GL_VIEWPORT, vpCache_);
    dprCache_ = devicePixelRatioF();

    rebuildOverlay();
}

// One FOV cone per camera. alpha (the Moildev angle off the optical axis) runs
// from 0 on the axis to fov/2 at the rim, and a ray with alpha = 0 points along
// world +Z -- the same convention alBa2Vector uses to turn a detection into a
// ray -- so the cone is drawn about +Z from the camera centre. The ring is the
// alpha = fov/2 circle placed on a sphere of the board's radius, i.e. exactly
// where that camera stops seeing at the depth being measured.
// Deepest side-plane point seen from `cam`: the maximum DEPTH, i.e. the distance
// along the optical axis (+Z), of every direction except "center". Depth, not
// straight-line distance -- a side board sitting far out to one side is much
// farther away than it is deep, and using the distance blew the ring up well
// past the boards it is supposed to bound. Falls back to all points, then to the
// scene radius, when there are no side planes yet.
double Point3dGlCanvas::sidePlaneMaxDepth(const Eigen::Vector3d &cam) const {
    double sideMax = 0, allMax = 0;
    for (const auto &p : pts_) {
        const double d = p.mid.z() - cam.z();  // +Z is the optical axis (alpha = 0)
        if (!std::isfinite(d) || d <= 0) continue;
        allMax = std::max(allMax, d);
        if (p.direction != "center") sideMax = std::max(sideMax, d);
    }
    if (sideMax > 0) return sideMax;
    if (allMax > 0) return allMax;
    return sceneRadius_;
}

void Point3dGlCanvas::drawFovRings() {
    if (!showFov_) return;

    auto cone = [&](const Eigen::Vector3d &cam, double fovDeg, double ringDepth, float r, float g,
                    float b) {
        const double full = (fovAngle_ > 0) ? fovAngle_ : fovDeg;
        if (full <= 0) return;  // no parameter loaded and no angle typed
        const double half = std::min(179.9, full * 0.5) * kDegToRad;
        double dist = ringDepth;
        if (!std::isfinite(dist) || dist < 1e-6) dist = sceneRadius_;
        // The ring's RADIUS is the deepest side plane, so the circle bounds the
        // boards instead of dwarfing them. Its position along the axis is then
        // whatever keeps the cone at the true half-angle: r / tan(half), which
        // goes negative past 90 deg because a >180 deg fisheye does see behind
        // its own centre. tan(90 deg) is infinite -> the ring sits on the camera.
        const double rr = dist;
        const double t = std::tan(half);
        const double zz = (std::abs(t) < 1e-6) ? 0.0 : rr / t;

        constexpr int kSeg = 96;
        auto ringPt = [&](int i) {
            const double t = 2.0 * M_PI * i / kSeg;
            return Eigen::Vector3d(cam.x() + rr * std::cos(t), cam.y() + rr * std::sin(t),
                                   cam.z() + zz);
        };

        glLineWidth(2.0f);
        glColor4f(r, g, b, 0.9f);
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < kSeg; ++i) {
            const Eigen::Vector3d p = ringPt(i);
            glVertex3d(p.x(), p.y(), p.z());
        }
        glEnd();

        // Cone edges + optical axis, so the ring reads as a viewing volume
        // rather than a floating circle.
        glLineWidth(1.0f);
        glColor4f(r, g, b, 0.35f);
        glBegin(GL_LINES);
        for (int i = 0; i < kSeg; i += kSeg / 12) {
            const Eigen::Vector3d p = ringPt(i);
            glVertex3d(cam.x(), cam.y(), cam.z());
            glVertex3d(p.x(), p.y(), p.z());
        }
        glVertex3d(cam.x(), cam.y(), cam.z());  // optical axis (alpha = 0), to the
        glVertex3d(cam.x(), cam.y(), cam.z() + dist);  // deepest side plane
        glEnd();
    };

    // Same radius for both, so the rings only differ if the parameter FOVs do.
    cone(camL_, fovParamL_, fovRingDepth_, 0.9f, 0.1f, 0.1f);  // Camera L colour
    cone(camR_, fovParamR_, fovRingDepth_, 0.1f, 0.3f, 0.9f);  // Camera R colour
}

// Every point's P and Q -- the closest approach of the left and right camera
// rays -- with the shortest inter-ray line drawn P -> mid -> Q. The mid point is
// exactly halfway, so a long segment is a large ray gap: the pair disagreed.
// P is drawn in the Camera L colour and Q in the Camera R colour, matching the
// camera markers they belong to.
void Point3dGlCanvas::drawPq() {
    if (!showPq_ || pts_.empty()) return;
    auto vert = [](const Eigen::Vector3d &p) { glVertex3d(p.x(), p.y(), p.z()); };

    glLineWidth(1.5f);
    glBegin(GL_LINES);
    for (const auto &p : pts_) {
        glColor4f(0.9f, 0.1f, 0.1f, 0.75f);  // P -> mid, in the Cam L colour
        vert(p.p);
        vert(p.mid);
        glColor4f(0.1f, 0.3f, 0.9f, 0.75f);  // mid -> Q, in the Cam R colour
        vert(p.mid);
        vert(p.q);
    }
    glEnd();

    glPointSize(5.0f);
    glBegin(GL_POINTS);
    for (const auto &p : pts_) {
        glColor3f(0.9f, 0.1f, 0.1f);  // P (Cam L ray)
        vert(p.p);
        glColor3f(0.1f, 0.3f, 0.9f);  // Q (Cam R ray)
        vert(p.q);
    }
    glEnd();
}

// One patch per direction: a translucent fill, a grid so the surface reads as a
// plane from any angle, a solid border, the residual (point -> plane) drop lines
// that show how flat the direction actually is, and a short normal stub.
// Depth writes are off for the fill so a plane never hides the points behind it.
void Point3dGlCanvas::drawPlanes() {
    if (!showPlanes_ || planes_.empty()) return;

    const double normalLen = sceneRadius_ * 0.12;
    glDepthMask(GL_FALSE);
    for (const auto &pl : planes_) {
        const auto c = colorForDir(pl.direction);
        const Eigen::Vector3d u = pl.basis1 * pl.half1;
        const Eigen::Vector3d v = pl.basis2 * pl.half2;
        const Eigen::Vector3d c00 = pl.centroid - u - v, c10 = pl.centroid + u - v,
                              c11 = pl.centroid + u + v, c01 = pl.centroid - u + v;
        auto vert = [](const Eigen::Vector3d &p) { glVertex3d(p.x(), p.y(), p.z()); };

        glColor4f(c[0], c[1], c[2], 0.16f);
        glBegin(GL_QUADS);
        vert(c00);
        vert(c10);
        vert(c11);
        vert(c01);
        glEnd();

        glColor4f(c[0], c[1], c[2], 0.45f);
        glLineWidth(1.0f);
        glBegin(GL_LINES);
        const int g = 4;  // interior grid lines per axis
        for (int i = 1; i < g; ++i) {
            const double t = -1.0 + 2.0 * i / g;
            vert(pl.centroid + u * t - v);
            vert(pl.centroid + u * t + v);
            vert(pl.centroid - u + v * t);
            vert(pl.centroid + u + v * t);
        }
        glEnd();

        glColor4f(c[0], c[1], c[2], 0.9f);
        glLineWidth(2.0f);
        glBegin(GL_LINE_LOOP);
        vert(c00);
        vert(c10);
        vert(c11);
        vert(c01);
        glEnd();

        // Normal stub, from the centroid towards the cameras.
        glBegin(GL_LINES);
        vert(pl.centroid);
        vert(pl.centroid + pl.normal * normalLen);
        glEnd();

        // Drop lines: each point to its foot on the plane (its residual).
        glColor4f(0.8f, 0.2f, 0.2f, 0.75f);
        glLineWidth(1.0f);
        glBegin(GL_LINES);
        for (const auto &p : pts_) {
            if (p.direction != pl.direction) continue;
            const double dist = (p.mid - pl.centroid).dot(pl.normal);
            vert(p.mid);
            vert(p.mid - dist * pl.normal);
        }
        glEnd();
    }
    glDepthMask(GL_TRUE);
}

// Drawn in the scene: the axis letters, and the info box of the point being
// hovered or pinned (it has to sit beside that point to identify it). The fixed
// names are in the side panel instead.
void Point3dGlCanvas::rebuildOverlay() {
    if (!overlay_) return;
    std::vector<GlTextOverlay::Item> items;
    const double L = center_.norm() + sceneRadius_ * 1.2;

    auto add = [&](const Eigen::Vector3d &w, const QString &text, const QColor &c,
                   bool box = false) {
        QPointF s;
        if (projectToScreen(w, s)) items.push_back({s, text, c, box});
    };
    add({L, 0, 0}, "X", QColor(200, 20, 20));
    add({0, L, 0}, "Y", QColor(20, 150, 20));
    add({0, 0, L}, "Z", QColor(20, 50, 220));

    const Highlight h = highlighted();
    if (h.idx >= 0) {
        const Measure3d::Point3d &p = pts_[h.idx];
        const Eigen::Vector3d w = entityPos(h);
        const Eigen::Vector3d baseCenter = (camL_ + camR_) * 0.5;
        const double depth = (w - baseCenter).norm();  // distance to baseline centre
        QString title = QString("Point %1  ·  %2").arg(p.pointId).arg(
            QString::fromStdString(p.direction));
        QColor color(20, 20, 20);
        if (h.kind == PickKind::P) {
            title += "  ·  P (Cam L ray)";
            color = QColor(170, 20, 20);
        } else if (h.kind == PickKind::Q) {
            title += "  ·  Q (Cam R ray)";
            color = QColor(20, 50, 200);
        }
        QString info = QString("%1\nX %2   Y %3   Z %4\ndepth %5")
                           .arg(title)
                           .arg(w.x(), 0, 'f', 1)
                           .arg(w.y(), 0, 'f', 1)
                           .arg(w.z(), 0, 'f', 1)
                           .arg(depth, 0, 'f', 1);
        // On a P or Q, the gap to its partner is the number that matters.
        if (h.kind != PickKind::Mid)
            info += QString("\nray gap %1").arg((p.p - p.q).norm(), 0, 'f', 2);
        add(w, info, color, /*box=*/true);
    }

    overlay_->setItems(std::move(items));
}

bool Point3dGlCanvas::projectToScreen(const Eigen::Vector3d &w, QPointF &out) const {
    const double x = w.x(), y = w.y(), z = w.z();
    const double ex = mvCache_[0] * x + mvCache_[4] * y + mvCache_[8] * z + mvCache_[12];
    const double ey = mvCache_[1] * x + mvCache_[5] * y + mvCache_[9] * z + mvCache_[13];
    const double ez = mvCache_[2] * x + mvCache_[6] * y + mvCache_[10] * z + mvCache_[14];
    const double ew = mvCache_[3] * x + mvCache_[7] * y + mvCache_[11] * z + mvCache_[15];
    const double cx = prCache_[0] * ex + prCache_[4] * ey + prCache_[8] * ez + prCache_[12] * ew;
    const double cy = prCache_[1] * ex + prCache_[5] * ey + prCache_[9] * ez + prCache_[13] * ew;
    const double cz = prCache_[2] * ex + prCache_[6] * ey + prCache_[10] * ez + prCache_[14] * ew;
    const double cw = prCache_[3] * ex + prCache_[7] * ey + prCache_[11] * ez + prCache_[15] * ew;
    if (cw == 0.0) return false;
    const double nx = cx / cw, ny = cy / cw, nz = cz / cw;
    if (nz < -1.0 || nz > 1.0) return false;
    const double winX = vpCache_[0] + (nx + 1.0) * 0.5 * vpCache_[2];
    const double winY = vpCache_[1] + (ny + 1.0) * 0.5 * vpCache_[3];
    out = QPointF(winX / dprCache_, height() - winY / dprCache_);
    return true;
}

// Nearest entity under the cursor. P and Q only compete when they are drawn.
Point3dGlCanvas::Highlight Point3dGlCanvas::pickEntity(const QPointF &pos) const {
    Highlight best;
    double bestD2 = 14.0 * 14.0;
    auto consider = [&](const Eigen::Vector3d &w, int i, PickKind kind) {
        QPointF s;
        if (!projectToScreen(w, s)) return;
        const double d2 =
            (s.x() - pos.x()) * (s.x() - pos.x()) + (s.y() - pos.y()) * (s.y() - pos.y());
        if (d2 < bestD2) {
            bestD2 = d2;
            best = {i, kind};
        }
    };
    for (int i = 0; i < static_cast<int>(pts_.size()); ++i) {
        consider(pts_[i].mid, i, PickKind::Mid);
        if (showPq_) {
            consider(pts_[i].p, i, PickKind::P);
            consider(pts_[i].q, i, PickKind::Q);
        }
    }
    return best;
}

void Point3dGlCanvas::mousePressEvent(QMouseEvent *e) {
    lastMouse_ = e->pos();
    orbiting_ = (e->button() == Qt::LeftButton);
    panning_ = (e->button() == Qt::RightButton || e->button() == Qt::MiddleButton);
    if (orbiting_ || panning_) setCursor(Qt::ClosedHandCursor);
}

void Point3dGlCanvas::mouseReleaseEvent(QMouseEvent *) {
    orbiting_ = panning_ = false;
    setCursor(Qt::OpenHandCursor);
}

void Point3dGlCanvas::mouseMoveEvent(QMouseEvent *e) {
    const QPoint d = e->pos() - lastMouse_;

    if (orbiting_) {
        lastMouse_ = e->pos();
        // Trackball: turn about the CAMERA's own axes, applied on the left of the
        // current orientation. Nothing is expressed as yaw/pitch, so the scene can
        // keep rolling past the poles in any direction, without limit.
        const Eigen::Quaterniond turn =
            Eigen::Quaterniond(Eigen::AngleAxisd(d.y() * 0.4 * kDegToRad, Eigen::Vector3d::UnitX())) *
            Eigen::Quaterniond(Eigen::AngleAxisd(d.x() * 0.4 * kDegToRad, Eigen::Vector3d::UnitY()));
        rot_ = (turn * rot_).normalized();
        update();
        return;
    }

    if (panning_) {
        lastMouse_ = e->pos();
        // Drag the orbit target across the view plane, one screen pixel to one
        // pixel's worth of world at the target's depth.
        const double worldPerPixel =
            2.0 * std::tan(kFovDeg * kDegToRad / 2.0) * dist_ / std::max(1, height());
        const Eigen::Matrix3d R = rot_.toRotationMatrix();
        const Eigen::Vector3d right = R.row(0).transpose();  // camera axes in world space
        const Eigen::Vector3d up = R.row(1).transpose();
        center_ += (-right * d.x() + up * d.y()) * worldPerPixel;
        update();
        return;
    }

    if (selected_.idx >= 0) return;  // a point is pinned: don't hover-highlight others
    const Highlight h = pickEntity(e->position());
    if (h.idx != hovered_.idx || h.kind != hovered_.kind) {
        hovered_ = h;
        update();  // the info box beside the point is redrawn with the frame
    }
}

void Point3dGlCanvas::mouseDoubleClickEvent(QMouseEvent *e) {
    if (e->button() != Qt::LeftButton) {  // right/middle double-click: back to the start
        resetView();
        return;
    }
    selected_ = pickEntity(e->position());  // empty space clears the pin
    hovered_ = {};
    update();
}

void Point3dGlCanvas::wheelEvent(QWheelEvent *e) {
    const double steps = e->angleDelta().y() / 120.0;
    dist_ *= std::pow(0.85, steps);
    // Wide range: close enough to inspect one corner, far enough to see the whole
    // rig from outside.
    dist_ = std::max(sceneRadius_ * 0.02, std::min(dist_, sceneRadius_ * 200.0));
    update();
}

// ======================================================================
// Point3dGlView -- information panel (left) + canvas
// ======================================================================

Point3dGlView::Point3dGlView(QWidget *parent) : QWidget(parent) {
    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ---- information panel on the LEFT, beside the 3D area ----
    auto *panel = new QFrame(this);
    panel->setFrameShape(QFrame::StyledPanel);
    panel->setStyleSheet("QFrame{background:#f6f6f6;}");
    panel->setFixedWidth(235);
    auto *panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(8, 8, 8, 8);
    panelLayout->setSpacing(6);

    // Scrolled, so a 5-plane legend still fits on a small monitor.
    auto *sa = new QScrollArea(panel);
    sa->setWidgetResizable(true);
    sa->setFrameShape(QFrame::NoFrame);
    sa->setStyleSheet("QScrollArea{background:transparent;} QWidget{background:transparent;}");
    sceneInfo_ = new QLabel(sa);
    sceneInfo_->setTextFormat(Qt::RichText);
    sceneInfo_->setWordWrap(true);
    sceneInfo_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    sceneInfo_->setStyleSheet("font-size:12px;");
    sa->setWidget(sceneInfo_);
    panelLayout->addWidget(sa, 1);

    // Plane visibility + reset live here too, so every place that embeds this
    // widget (the anypoint "3D View" tab and the ORI_DET result tab) gets them
    // without wiring anything.
    planeToggle_ = new QCheckBox(tr("Show fitted planes"), panel);
    planeToggle_->setStyleSheet("font-size:12px;");
    panelLayout->addWidget(planeToggle_);

    // One click shows the triangulation behind every point: P on the left
    // camera's ray, Q on the right camera's, joined through their mid point.
    pqToggle_ = new QPushButton(tr("Show P / Q points"), panel);
    pqToggle_->setCheckable(true);
    pqToggle_->setToolTip(tr("Draw, for every point, P (closest approach on the Cam L ray) and "
                             "Q (on the Cam R ray) joined by the shortest inter-ray line through "
                             "the mid point. Hover a P or Q for its X/Y/Z and the ray gap; "
                             "double-click a point to draw Cam L -> P and Cam R -> Q as well."));
    panelLayout->addWidget(pqToggle_);

    // FOV cones. The angle comes from each camera's parameter JSON ("cameraFov")
    // until the user types their own, so the ring drawn is the one the camera
    // actually has, not a guess.
    fovToggle_ = new QPushButton(tr("Show FOV rings"), panel);
    fovToggle_->setCheckable(true);
    fovToggle_->setToolTip(tr("Draw each camera's field of view as a cone from its centre. The "
                              "ring's radius is the DEEPEST side plane (north/south/west/east), "
                              "the same for both cameras, so it bounds the boards rather than "
                              "dwarfing them. The angle starts at the camera parameter FOV; type "
                              "a smaller one to see how much of the board a narrower field would "
                              "still cover."));
    panelLayout->addWidget(fovToggle_);

    auto *fovRow = new QHBoxLayout();
    fovRow->setContentsMargins(0, 0, 0, 0);
    auto *fovLabel = new QLabel(tr("FOV angle"), panel);
    fovLabel->setStyleSheet("font-size:12px;");
    fovRow->addWidget(fovLabel);
    fovSpin_ = new QDoubleSpinBox(panel);
    fovSpin_->setRange(1.0, 360.0);
    fovSpin_->setDecimals(1);
    fovSpin_->setSingleStep(5.0);
    fovSpin_->setSuffix("°");
    fovSpin_->setValue(220.0);  // replaced by the parameter value once one loads
    fovSpin_->setToolTip(tr("Full cone angle in degrees. A camera parameter file sets this "
                            "automatically; alpha (the Moildev angle off the optical axis) runs "
                            "to half of it."));
    fovRow->addWidget(fovSpin_, 1);
    panelLayout->addLayout(fovRow);

    // ---- viewpoint buttons ----
    // Sighting along an axis is what makes a fitted plane readable: face-on it
    // fills the view, edge-on it collapses to a line and any point off the plane
    // stands out immediately. So every axis gets both directions, plus the
    // three-quarter default, the rig's own viewpoint, fit and focus.
    auto *viewTitle = new QLabel(QString("<b>%1</b>").arg(tr("Viewpoint")), panel);
    viewTitle->setTextFormat(Qt::RichText);
    viewTitle->setStyleSheet("font-size:12px;");
    panelLayout->addWidget(viewTitle);

    using Preset = Point3dGlCanvas::ViewPreset;
    auto *axisGrid = new QGridLayout();
    axisGrid->setContentsMargins(0, 0, 0, 0);
    axisGrid->setSpacing(4);
    auto *actionGrid = new QGridLayout();
    actionGrid->setContentsMargins(0, 0, 0, 0);
    actionGrid->setSpacing(4);

    auto addBtn = [&](QGridLayout *grid, int row, int col, const QString &text, const QString &tip,
                      std::function<void()> action) {
        auto *b = new QPushButton(text, panel);
        b->setToolTip(tip);
        b->setStyleSheet("font-size:11px; padding:3px;");
        connect(b, &QPushButton::clicked, this, [action = std::move(action)] { action(); });
        grid->addWidget(b, row, col);
    };
    auto preset = [this](Preset p) { return [this, p] { canvas_->setPreset(p); }; };

    addBtn(axisGrid, 0, 0, "+X", tr("Eye on +X, looking back: shows the Y-Z plane"),
           preset(Preset::PlusX));
    addBtn(axisGrid, 0, 1, "+Y", tr("Eye on +Y, looking back: shows the X-Z plane"),
           preset(Preset::PlusY));
    addBtn(axisGrid, 0, 2, "+Z", tr("Eye on +Z, looking back: shows the X-Y plane"),
           preset(Preset::PlusZ));
    addBtn(axisGrid, 1, 0, "-X", tr("Eye on -X, the opposite side"), preset(Preset::MinusX));
    addBtn(axisGrid, 1, 1, "-Y", tr("Eye on -Y, the opposite side"), preset(Preset::MinusY));
    addBtn(axisGrid, 1, 2, "-Z", tr("Eye on -Z, the opposite side"), preset(Preset::MinusZ));
    panelLayout->addLayout(axisGrid);

    addBtn(actionGrid, 0, 0, tr("Isometric"), tr("The opening three-quarter view"),
           preset(Preset::Isometric));
    addBtn(actionGrid, 0, 1, tr("From cameras"),
           tr("Look from the baseline centre towards the board -- roughly what the rig sees"),
           preset(Preset::Cameras));
    addBtn(actionGrid, 1, 0, tr("Fit all"), tr("Re-frame every point without turning the scene"),
           [this] { canvas_->fitToScene(); });
    addBtn(actionGrid, 1, 1, tr("Focus point"),
           tr("Double-click a point to pin it, then this orbits around that point, zoomed in "
              "(with nothing pinned it just fits everything)"),
           [this] { canvas_->focusHighlighted(); });
    panelLayout->addLayout(actionGrid);

    auto *resetBtn = new QPushButton(tr("Reset view"), panel);
    resetBtn->setToolTip(tr("Back to the opening view: isometric, everything framed"));
    panelLayout->addWidget(resetBtn);

    auto *hint = new QLabel(tr("<b>Left drag</b> orbit (no limit)<br>"
                               "<b>Right drag</b> pan<br>"
                               "<b>Wheel</b> zoom<br>"
                               "<b>Double-click</b> pin a point"),
                            panel);
    hint->setTextFormat(Qt::RichText);
    hint->setWordWrap(true);
    hint->setStyleSheet("font-size:11px; color:#555;");
    panelLayout->addWidget(hint);

    root->addWidget(panel);

    canvas_ = new Point3dGlCanvas(this);
    root->addWidget(canvas_, 1);
    planeToggle_->setChecked(canvas_->planesVisible());

    connect(planeToggle_, &QCheckBox::toggled, this, &Point3dGlView::setPlanesVisible);
    connect(pqToggle_, &QPushButton::toggled, this,
            [this](bool on) { canvas_->setPqVisible(on); });
    connect(fovToggle_, &QPushButton::toggled, this, [this](bool on) {
        canvas_->setFovRingsVisible(on);
        refreshSceneInfo();
    });
    connect(fovSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this](double deg) {
                fovEdited_ = true;  // the user owns the angle from now on
                canvas_->setFovRingAngle(deg);
                refreshSceneInfo();
            });
    connect(resetBtn, &QPushButton::clicked, this, &Point3dGlView::resetView);
    connect(canvas_, &Point3dGlCanvas::sceneChanged, this, &Point3dGlView::refreshSceneInfo);

    refreshSceneInfo();
}

void Point3dGlView::setPoints(const std::vector<Measure3d::Point3d> &pts,
                              const Eigen::Vector3d &camL, const Eigen::Vector3d &camR) {
    canvas_->setPoints(pts, camL, camR);
}

// The parameter FOVs seed the input until the user types an angle of their own,
// so what is drawn by default is the camera's real field of view.
void Point3dGlView::setCameraFov(double fovLeftDeg, double fovRightDeg) {
    canvas_->setCameraFov(fovLeftDeg, fovRightDeg);
    if (!fovEdited_ && fovSpin_) {
        const double seed = fovLeftDeg > 0 ? fovLeftDeg : fovRightDeg;
        if (seed > 0) {
            QSignalBlocker block(fovSpin_);
            fovSpin_->setValue(seed);
        }
    }
    // Angle 0 keeps each camera on its own parameter FOV until the user edits.
    canvas_->setFovRingAngle(fovEdited_ && fovSpin_ ? fovSpin_->value() : 0.0);
    refreshSceneInfo();
}

void Point3dGlView::resetView() { canvas_->resetView(); }

void Point3dGlView::setPlanesVisible(bool on) {
    canvas_->setPlanesVisible(on);
    if (planeToggle_ && planeToggle_->isChecked() != on) {
        QSignalBlocker block(planeToggle_);
        planeToggle_->setChecked(on);
    }
}

bool Point3dGlView::planesVisible() const { return canvas_->planesVisible(); }

// Cameras, baseline centre and origin, then one colour-keyed entry per fitted
// plane (point count + thickness, i.e. how flat that direction came out). One
// entry per line: the panel is narrow and vertical.
void Point3dGlView::refreshSceneInfo() {
    if (!sceneInfo_) return;
    const Eigen::Vector3d &camL = canvas_->cameraL();
    const Eigen::Vector3d &camR = canvas_->cameraR();
    const Eigen::Vector3d baseCenter = (camL + camR) * 0.5;

    // FOV straight from each parameter JSON, so the panel says what the ring is
    // based on (and shows when no parameter has been loaded).
    auto fovText = [this](double paramFov) {
        if (paramFov <= 0) return QString();
        QString s = QString(" &middot; fov %1°").arg(paramFov, 0, 'f', 0);
        if (canvas_->fovRingAngle() > 0)
            s += QString(" (%1 %2°)").arg(tr("drawn")).arg(canvas_->fovRingAngle(), 0, 'f', 0);
        return s;
    };

    QStringList html;
    html << QString("<b>%1</b>").arg(tr("Scene"));
    html << infoEntry(tr("Camera L"), QColor(200, 20, 20),
                      fmtVec(camL) + fovText(canvas_->parameterFovLeft()));
    html << infoEntry(tr("Camera R"), QColor(20, 50, 220),
                      fmtVec(camR) + fovText(canvas_->parameterFovRight()));
    html << infoEntry(tr("Baseline center"), QColor(20, 130, 20), fmtVec(baseCenter));
    html << infoEntry(tr("Origin O"), QColor(90, 90, 90), "(0.0, 0.0, 0.0)");
    html << infoEntry(tr("Points"), QColor(60, 60, 60), QString::number(canvas_->points().size()));

    html << QString("<br><b>%1</b>").arg(tr("Fitted planes"));
    const auto &planes = canvas_->planes();
    if (planes.empty()) {
        html << (canvas_->points().empty()
                     ? tr("No result yet.")
                     : tr("No direction has 3 or more points to fit."));
    } else {
        for (const auto &pl : planes) {
            QString v = tr("%1 pts").arg(pl.pointCount);
            if (!std::isnan(pl.thickness))
                v += tr(", %1 mm thick").arg(pl.thickness, 0, 'f', 2);
            html << infoEntry(QString::fromStdString(pl.direction), qColorForDir(pl.direction),
                              "<br>" + v);
        }
    }
    sceneInfo_->setText(html.join("<br>"));
}
