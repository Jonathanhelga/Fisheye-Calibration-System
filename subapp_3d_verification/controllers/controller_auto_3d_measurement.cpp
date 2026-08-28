#include "controller_auto_3d_measurement.h"

#include "Help.h"

#include <cmath>
#include <iostream>

#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QFileInfo>
#include <QFuture>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpinBox>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QTabWidget>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <opencv2/calib3d.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "AnypointChessboard.h"
#include "PanelCornerRecovery.h"
#include "PlaneFit3dViz.h"
#include "Point3dGlView.h"
#include "UI3d_measurement.h"
#include "UiFullScreen.h"

using Measure3d::DetPoint;
using Measure3d::DetectionResult;
using Measure3d::Point3d;
using Vec3 = Eigen::Vector3d;

// The dialog is two-camera (left/right); the mid camera was dropped along with
// every *_mid* widget when the layout moved to UI3d_measurement.ui.
const std::vector<std::string> Controller3dMeasurement::kCameras = {"left", "right"};
const std::vector<std::string> Controller3dMeasurement::kDirections = {"center", "north", "south",
                                                                  "west", "east"};

// ORI_DET click order: per the spec the user does center first, then east, west,
// south, north -- each plane LEFT (4 corners) then RIGHT (4 corners).
const std::vector<std::string> Controller3dMeasurement::kOriPlaneOrder = {
    "center", "east", "west", "south", "north"};
// The 4 clicks per plane, in this order (matches the reference's homography input).
const std::array<const char *, 4> Controller3dMeasurement::kOriCornerLabels = {
    "top-left", "top-right", "bottom-right", "bottom-left"};

namespace {
// default_angles: (pitch, yaw, zoom).
const std::map<std::string, std::array<double, 3>> kDefaultAngles = {
    {"center", {0, 0, 4}},   {"north", {-90, 0, 4}}, {"south", {90, 0, 4}},
    {"west", {0, -90, 4}},   {"east", {0, 90, 4}}};

QPixmap matToPixmap(const cv::Mat &bgr) {
    cv::Mat m;
    if (bgr.channels() == 1)
        cv::cvtColor(bgr, m, cv::COLOR_GRAY2BGR);
    else
        m = bgr;
    if (!m.isContinuous()) m = m.clone();
    QImage img(m.data, m.cols, m.rows, static_cast<int>(m.step), QImage::Format_BGR888);
    return QPixmap::fromImage(img.copy());
}

// Where each plane sits in the ORIGINAL fisheye picture. Not a guess: it falls
// out of mapsAnypointMode2, which is what the anypoint method uses to look in a
// direction. At the centre of an anypoint view, yaw = +90 ("east") samples at
// beta = 180 deg, i.e. to the RIGHT of the fisheye centre, and pitch = -90
// ("north") samples at beta = -90 deg, which lands BELOW the centre because
// image rows grow downward. So on screen north is at the bottom and south at the
// top -- exactly the thing that makes people click the wrong panel.
struct PlaneCompass {
    const char *plane;
    int dx, dy;         // direction from the fisheye centre, in image pixels
    const char *where;  // human wording for the prompt
};
const std::array<PlaneCompass, 5> kPlaneCompass = {{
    {"center", 0, 0, "middle"},
    {"east", 1, 0, "right"},
    {"west", -1, 0, "left"},
    {"south", 0, -1, "top"},
    {"north", 0, 1, "bottom"},
}};

// Mean distance between neighbouring inner corners (original px). This is what
// the overlay is sized against: how much room one corner actually has before it
// runs into the next, which depends on both the image size and how many squares
// the board has. Returns 0 when the grid is unknown.
double cornerSpacing(const PanelCorner::PanelDetection &det) {
    std::map<std::pair<int, int>, cv::Point2f> byRc;
    for (const auto &c : det.corners) byRc[{c.row, c.col}] = c.pt;
    double sum = 0;
    int n = 0;
    for (const auto &kv : byRc) {
        const auto right = byRc.find({kv.first.first, kv.first.second + 1});
        const auto down = byRc.find({kv.first.first + 1, kv.first.second});
        if (right != byRc.end()) {
            sum += cv::norm(kv.second - right->second);
            ++n;
        }
        if (down != byRc.end()) {
            sum += cv::norm(kv.second - down->second);
            ++n;
        }
    }
    return n > 0 ? sum / n : 0.0;
}

const PlaneCompass *compassFor(const std::string &plane) {
    for (const auto &c : kPlaneCompass)
        if (plane == c.plane) return &c;
    return nullptr;
}

// Everything a detection worker produces, carried back to the UI thread in one
// value (QtConcurrent futures return by value; the cv::Mats are ref-counted).
struct DetectOutput {
    DetectionResult res;
    cv::Mat overlayAny, overlayFish;
    std::string camKey, direction;
    std::pair<int, int> ps{0, 0};
};

// ---- auto-framing search (runs on a worker thread; touches no UI) ----------

// Snapshot of one (camera, direction) the search needs: a read-only Moildev, the
// full-resolution gray fisheye, the current pitch/yaw/zoom as the search origin,
// and a fixed pattern size (0,0 => discover from a candidate list).
struct AutoInput {
    std::string cam, dir;
    const Moildev *moil = nullptr;
    cv::Mat grayFull;  // imageWidth x imageHeight, single channel
    double basePitch = 0, baseYaw = 0, baseZoom = 4;
    int fixedCols = 0, fixedRows = 0;
};

struct AutoFrame {
    std::string cam, dir;
    bool found = false;
    double pitch = 0, yaw = 0, zoom = 4;
    int cols = 0, rows = 0;
};

bool detectSB(const cv::Mat &gray, int c, int r, int flags) {
    if (c < 3 || r < 3) return false;
    std::vector<cv::Point2f> pts;
    try {
        return cv::findChessboardCornersSB(gray, cv::Size(c, r), pts, flags);
    } catch (const cv::Exception &) {
        return false;
    }
}

// Find a framing (zoom) and pattern size at which the board is detected. Testing
// lessons baked in:
//   * search at 2032 px (not 1024): low res gave both false positives and false
//     negatives; 2032 matches the full-3040 detector but is ~3x faster,
//   * current framing (baseZoom) is tried first, so an already-framed board is
//     found immediately -- same result as the old manual Detect (no regression),
//   * candidate sizes are ordered by descending corner count so the full board
//     wins over a sub-block (e.g. 6x5 over an embedded 5x5),
//   * count-1 is tried too, in case the user typed SQUARES not inner corners.
AutoFrame searchOneDirection(const AutoInput &in) {
    // Two stages: LOCATE the board by panning pitch/yaw on a fast 1024 canvas, then
    // MAXIMISE the board size at that direction, validated at FULL resolution (what
    // the app's detector actually uses). NORMALIZE|EXHAUSTIVE = one detect per try.
    const int flags = cv::CALIB_CB_NORMALIZE_IMAGE | cv::CALIB_CB_EXHAUSTIVE;
    AutoFrame best;
    best.cam = in.cam;
    best.dir = in.dir;

    // Candidate sizes: the typed size, its transpose, and count-1 (in case the
    // user typed SQUARES rather than inner corners). Ordered by descending corner
    // count so the full board wins over any sub-block (e.g. 6x5 over 5x5).
    std::vector<std::pair<int, int>> sizes;
    auto add = [&](int c, int r) {
        if (c < 3 || r < 3) return;
        for (const auto &s : sizes)
            if (s.first == c && s.second == r) return;
        sizes.push_back({c, r});
    };
    if (in.fixedCols > 1 && in.fixedRows > 1) {
        add(in.fixedCols, in.fixedRows);
        add(in.fixedRows, in.fixedCols);
        add(in.fixedCols - 1, in.fixedRows - 1);  // squares -> inner corners
        add(in.fixedRows - 1, in.fixedCols - 1);
    } else {
        for (int c = 4; c <= 9; ++c) add(c, c);  // no hint: try square boards
    }
    std::sort(sizes.begin(), sizes.end(),
              [](const auto &a, const auto &b) { return a.first * a.second > b.first * b.second; });

    std::cerr << "[auto] search " << in.cam << "/" << in.dir << " hint=" << in.fixedCols << "x"
              << in.fixedRows << " basePitch=" << in.basePitch << " baseYaw=" << in.baseYaw
              << " baseZoom=" << in.baseZoom << " sizes=";
    for (const auto &s : sizes) std::cerr << s.first << "x" << s.second << " ";
    std::cerr << "\n";

    // Stage A -- LOCATE the board direction. It may sit off the preset (off-centre
    // optical axis / off-centre board), so pan an alpha(=yaw)/beta(=pitch) grid on
    // a fast 1024 canvas. A mid zoom (2.5) leaves margin so the whole board shows.
    constexpr int kSweep = 1024;
    const double off[] = {0, -15, 15, -30, 30};
    double locPitch = 0, locYaw = 0;
    int locCorners = 0;
    for (double dp : off)
        for (double dy : off) {
            cv::Mat mx, my, any;
            in.moil->mapsAnypointMode2(in.basePitch + dp, in.baseYaw + dy, 2.5, mx, my, kSweep,
                                       kSweep);
            cv::remap(in.grayFull, any, mx, my, cv::INTER_LINEAR);
            for (const auto &s : sizes) {  // descending corner count
                if (!detectSB(any, s.first, s.second, flags)) continue;
                if (s.first * s.second > locCorners) {
                    locCorners = s.first * s.second;
                    locPitch = in.basePitch + dp;
                    locYaw = in.baseYaw + dy;
                }
                break;  // largest size for this framing
            }
        }
    std::cerr << "[auto] locate corners=" << locCorners << " pitch=" << locPitch
              << " yaw=" << locYaw << "\n";
    if (locCorners == 0) return best;  // board not found anywhere

    // Stage B -- MAXIMISE the board size at the located direction, validating at
    // FULL resolution (exactly what the app's detector uses -- an intermediate
    // resolution can spuriously fail where full res succeeds). At a tight (high)
    // zoom the outer ring is clipped and only a sub-block detects, so try low->high
    // zooms (a low zoom leaves margin for the full board) and keep the detection
    // with the MOST corners (prefers the full 7x7 over a 6x6 sub-block).
    const int maxCorners = sizes.front().first * sizes.front().second;
    int bestCorners = 0;
    for (double z : {1.5, 2.0, 2.5, 3.0, 4.0}) {
        cv::Mat mx, my, any;
        in.moil->mapsAnypointMode2(locPitch, locYaw, z, mx, my);  // full resolution
        cv::remap(in.grayFull, any, mx, my, cv::INTER_LINEAR);
        cv::imwrite("/tmp/auto_full_" + in.cam + "_" + in.dir + "_z" +
                        std::to_string(static_cast<int>(z * 10)) + ".png",
                    any);
        for (const auto &s : sizes) {  // descending -> full board first
            if (!detectSB(any, s.first, s.second, flags)) continue;
            if (s.first * s.second > bestCorners) {
                bestCorners = s.first * s.second;
                best.found = true;
                best.pitch = locPitch;
                best.yaw = locYaw;
                best.zoom = z;
                best.cols = s.first;
                best.rows = s.second;
            }
            break;
        }
        if (bestCorners >= maxCorners) break;  // full board found -> can't do better
    }
    std::cerr << "[auto] best " << best.cols << "x" << best.rows << " pitch=" << best.pitch
              << " yaw=" << best.yaw << " zoom=" << best.zoom << " found=" << best.found << "\n";
    return best;
}
}  // namespace

Controller3dMeasurement::Controller3dMeasurement(AxisRosClient *axis, CameraRosClient *cam, QWidget *parent)
    : QDialog(parent),
      ui_(new Ui::sub_dialog_auto_3d_measurement),
      uiFull_(new Ui::Form),
      axis_(axis),
      cam_(cam) {
    ui_->setupUi(this);

    fullscreenWindow_ = new QWidget(this);
    fullscreenWindow_->setWindowFlags(Qt::Window | Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
    uiFull_->setupUi(fullscreenWindow_);

    populateReorderCombos();
    initSpinboxFromDefaultAngles();
    connectBtnEvent();
    connectSpinboxes();

    // Create the GPU 3D view up-front (before the dialog is shown). Adding a
    // QOpenGLWidget to an already-visible window forces the top-level native
    // window to be recreated, which looked like the dialog closing/reopening on
    // the first Calculate. Building it now avoids that.
    ensure3dViewEmbedded();
    ensureOriDetTab();
    // Refresh the ORI_DET panels whenever that tab is shown or the images change.
    if (auto *tw = findChild<QTabWidget *>("tabWidget"))
        connect(tw, &QTabWidget::currentChanged, this, [this](int) { oriShowPanels(); });

    // Make image labels clickable (double-RIGHT-click -> fullscreen).
    for (const auto &cam : kCameras) {
        if (auto *l = findChild<QLabel *>(QString::fromStdString("label_original_" + cam)))
            makeLabelClickable(l);
        if (auto *l = findChild<QLabel *>(QString::fromStdString("label_" + cam + "_result")))
            makeLabelClickable(l);
        if (auto *l = findChild<QLabel *>(QString::fromStdString("label_overlay_original_" + cam)))
            makeLabelClickable(l);
        for (const auto &dir : kDirections)
            if (auto *l = anyLabel(cam, dir)) makeLabelClickable(l);
    }
    makeLabelClickable(uiFull_->label_full_screen);

    // Session cache: buttons + the debounce timer, then offer the autosave once
    // the dialog is actually on screen (a modal question from a constructor
    // would pop up before the window it belongs to).
    buildSessionButtons();
    autosaveTimer_ = new QTimer(this);
    autosaveTimer_->setSingleShot(true);
    autosaveTimer_->setInterval(2000);
    connect(autosaveTimer_, &QTimer::timeout, this, [this] { writeAutosaveNow(); });
    // The restore prompt belongs to showEvent, not here: MainWindow builds this
    // dialog when the calibration app starts, so asking from the constructor put
    // the question on screen before the user had opened 3D verification at all.

    Help::attach(this, "measure3d");
}

Controller3dMeasurement::~Controller3dMeasurement() {
    delete ui_;
    delete uiFull_;
}

int Controller3dMeasurement::posOf(const std::string &camKey) const {
    if (camKey == "left") return LEFT;
    if (camKey == "right") return RIGHT;
    return -1;
}

// ---- widget accessors (by Python object name) ----
QDoubleSpinBox *Controller3dMeasurement::spin(const std::string &axis, const std::string &cam,
                                          const std::string &dir) const {
    return findChild<QDoubleSpinBox *>(
        QString::fromStdString("doubleSpinBox_" + axis + "_" + cam + "_" + dir));
}
QLineEdit *Controller3dMeasurement::pointEdit(const std::string &cam, const std::string &dir) const {
    return findChild<QLineEdit *>(QString::fromStdString("lineEdit_point_" + cam + "_" + dir));
}
QComboBox *Controller3dMeasurement::reorderCombo(const std::string &cam, const std::string &dir) const {
    return findChild<QComboBox *>(QString::fromStdString("comboBox_reorder_" + cam + "_" + dir));
}
QLabel *Controller3dMeasurement::anyLabel(const std::string &cam, const std::string &dir) const {
    return findChild<QLabel *>(QString::fromStdString("label_" + dir + "_" + cam));
}

// ============================ setup ============================

void Controller3dMeasurement::populateReorderCombos() {
    const QStringList items = {"default",   "flip_horizontal", "flip_vertical", "rotate_90",
                               "rotate_180", "rotate_270",      "adaptive"};
    for (const auto &cam : kCameras)
        for (const auto &dir : kDirections)
            if (auto *c = reorderCombo(cam, dir)) {
                if (c->count() == 0) c->addItems(items);
            }
}

void Controller3dMeasurement::initSpinboxFromDefaultAngles() {
    for (const auto &cam : kCameras)
        for (const auto &dir : kDirections) {
            auto *a = spin("alpha", cam, dir);
            auto *b = spin("beta", cam, dir);
            auto *z = spin("zoom", cam, dir);
            if (!a || !b || !z) continue;
            const auto def = kDefaultAngles.at(dir);  // pitch, yaw, zoom
            a->setValue(def[1]);                       // alpha == yaw
            b->setValue(def[0]);                       // beta  == pitch
            z->setValue(def[2]);
        }
}

void Controller3dMeasurement::connectSpinboxes() {
    for (const auto &cam : kCameras)
        for (const auto &dir : kDirections)
            for (const char *axis : {"alpha", "beta", "zoom"})
                if (auto *sb = spin(axis, cam, dir))
                    connect(sb, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                            [this, cam, dir](double) {
                                const int pos = posOf(cam);
                                if (moil_[pos].valid() && !imageSource_[pos].empty())
                                    generateOneAnypointRemap(pos, cam, dir);
                            });
}

void Controller3dMeasurement::connectBtnEvent() {
    ui_->btn_left_image->disconnect();
    connect(ui_->btn_left_image, &QPushButton::clicked, this, [this] { onclickOpenMedia(LEFT); });
    connect(ui_->btn_right_image, &QPushButton::clicked, this, [this] { onclickOpenMedia(RIGHT); });
    // connect(ui_->btn_mid_image, &QPushButton::clicked, this, [this] { onclickOpenMedia(MID); });
    connect(ui_->btn_left_parameter, &QPushButton::clicked, this, [this] { onclickOpenParameter(LEFT); });
    connect(ui_->btn_right_parameter, &QPushButton::clicked, this, [this] { onclickOpenParameter(RIGHT); });
    // connect(ui_->btn_mid_parameter, &QPushButton::clicked, this, [this] { onclickOpenParameter(MID); });

    for (const auto &cam : kCameras)
        for (const auto &dir : kDirections)
            if (auto *b = findChild<QPushButton *>(
                    QString::fromStdString("pushButton_Detect_" + cam + "_" + dir)))
                connect(b, &QPushButton::clicked, this,
                        [this, cam, dir] { autoDetectDirection(cam, dir); });

    for (const auto &cam : kCameras)
        if (auto *b = findChild<QPushButton *>(
                QString::fromStdString("pushButton_Detect_" + cam + "_result")))
            connect(b, &QPushButton::clicked, this, [this, cam] { overlayAllToFisheye(cam); });

    connect(ui_->pushButton_start_calculated, &QPushButton::clicked, this,
            [this] { handleStartCalculation(); });
    // The 3D result now appears automatically in the "3D View" tab after
    // Start Calculation, so the old "Show 3D" buttons were removed from the UI.
}

void Controller3dMeasurement::makeLabelClickable(QLabel *label) {
    if (!label) return;
    label->setAttribute(Qt::WA_TransparentForMouseEvents, false);
    label->setMouseTracking(true);
    label->setFocusPolicy(Qt::StrongFocus);
    label->installEventFilter(this);
    if (uiFull_ && label == uiFull_->label_full_screen)
        label->setToolTip(tr("Double-click to close. Wheel zooms, left drag pans."));
    else
        label->setToolTip(tr("Double-RIGHT-click to open this image fullscreen."));
}

// ============================ pipeline ============================

std::array<double, 3> Controller3dMeasurement::anglesFromUi(const std::string &camKey,
                                                        const std::string &direction) const {
    auto *a = spin("alpha", camKey, direction);
    auto *b = spin("beta", camKey, direction);
    auto *z = spin("zoom", camKey, direction);
    const auto def = kDefaultAngles.at(direction);
    if (!a || !b || !z) return {def[0], def[1], def[2]};
    // get_angles_from_ui: pitch = beta, yaw = alpha.
    return {b->value(), a->value(), z->value()};
}

std::pair<int, int> Controller3dMeasurement::patternSizeFromUi(const std::string &camKey,
                                                          const std::string &direction) const {
    auto *le = pointEdit(camKey, direction);
    if (!le) return {11, 11};
    QString txt = le->text().trimmed().toLower().remove(' ');
    if (txt.contains('x')) {
        const QStringList parts = txt.split('x');
        bool a = false, b = false;
        int cols = parts.value(0).toInt(&a), rows = parts.value(1).toInt(&b);
        if (a && b) return {cols, rows};
    } else {
        bool ok = false;
        int n = txt.toInt(&ok);
        if (ok) return {n, n};
    }
    return {11, 11};
}

void Controller3dMeasurement::onclickOpenMedia(int pos) {
    if (detecting_) return;  // don't reload Moildev/image under a running worker
    const QString img = QFileDialog::getOpenFileName(
        this, tr("Open fisheye image"), "", tr("Images (*.png *.jpg *.jpeg *.bmp);;All Files (*)"));
    if (img.isEmpty()) return;
    imagePath_[pos] = img.toStdString();

    std::string param = paramPath_[pos];
    if (param.empty()) {
        const QString p = QFileDialog::getOpenFileName(this, tr("Select camera parameter JSON"), "",
                                                       tr("JSON (*.json);;All Files (*)"));
        if (p.isEmpty()) return;
        param = p.toStdString();
    }
    // A new image makes the old detections and 3D result stale. Remember which
    // directions were detected, wipe the stale state, load + rebuild the anypoint
    // views, then re-detect those directions and recalculate.
    const std::vector<std::string> hadDetections = detectedDirections(kCameras[pos]);
    invalidateResults(pos);
    readImageSource(pos, imagePath_[pos], param);
    generateAllAnypointForPosition(pos);
    showPathToUi(pos);
    scheduleRedetect(pos, hadDetections);
}

void Controller3dMeasurement::onclickOpenParameter(int pos) {
    if (detecting_) return;  // don't reload Moildev under a running worker
    const QString p = QFileDialog::getOpenFileName(this, tr("Select camera parameter JSON"), "",
                                                   tr("JSON (*.json);;All Files (*)"));
    if (p.isEmpty()) return;
    if (!createMoildev(pos, p.toStdString())) return;  // load failed -> keep old params
    showPathToUi(pos);
    if (imageSource_[pos].empty()) return;  // no image yet: nothing downstream to refresh

    // The new parameters change the anypoint remap, so the old detections
    // (alpha/beta computed with the previous JSON) and the 3D result no longer
    // apply. Wipe them, rebuild the anypoint views with the new parameters, then
    // re-detect the previously-detected directions and recalculate -- this is what
    // makes the result actually update when the JSON is swapped.
    const std::vector<std::string> hadDetections = detectedDirections(kCameras[pos]);
    invalidateResults(pos);
    generateAllAnypointForPosition(pos);
    scheduleRedetect(pos, hadDetections);
}

bool Controller3dMeasurement::createMoildev(int pos, const std::string &paramPath) {
    if (!moil_[pos].load(paramPath)) return false;
    paramPath_[pos] = paramPath;
    return true;
}

void Controller3dMeasurement::readImageSource(int pos, const std::string &imagePath,
                                          const std::string &paramPath) {
    cv::Mat bgr = cv::imread(imagePath, cv::IMREAD_COLOR);
    if (bgr.empty()) return;
    imageSource_[pos] = bgr;
    imagePath_[pos] = imagePath;
    // Reload the Moildev when it isn't loaded yet OR a different parameter file
    // was supplied; otherwise a freshly chosen JSON would be silently ignored.
    if (!moil_[pos].valid() || paramPath != paramPath_[pos]) {
        if (!createMoildev(pos, paramPath)) return;
    } else {
        paramPath_[pos] = paramPath;
    }
    updateResolutionToUi(pos);
    QTimer::singleShot(0, this, [this] { showToUi(); });
}

void Controller3dMeasurement::updateResolutionToUi(int pos) {
    if (imageSource_[pos].empty()) return;
    const std::string key = kCameras[pos];
    if (auto *le = findChild<QLineEdit *>(QString::fromStdString("lineEdit_resolution_" + key)))
        le->setText(QString("%1 x %2 px").arg(imageSource_[pos].cols).arg(imageSource_[pos].rows));
}

void Controller3dMeasurement::generateAllAnypointForPosition(int pos) {
    const std::string cam = kCameras[pos];
    if (!moil_[pos].valid() || imageSource_[pos].empty()) return;
    for (const auto &dir : kDirections) generateOneAnypointRemap(pos, cam, dir);
    showToUi();
}

void Controller3dMeasurement::generateOneAnypointRemap(int pos, const std::string &camKey,
                                                   const std::string &direction) {
    if (!moil_[pos].valid() || imageSource_[pos].empty()) return;
    const auto ang = anglesFromUi(camKey, direction);  // pitch, yaw, zoom
    cv::Mat mapX, mapY;
    moil_[pos].mapsAnypointMode2(ang[0], ang[1], ang[2], mapX, mapY);

    cv::Mat gray;
    cv::cvtColor(imageSource_[pos], gray, cv::COLOR_BGR2GRAY);
    cv::Mat resized;
    cv::resize(gray, resized, cv::Size(mapX.cols, mapX.rows));
    cv::Mat anyGray;
    cv::remap(resized, anyGray, mapX, mapY, cv::INTER_LINEAR);

    grayAnypoint_[camKey][direction] = anyGray;
    anypointMaps_[camKey][direction] = {mapX, mapY};  // reused by Detect
    cv::Mat anyBgr;
    cv::cvtColor(anyGray, anyBgr, cv::COLOR_GRAY2BGR);
    anypointImages_[camKey][direction] = anyBgr;

    if (auto *lbl = anyLabel(camKey, direction))
        show_image_to_label(lbl, anyBgr, lbl->width() > 0 ? lbl->width() : 320);
}

void Controller3dMeasurement::detectCheckerboardAnypoint(const std::string &camKey,
                                                     const std::string &direction) {
    const int pos = posOf(camKey);
    std::cerr << "[auto] detect() entry " << camKey << "/" << direction << "\n";
    auto git = grayAnypoint_.find(camKey);
    if (git == grayAnypoint_.end() || git->second.find(direction) == git->second.end()) {
        std::cerr << "[auto] detect() ABORT: no grayAnypoint\n";
        return;
    }
    if (!moil_[pos].valid() || imageSource_[pos].empty()) {
        std::cerr << "[auto] detect() ABORT: moil/image invalid\n";
        return;
    }
    if (detecting_) {
        std::cerr << "[auto] detect() ABORT: detecting_ still true\n";
        return;  // one detection at a time (shared buffer + output files)
    }

    std::string preprocess = "standard";
    if (camKey == "left") {
        if (auto *r = findChild<QRadioButton *>("radio_preprocess_enhanced_left"))
            preprocess = r->isChecked() ? "enhanced" : "standard";
    } else if (camKey == "right") {
        if (auto *r = findChild<QRadioButton *>("radio_preprocess_enhanced_right"))
            preprocess = r->isChecked() ? "enhanced" : "standard";
    }

    const auto ps = patternSizeFromUi(camKey, direction);
    const auto ang = anglesFromUi(camKey, direction);
    std::string reorder = "default";
    if (auto *c = reorderCombo(camKey, direction)) reorder = c->currentText().toStdString();

    // Reuse the remap tables built when the anypoint view was generated; empty
    // Mats fall back to an internal recompute inside procesAnypoint.
    cv::Mat cachedMapX, cachedMapY;
    if (auto cit = anypointMaps_.find(camKey); cit != anypointMaps_.end()) {
        auto dit = cit->second.find(direction);
        if (dit != cit->second.end()) {
            cachedMapX = dit->second.first;
            cachedMapY = dit->second.second;
        }
    }

    // Snapshot the worker's inputs (cv::Mat headers are cheap, ref-counted). The
    // Moildev is only read on the worker and is never reloaded while detecting_.
    const cv::Mat grayImg = grayAnypoint_[camKey][direction];
    const cv::Mat srcImg = imageSource_[pos];
    const Moildev *moil = &moil_[pos];

    detecting_ = true;

    // Run the heavy findChessboardCornersSB off the UI thread so the window stays
    // responsive; apply the results back on the UI thread when it finishes.
    auto *watcher = new QFutureWatcher<DetectOutput>(this);
    connect(watcher, &QFutureWatcher<DetectOutput>::finished, this, [this, watcher] {
        const DetectOutput o = watcher->result();
        watcher->deleteLater();
        detecting_ = false;

        if (o.res.found) {
            // update_pattern_size_ui (cols/rows line edits, if present).
            if (auto *le = findChild<QLineEdit *>(
                    QString::fromStdString("lineEdit_cols_" + o.camKey + "_" + o.direction)))
                le->setText(QString::number(o.ps.first));
            if (auto *le = findChild<QLineEdit *>(
                    QString::fromStdString("lineEdit_rows_" + o.camKey + "_" + o.direction)))
                le->setText(QString::number(o.ps.second));
        }
        std::cerr << "[auto] full-res detect " << o.camKey << "/" << o.direction
                  << " found=" << o.res.found << " points=" << o.res.points.size() << "\n";
        if (!o.res.empty()) detection_[o.camKey][o.direction] = o.res;
        anypointImages_[o.camKey][o.direction] = o.overlayAny;
        showToUi();
        scheduleAutosave();    // a detection is minutes of work: keep it
        driveRedetectQueue();  // advance the auto re-detect/recalc queue (no-op if idle)
    });

    watcher->setFuture(QtConcurrent::run([=]() -> DetectOutput {
        DetectOutput o;
        o.camKey = camKey;
        o.direction = direction;
        o.ps = ps;
        o.res = AnypointChessboard::procesAnypoint(grayImg, *moil, srcImg, camKey, direction,
                                                   ps.first, ps.second, ang[0], ang[1], ang[2],
                                                   reorder, preprocess, o.overlayAny, o.overlayFish,
                                                   cachedMapX, cachedMapY);
        return o;
    }));
}

cv::Mat Controller3dMeasurement::combineFisheyeOverlay(const std::string &camKey) {
    const int pos = posOf(camKey);
    cv::Mat base = imageSource_[pos].clone();
    const int W0 = base.cols, H0 = base.rows;
    auto dit = detection_.find(camKey);
    if (dit == detection_.end()) return base;

    for (const auto &dirRes : dit->second) {
        const DetectionResult &df = dirRes.second;
        if (df.empty()) continue;
        const int Wf = df.fishW ? df.fishW : W0;
        const int Hf = df.fishH ? df.fishH : H0;
        const double sx = static_cast<double>(W0) / std::max(1, Wf);
        const double sy = static_cast<double>(H0) / std::max(1, Hf);
        for (const auto &p : df.points) {
            if (p.xFish < 0 || p.yFish < 0) continue;
            const int x = static_cast<int>(std::lround(p.xFish * sx));
            const int y = static_cast<int>(std::lround(p.yFish * sy));
            if (x >= 0 && x < W0 && y >= 0 && y < H0)
                cv::drawMarker(base, {x, y}, cv::Scalar(0, 0, 255), cv::MARKER_TILTED_CROSS, 30, 2);
            const std::string t = std::to_string(p.pointId);
            cv::putText(base, t, {x + 6, y - 6}, cv::FONT_HERSHEY_SIMPLEX, 0.5,
                        cv::Scalar(255, 255, 255), 3, cv::LINE_AA);
            cv::putText(base, t, {x + 6, y - 6}, cv::FONT_HERSHEY_SIMPLEX, 0.5,
                        cv::Scalar(0, 0, 255), 1, cv::LINE_AA);
        }
    }
    return base;
}

void Controller3dMeasurement::overlayAllToFisheye(const std::string &camKey) {
    if (detecting_) return;  // flushes the shared buffer a detection may be writing
    const int pos = posOf(camKey);
    if (imageSource_[pos].empty()) return;
    cv::Mat overlay = combineFisheyeOverlay(camKey);
    resultDraw_[pos] = overlay;
    const std::string outDir = Measure3d::getOutputDir(camKey);
    cv::imwrite(outDir + "/" + camKey + "_overlay_result.png", overlay);
    AnypointChessboard::saveCombinedAlphaBeta(camKey);
    showToUi();
}

void Controller3dMeasurement::handleStartCalculation() {
    if (detecting_) return;  // needs the completed detections; wait for the worker
    if (auto *r = findChild<QRadioButton *>("radioButton_2_camera"))
        if (!r->isChecked()) return;

    auto gather = [this](const std::string &cam) {
        std::vector<DetPoint> out;
        auto it = detection_.find(cam);
        if (it == detection_.end()) return out;
        for (const auto &dirRes : it->second)
            for (const auto &p : dirRes.second.points)
                if (p.hasAlphaBeta()) out.push_back(p);
        return out;
    };
    const std::vector<DetPoint> dfL = gather("left");
    const std::vector<DetPoint> dfR = gather("right");
    if (dfL.empty() || dfR.empty()) return;  // need detected points on both cameras

    auto posVal = [this](const char *axis, int pos) -> double {
        auto *sb = findChild<QSpinBox *>(
            QString("spinBox_%1_%2").arg(axis).arg(QString::fromStdString(kCameras[pos])));
        return sb ? sb->value() : 0.0;
    };
    const Vec3 camL(posVal("x", LEFT), posVal("y", LEFT), posVal("z", LEFT));
    const Vec3 camR(posVal("x", RIGHT), posVal("y", RIGHT), posVal("z", RIGHT));

    const std::vector<Point3d> df3d =
        AnypointChessboard::compute3dPoints(dfL, dfR, camL, camR, "lr", nullptr);
    if (df3d.empty()) return;  // triangulation produced no points

    // Populate the "3D View" tab automatically. Do NOT switch to it here: the
    // user opens the tab when they want, so Start Calculation doesn't jump the UI.
    points3d_ = df3d;
    camL3d_ = camL;
    camR3d_ = camR;
    ensure3dViewEmbedded();
    // FOV rings come from the loaded parameter JSONs, not from a typed guess.
    glView_->setCameraFov(moil_[LEFT].cameraFov(), moil_[RIGHT].cameraFov());
    glView_->setPoints(points3d_, camL3d_, camR3d_);

    // _pattern_size_for_show_from('left'): swap to match Python.
    std::map<std::string, std::pair<int, int>> patternForShow;
    for (const auto &d : kDirections) {
        const auto cr = patternSizeFromUi("left", d);  // (cols, rows)
        patternForShow[d] = {cr.second, cr.first};     // Python out[d] = (cols, rows) after unpack swap
    }

    std::vector<Point3d> df3dCopy = df3d;
    PlaneFit3dViz::VizResult vr =
        PlaneFit3dViz::show3dPoint2camOriVisualization(df3dCopy, camL, camR, &patternForShow);

    // Kept as well as displayed, so a restored session can show these numbers
    // without re-running the calculation.
    lastAngle_ = vr.angleMap;
    lastInterRay_ = vr.meanMaps;
    lastThickness_ = vr.meanPlaneDist;
    lastDepth_ = vr.depthOriginPerDir;
    updateAngleToUi(vr.angleMap);
    updateMeanDistToUi(vr.meanMaps);
    updatePlaneDistToUi(vr.meanPlaneDist);
    updateDepthToUi(vr.depthOriginPerDir);

    PlaneFit3dViz::ReprojCompare rc = PlaneFit3dViz::compareReprojectionWithOriginal(
        df3d, dfL, dfR, camL, camR, &moil_[LEFT], &moil_[RIGHT], nullptr);

    auto meanRms = [](const std::vector<Measure3d::ReprojRow> &rows, double &mean, double &rms) {
        double s = 0, sq = 0;
        int n = 0;
        for (const auto &r : rows)
            if (!std::isnan(r.error)) {
                s += r.error;
                sq += r.error * r.error;
                ++n;
            }
        mean = n ? s / n : Measure3d::nan();
        rms = n ? std::sqrt(sq / n) : Measure3d::nan();
    };
    if (!rc.left.empty() || !rc.right.empty()) {
        double mL, rL, mR, rR;
        meanRms(rc.left, mL, rL);
        meanRms(rc.right, mR, rR);
        lastMeanErrL_ = mL;
        lastRmsL_ = rL;
        lastMeanErrR_ = mR;
        lastRmsR_ = rR;
        auto setf = [this](const char *name, double v) {
            if (auto *e = findChild<QLineEdit *>(name))
                e->setText(std::isnan(v) ? "N/A" : QString::number(v, 'f', 2));
        };
        setf("lineEdit_means_error_left_ori", mL);
        setf("lineEdit_rms_left_ori", rL);
        setf("lineEdit_means_error_right_ori", mR);
        setf("lineEdit_rms_right_ori", rR);

        overlayReprojection(LEFT, rc.left);
        overlayReprojection(RIGHT, rc.right);
        showToUi();
    }
    scheduleAutosave();  // a finished calculation is worth remembering
}

// ============================ session cache ============================

// The Save/Load buttons prefer widgets drawn in the .ui (btn_save_session_3d /
// btn_load_session_3d). When they are not there they are built here, so the
// feature works today and moves to the designed layout the moment those objects
// exist -- delete nothing, just draw them.
void Controller3dMeasurement::buildSessionButtons() {
    btnSaveSession_ = findChild<QPushButton *>("btn_save_session_3d");
    btnLoadSession_ = findChild<QPushButton *>("btn_load_session_3d");
    const bool fromUi = (btnSaveSession_ && btnLoadSession_);

    if (!fromUi) {
        auto *row = new QHBoxLayout();
        btnSaveSession_ = new QPushButton(tr("Save session (.yaml)"), this);
        btnLoadSession_ = new QPushButton(tr("Load session (.yaml)"), this);
        sessionStatus_ = new QLabel(this);
        sessionStatus_->setStyleSheet("color:#555; font-size:12px;");
        row->addWidget(btnSaveSession_);
        row->addWidget(btnLoadSession_);
        row->addWidget(sessionStatus_, 1);
        if (ui_->verticalLayout_6) ui_->verticalLayout_6->insertLayout(0, row);
    }
    btnSaveSession_->setToolTip(tr("Write everything in this dialog to a YAML file: image and "
                                   "parameter paths, camera positions, every detection, the "
                                   "ORI_DET clicks and recovered grids, the survey and the 3D "
                                   "results. Images are referenced by path, not embedded."));
    btnLoadSession_->setToolTip(tr("Restore a saved session. The images are re-read from the "
                                   "paths in the file; anything missing is reported and the rest "
                                   "still loads."));
    connect(btnSaveSession_, &QPushButton::clicked, this, [this] { onclickSaveSession(); });
    connect(btnLoadSession_, &QPushButton::clicked, this, [this] { onclickLoadSession(); });
}

// Redraw a stored detection onto its (freshly remapped) anypoint view, in the
// style AnypointChessboard uses for its own overlay, so a restored session shows
// the corners without re-running Detect. xRect/yRect are the anypoint pixel
// coordinates the detector measured in, which is exactly this image's frame.
void Controller3dMeasurement::drawDetectionOnAnypoint(const std::string &camKey,
                                                      const std::string &direction) {
    auto cit = detection_.find(camKey);
    if (cit == detection_.end()) return;
    auto dit = cit->second.find(direction);
    if (dit == cit->second.end() || dit->second.empty()) return;
    auto iit = anypointImages_.find(camKey);
    if (iit == anypointImages_.end()) return;
    auto mit = iit->second.find(direction);
    if (mit == iit->second.end() || mit->second.empty()) return;

    cv::Mat img = mit->second;  // drawn in place; regenerated on the next remap
    for (size_t idx = 0; idx < dit->second.points.size(); ++idx) {
        const auto &p = dit->second.points[idx];
        const cv::Point q(static_cast<int>(p.xRect), static_cast<int>(p.yRect));
        if (q.x < 0 || q.y < 0 || q.x >= img.cols || q.y >= img.rows) continue;
        cv::drawMarker(img, q, cv::Scalar(0, 0, 255), cv::MARKER_TILTED_CROSS, 20, 2);
        cv::putText(img, std::to_string(idx), cv::Point(q.x + 5, q.y - 5),
                    cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(0, 0, 255), 1);
    }
    if (auto *lbl = anyLabel(camKey, direction))
        show_image_to_label(lbl, img, lbl->width() > 0 ? lbl->width() : 320);
}

Measure3d::Session Controller3dMeasurement::captureSession() const {
    Measure3d::Session s;
    s.savedAt = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss").toStdString();
    if (auto *r = findChild<QRadioButton *>("radioButton_2_camera")) s.twoCamera = r->isChecked();

    for (int pos = 0; pos < 2; ++pos) {
        Measure3d::SessionCamera c;
        c.side = kCameras[pos];
        c.imagePath = imagePath_[pos];
        c.parameterPath = paramPath_[pos];
        const QFileInfo fi(QString::fromStdString(c.imagePath));
        if (fi.exists()) {
            c.imageBytes = fi.size();
            c.imageMtime = fi.lastModified().toSecsSinceEpoch();
        }
        auto posVal = [this](const char *axis, int p) -> double {
            auto *sb = findChild<QSpinBox *>(
                QString("spinBox_%1_%2").arg(axis).arg(QString::fromStdString(kCameras[p])));
            return sb ? sb->value() : 0.0;
        };
        c.posX = posVal("x", pos);
        c.posY = posVal("y", pos);
        c.posZ = posVal("z", pos);
        if (auto *r = findChild<QRadioButton *>(
                QString::fromStdString("radio_preprocess_enhanced_" + kCameras[pos])))
            c.preprocess = r->isChecked() ? "enhanced" : "standard";
        for (const auto &dir : kDirections) {
            Measure3d::SessionDirection d;
            d.direction = dir;
            if (auto *a = spin("alpha", kCameras[pos], dir)) d.alpha = a->value();
            if (auto *b = spin("beta", kCameras[pos], dir)) d.beta = b->value();
            if (auto *z = spin("zoom", kCameras[pos], dir)) d.zoom = z->value();
            if (auto *le = pointEdit(kCameras[pos], dir)) d.pattern = le->text().toStdString();
            if (auto *cb = reorderCombo(kCameras[pos], dir))
                d.reorder = cb->currentText().toStdString();
            c.directions.push_back(d);
        }
        s.cameras.push_back(c);
    }

    s.detections = detection_;

    s.points3d = points3d_;
    s.camL3d = camL3d_;
    s.camR3d = camR3d_;
    s.angle = lastAngle_;
    s.interRay = lastInterRay_;
    s.thickness = lastThickness_;
    s.depth = lastDepth_;
    s.meanErrLeft = lastMeanErrL_;
    s.rmsLeft = lastRmsL_;
    s.meanErrRight = lastMeanErrR_;
    s.rmsRight = lastRmsR_;

    s.oriMethod = static_cast<int>(oriMethod_);
    s.oriRowsHint = oriRows_ ? oriRows_->value() : 6;
    s.oriColsHint = oriCols_ ? oriCols_->value() : 9;
    s.oriIndexMode = oriIndexMode_;
    s.oriStepPlane = oriStepPlane_;
    s.oriStepCam = oriStepCam_;
    for (const auto &plane : kOriPlaneOrder) {
        Measure3d::SessionOriPlane p;
        p.plane = plane;
        bool any = false;
        auto cit = oriClicks_.find(plane);
        if (cit != oriClicks_.end()) {
            p.clicks[LEFT] = cit->second[LEFT];
            p.clicks[RIGHT] = cit->second[RIGHT];
            any = any || !p.clicks[LEFT].empty() || !p.clicks[RIGHT].empty();
        }
        auto rit = oriRecovered_.find(plane);
        if (rit != oriRecovered_.end()) {
            p.recovered[LEFT] = rit->second[LEFT];
            p.recovered[RIGHT] = rit->second[RIGHT];
            any = any || !p.recovered[LEFT].corners.empty() || !p.recovered[RIGHT].corners.empty();
        }
        auto bit = oriBoundary_.find(plane);
        if (bit != oriBoundary_.end()) {
            p.boundary[LEFT] = bit->second[LEFT];
            p.boundary[RIGHT] = bit->second[RIGHT];
        }
        if (any) s.oriPlanes.push_back(p);
    }
    for (int side = 0; side < 2; ++side) {
        for (const auto &l : oriSurvey_[side]) {
            Measure3d::SessionSurveyLayer sl;
            sl.name = l.name;
            sl.points = l.points;
            sl.boards = l.boards;
            sl.ms = l.ms;
            sl.structured = l.structured;
            s.survey[side].push_back(sl);
        }
        s.oriReproj[side] = oriReproj_[side];
    }
    s.oriPoints3d = oriPoints3d_;
    s.oriCamL = oriCamL_;
    s.oriCamR = oriCamR_;
    s.oriAngle = oriAngle_;
    s.oriInterRay = oriInterRay_;
    s.oriThickness = oriThickness_;
    s.oriDepth = oriDepth_;
    s.oriMeanErrLeft = oriMeanErrL_;
    s.oriRmsLeft = oriRmsL_;
    s.oriMeanErrRight = oriMeanErrR_;
    s.oriRmsRight = oriRmsR_;
    return s;
}

void Controller3dMeasurement::applySession(const Measure3d::Session &s) {
    if (detecting_) return;  // a worker owns the detection buffers right now
    restoring_ = true;       // no autosaves while we churn the widgets

    if (auto *r = findChild<QRadioButton *>("radioButton_2_camera")) {
        QSignalBlocker block(r);
        r->setChecked(s.twoCamera);  // Start Calculation is gated on this
    }

    QStringList missing;
    for (const auto &c : s.cameras) {
        const int pos = posOf(c.side);
        if (pos < 0) continue;

        // Spin boxes / line edits first, blocked so each setValue does not kick
        // off an anypoint remap we are about to do once anyway.
        auto setPos = [this](const char *axis, int p, double v) {
            if (auto *sb = findChild<QSpinBox *>(
                    QString("spinBox_%1_%2").arg(axis).arg(QString::fromStdString(kCameras[p])))) {
                QSignalBlocker block(sb);
                sb->setValue(static_cast<int>(std::lround(v)));
            }
        };
        setPos("x", pos, c.posX);
        setPos("y", pos, c.posY);
        setPos("z", pos, c.posZ);
        if (auto *r = findChild<QRadioButton *>(
                QString::fromStdString("radio_preprocess_enhanced_" + c.side))) {
            QSignalBlocker block(r);
            r->setChecked(c.preprocess == "enhanced");
        }
        for (const auto &d : c.directions) {
            if (auto *a = spin("alpha", c.side, d.direction)) { QSignalBlocker b(a); a->setValue(d.alpha); }
            if (auto *b = spin("beta", c.side, d.direction)) { QSignalBlocker bb(b); b->setValue(d.beta); }
            if (auto *z = spin("zoom", c.side, d.direction)) { QSignalBlocker bz(z); z->setValue(d.zoom); }
            if (auto *le = pointEdit(c.side, d.direction))
                le->setText(QString::fromStdString(d.pattern));
            if (auto *cb = reorderCombo(c.side, d.direction)) {
                QSignalBlocker b(cb);
                const int idx = cb->findText(QString::fromStdString(d.reorder));
                if (idx >= 0) cb->setCurrentIndex(idx);
            }
        }

        // Images are referenced, not stored: re-read them from their paths.
        if (!c.parameterPath.empty() && QFileInfo::exists(QString::fromStdString(c.parameterPath)))
            createMoildev(pos, c.parameterPath);
        else if (!c.parameterPath.empty())
            missing << QString::fromStdString(c.parameterPath);

        if (!c.imagePath.empty() && QFileInfo::exists(QString::fromStdString(c.imagePath))) {
            cv::Mat bgr = cv::imread(c.imagePath, cv::IMREAD_COLOR);
            if (bgr.empty()) {
                missing << QString::fromStdString(c.imagePath);
            } else {
                imageSource_[pos] = bgr;
                imagePath_[pos] = c.imagePath;
                updateResolutionToUi(pos);
                if (moil_[pos].valid()) generateAllAnypointForPosition(pos);
            }
        } else if (!c.imagePath.empty()) {
            missing << QString::fromStdString(c.imagePath);
        }
        showPathToUi(pos);
    }

    detection_ = s.detections;
    // The anypoint views were just rebuilt from the raw remap, so they show no
    // corners. Redraw the stored detections onto them (same marker style the
    // detector uses) -- a restored session must look detected, not blank.
    for (int pos = 0; pos < 2; ++pos) {
        if (imageSource_[pos].empty()) continue;
        for (const auto &dir : kDirections) drawDetectionOnAnypoint(kCameras[pos], dir);
        if (detection_.count(kCameras[pos]))
            resultDraw_[pos] = combineFisheyeOverlay(kCameras[pos]);
    }

    // ---- anypoint result: shown as stored, not recomputed ----
    points3d_ = s.points3d;
    camL3d_ = s.camL3d;
    camR3d_ = s.camR3d;
    lastAngle_ = s.angle;
    lastInterRay_ = s.interRay;
    lastThickness_ = s.thickness;
    lastDepth_ = s.depth;
    lastMeanErrL_ = s.meanErrLeft;
    lastRmsL_ = s.rmsLeft;
    lastMeanErrR_ = s.meanErrRight;
    lastRmsR_ = s.rmsRight;
    ensure3dViewEmbedded();
    glView_->setCameraFov(moil_[LEFT].cameraFov(), moil_[RIGHT].cameraFov());
    glView_->setPoints(points3d_, camL3d_, camR3d_);
    updateAngleToUi(lastAngle_);
    updateMeanDistToUi(lastInterRay_);
    updatePlaneDistToUi(lastThickness_);
    updateDepthToUi(lastDepth_);
    auto setf = [this](const char *name, double v) {
        if (auto *e = findChild<QLineEdit *>(name))
            e->setText(std::isnan(v) ? "N/A" : QString::number(v, 'f', 2));
    };
    setf("lineEdit_means_error_left_ori", lastMeanErrL_);
    setf("lineEdit_rms_left_ori", lastRmsL_);
    setf("lineEdit_means_error_right_ori", lastMeanErrR_);
    setf("lineEdit_rms_right_ori", lastRmsR_);

    // ---- ORI_DET ----
    ensureOriDetTab();
    oriClicks_.clear();
    oriRecovered_.clear();
    oriBoundary_.clear();
    for (const auto &p : s.oriPlanes) {
        oriClicks_[p.plane][LEFT] = p.clicks[LEFT];
        oriClicks_[p.plane][RIGHT] = p.clicks[RIGHT];
        oriRecovered_[p.plane][LEFT] = p.recovered[LEFT];
        oriRecovered_[p.plane][RIGHT] = p.recovered[RIGHT];
        oriBoundary_[p.plane][LEFT] = p.boundary[LEFT];
        oriBoundary_[p.plane][RIGHT] = p.boundary[RIGHT];
    }
    oriStepPlane_ = s.oriStepPlane;
    oriStepCam_ = s.oriStepCam;
    oriMethod_ = static_cast<PanelCorner::Method>(s.oriMethod);
    if (oriMethodCombo_) {
        QSignalBlocker b(oriMethodCombo_);
        const int idx = oriMethodCombo_->findData(s.oriMethod);
        if (idx >= 0) oriMethodCombo_->setCurrentIndex(idx);
    }
    if (oriRows_) { QSignalBlocker b(oriRows_); oriRows_->setValue(s.oriRowsHint); }
    if (oriCols_) { QSignalBlocker b(oriCols_); oriCols_->setValue(s.oriColsHint); }
    oriIndexMode_ = s.oriIndexMode;
    if (oriIndexModeCombo_) {
        QSignalBlocker b(oriIndexModeCombo_);
        const int idx = oriIndexModeCombo_->findData(s.oriIndexMode);
        if (idx >= 0) oriIndexModeCombo_->setCurrentIndex(idx);
    }
    for (int side = 0; side < 2; ++side) {
        oriSurvey_[side].clear();
        for (const auto &l : s.survey[side]) {
            PanelCorner::SurveyLayer sl;
            sl.name = l.name;
            sl.points = l.points;
            sl.boards = l.boards;
            sl.ms = l.ms;
            sl.structured = l.structured;
            oriSurvey_[side].push_back(sl);
        }
        oriReproj_[side] = s.oriReproj[side];
    }
    oriSurveyRefreshLegend();
    if (oriSurveyLayerCombo_) {
        QSignalBlocker b(oriSurveyLayerCombo_);
        oriSurveyLayerCombo_->setEnabled(!oriSurvey_[LEFT].empty() || !oriSurvey_[RIGHT].empty());
    }

    oriPoints3d_ = s.oriPoints3d;
    oriCamL_ = s.oriCamL;
    oriCamR_ = s.oriCamR;
    oriAngle_ = s.oriAngle;
    oriInterRay_ = s.oriInterRay;
    oriThickness_ = s.oriThickness;
    oriDepth_ = s.oriDepth;
    oriMeanErrL_ = s.oriMeanErrLeft;
    oriRmsL_ = s.oriRmsLeft;
    oriMeanErrR_ = s.oriMeanErrRight;
    oriRmsR_ = s.oriRmsRight;
    if (!oriPoints3d_.empty()) {
        ensureOriResultTab();
        if (oriGlView_) {
            oriGlView_->setCameraFov(moil_[LEFT].cameraFov(), moil_[RIGHT].cameraFov());
            oriGlView_->setPoints(oriPoints3d_, oriCamL_, oriCamR_);
        }
        oriShowResults(oriAngle_, oriInterRay_, oriThickness_, oriDepth_, oriMeanErrL_, oriRmsL_,
                       oriMeanErrR_, oriRmsR_);
    }
    oriShowPanels();
    oriUpdateCounts();
    oriUpdatePrompt(false);
    showToUi();

    restoring_ = false;

    // Recalculate straight away when the restored detections allow it: pressing
    // Start Calculation again would only reproduce what was just loaded, and it
    // also refreshes the reprojection overlays, which are not stored. This is
    // also what fills the 3D view when the session was saved before its
    // calculation ran.
    const bool bothDetected =
        !detectedDirections("left").empty() && !detectedDirections("right").empty();
    if (bothDetected && moil_[LEFT].valid() && moil_[RIGHT].valid()) handleStartCalculation();

    if (sessionStatus_)
        sessionStatus_->setText(tr("restored %1 — %2")
                                    .arg(QString::fromStdString(s.savedAt),
                                         QString::fromStdString(s.summary())));
    if (!missing.isEmpty())
        QMessageBox::warning(this, tr("Load session"),
                             tr("Restored, but these files are missing or unreadable:\n%1\n\n"
                                "Everything that did not need them was still loaded.")
                                 .arg(missing.join("\n")));
}

void Controller3dMeasurement::scheduleAutosave() {
    if (restoring_ || !autosaveTimer_) return;
    autosaveTimer_->start();  // restarts the 2 s debounce
}

void Controller3dMeasurement::writeAutosaveNow() {
    if (detecting_ || restoring_) return;
    const Measure3d::Session s = captureSession();
    if (s.empty()) return;  // nothing worth remembering yet
    Measure3d::saveSession(s, Measure3d::autosaveSessionPath());
}

void Controller3dMeasurement::onclickSaveSession() {
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Save 3D session"), QString::fromStdString(Measure3d::autosaveSessionPath()),
        tr("Session cache (*.yaml *.json *.xml);;All Files (*)"));
    if (path.isEmpty()) return;
    const Measure3d::Session s = captureSession();
    if (!Measure3d::saveSession(s, path.toStdString())) {
        QMessageBox::warning(this, tr("Save session"), tr("Could not write %1.").arg(path));
        return;
    }
    if (sessionStatus_) sessionStatus_->setText(tr("saved %1").arg(path));
}

void Controller3dMeasurement::onclickLoadSession() {
    if (detecting_) return;
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Load 3D session"), QString::fromStdString(Measure3d::autosaveSessionPath()),
        tr("Session cache (*.yaml *.json *.xml);;All Files (*)"));
    if (path.isEmpty()) return;
    Measure3d::Session s;
    if (!Measure3d::loadSession(s, path.toStdString())) {
        QMessageBox::warning(this, tr("Load session"), tr("Could not read %1.").arg(path));
        return;
    }
    QApplication::setOverrideCursor(Qt::WaitCursor);
    applySession(s);
    QApplication::restoreOverrideCursor();
}

// Asked once per opening, as agreed: the last session is offered, never forced.
void Controller3dMeasurement::offerRestoreOnOpen() {
    const std::string path = Measure3d::autosaveSessionPath();
    if (!QFileInfo::exists(QString::fromStdString(path))) return;
    Measure3d::Session s;
    if (!Measure3d::loadSession(s, path) || s.empty()) return;

    const auto answer = QMessageBox::question(
        this, tr("Restore last session?"),
        tr("A session from %1 was found:\n%2\n\nRestore it?")
            .arg(QString::fromStdString(s.savedAt), QString::fromStdString(s.summary())),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (answer != QMessageBox::Yes) return;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    applySession(s);
    QApplication::restoreOverrideCursor();
}

// The dialog is constructed with the main window but only shown when the user
// presses "3D verification", so this is the first moment the question makes
// sense. Asked once per app run, not on every re-show.
void Controller3dMeasurement::showEvent(QShowEvent *event) {
    QDialog::showEvent(event);
    if (restorePromptShown_) return;
    restorePromptShown_ = true;
    QTimer::singleShot(0, this, [this] { offerRestoreOnOpen(); });
}

void Controller3dMeasurement::closeEvent(QCloseEvent *event) {
    if (autosaveTimer_) autosaveTimer_->stop();
    writeAutosaveNow();  // the dialog is closing: write now, not in 2 s
    QDialog::closeEvent(event);
}

// ============================ invalidation + auto refresh ============================

std::vector<std::string> Controller3dMeasurement::detectedDirections(const std::string &cam) const {
    std::vector<std::string> out;
    auto it = detection_.find(cam);
    if (it == detection_.end()) return out;
    for (const auto &kv : it->second)
        if (!kv.second.empty()) out.push_back(kv.first);
    return out;
}

// Blank every widget/state derived from the (joint) 3D result. Called whenever
// either camera's image or parameters change, since the result depends on both
// cameras' detections and would otherwise keep showing stale numbers.
void Controller3dMeasurement::clearResultUi() {
    points3d_.clear();
    updateAngleToUi({});
    updateMeanDistToUi({});
    updatePlaneDistToUi({});
    updateDepthToUi({});
    for (const char *n : {"lineEdit_means_error_left_ori", "lineEdit_rms_left_ori",
                          "lineEdit_means_error_right_ori", "lineEdit_rms_right_ori"})
        if (auto *e = findChild<QLineEdit *>(n)) e->setText("N/A");
    for (int pos = 0; pos < 2; ++pos) resultDrawOverOri_[pos].release();
    for (const auto &cam : kCameras)
        if (auto *l = findChild<QLabel *>(QString::fromStdString("label_overlay_original_" + cam)))
            l->clear();
    if (glView_) glView_->setPoints({}, camL3d_, camR3d_);  // safe with no points (axes only)
}

// Drop the stale detections and result overlay for one camera plus the shared 3D
// result, so nothing computed with the old image/parameters is left showing.
void Controller3dMeasurement::invalidateResults(int pos) {
    detection_.erase(kCameras[pos]);
    resultDraw_[pos].release();
    if (auto *l = findChild<QLabel *>(QString::fromStdString("label_" + kCameras[pos] + "_result")))
        l->clear();
    clearResultUi();
}

// Queue the given directions for re-detection on the changed camera and request a
// recalculation once they finish. driveRedetectQueue does the actual sequencing.
void Controller3dMeasurement::scheduleRedetect(int pos, const std::vector<std::string> &dirs) {
    if (dirs.empty()) return;  // nothing was detected before -> nothing to refresh
    for (const auto &d : dirs) pendingRedetect_.emplace_back(kCameras[pos], d);
    recalcAfterRedetect_ = true;
    driveRedetectQueue();
}

// Run one queued detection at a time (each spawns a worker whose finished-callback
// calls back here). When the queue drains, recalculate if it was requested.
void Controller3dMeasurement::driveRedetectQueue() {
    if (detecting_) return;  // a worker is running; it will call us again when done
    while (!pendingRedetect_.empty()) {
        const auto job = pendingRedetect_.front();
        pendingRedetect_.erase(pendingRedetect_.begin());
        detectCheckerboardAnypoint(job.first, job.second);
        if (detecting_) return;  // worker started; resume from its finished-callback
        // else the detection could not start (missing anypoint, etc.) -> try next
    }
    if (recalcAfterRedetect_) {
        recalcAfterRedetect_ = false;
        handleStartCalculation();
    }
}

void Controller3dMeasurement::autoDetectDirection(const std::string &cam, const std::string &dir) {
    if (detecting_) return;
    const int pos = posOf(cam);
    if (pos < 0 || !moil_[pos].valid() || imageSource_[pos].empty()) {
        QMessageBox::information(this, tr("Auto Detect"),
                                tr("Load an image + parameter JSON first."));
        return;
    }

    // Board size from this direction's "poin" field; if empty/invalid, ask once.
    auto parseSize = [](const QString &s, int &c, int &r) {
        c = r = 0;
        QString t = s.trimmed().toLower().remove(' ');
        if (t.contains('x')) {
            const QStringList p = t.split('x');
            bool a = false, b = false;
            const int cc = p.value(0).toInt(&a), rr = p.value(1).toInt(&b);
            if (a && b) { c = cc; r = rr; }
        } else {
            bool a = false;
            const int n = t.toInt(&a);
            if (a) c = r = n;
        }
    };
    int bc = 0, br = 0;
    if (auto *le = pointEdit(cam, dir)) parseSize(le->text(), bc, br);
    if (bc < 2 || br < 2) {
        bool ok = false;
        const QString t = QInputDialog::getText(
            this, tr("Auto Detect"),
            tr("Number of board squares (inner corners) — columns x rows, e.g. 6x5:"),
            QLineEdit::Normal, autoBoardSize_, &ok);
        if (!ok) return;
        parseSize(t, bc, br);
        if (bc < 2 || br < 2) {
            QMessageBox::warning(this, tr("Auto Detect"), tr("Invalid board size. Example: 6x5"));
            return;
        }
    }
    autoBoardSize_ = QString("%1x%2").arg(bc).arg(br);
    if (auto *le = pointEdit(cam, dir)) le->setText(autoBoardSize_);

    const int w = moil_[pos].imageWidth(), h = moil_[pos].imageHeight();
    cv::Mat gray, grayFull;
    cv::cvtColor(imageSource_[pos], gray, cv::COLOR_BGR2GRAY);
    cv::resize(gray, grayFull, cv::Size(w, h));

    AutoInput in;
    in.cam = cam;
    in.dir = dir;
    in.moil = &moil_[pos];
    in.grayFull = grayFull;
    if (auto *sb = spin("beta", cam, dir)) in.basePitch = sb->value();   // pitch == beta spin
    if (auto *sy = spin("alpha", cam, dir)) in.baseYaw = sy->value();    // yaw   == alpha spin
    if (auto *sz = spin("zoom", cam, dir)) in.baseZoom = sz->value();
    in.fixedCols = bc;
    in.fixedRows = br;

    std::cerr << "[auto] autoDetectDirection " << cam << "/" << dir << " poin=" << bc << "x" << br
              << "\n";
    detecting_ = true;  // block reloads + other detects while the search runs
    QApplication::setOverrideCursor(Qt::WaitCursor);

    auto *watcher = new QFutureWatcher<AutoFrame>(this);
    connect(watcher, &QFutureWatcher<AutoFrame>::finished, this, [this, watcher] {
        const AutoFrame r = watcher->result();
        watcher->deleteLater();
        detecting_ = false;
        QApplication::restoreOverrideCursor();

        if (!r.found) {
            QMessageBox::information(
                this, tr("Auto Detect"),
                tr("Board %1 not found for direction '%2'.\n"
                   "Check the number of squares (points) or set zoom/alpha-beta manually.")
                    .arg(autoBoardSize_, QString::fromStdString(r.dir)));
            return;
        }
        const int pos = posOf(r.cam);
        if (pos < 0) return;
        // Apply the winning view (blocked so setValue doesn't re-trigger a remap)
        // then rebuild the anypoint once and run the full-resolution detect.
        if (auto *sy = spin("alpha", r.cam, r.dir)) { QSignalBlocker b(sy); sy->setValue(r.yaw); }
        if (auto *sb = spin("beta", r.cam, r.dir)) { QSignalBlocker b(sb); sb->setValue(r.pitch); }
        if (auto *sz = spin("zoom", r.cam, r.dir)) { QSignalBlocker b(sz); sz->setValue(r.zoom); }
        if (auto *le = pointEdit(r.cam, r.dir)) le->setText(QString("%1x%2").arg(r.cols).arg(r.rows));
        generateOneAnypointRemap(pos, r.cam, r.dir);
        const bool haveGray =
            grayAnypoint_.count(r.cam) && grayAnypoint_[r.cam].count(r.dir) &&
            !grayAnypoint_[r.cam][r.dir].empty();
        std::cerr << "[auto] apply " << r.cam << "/" << r.dir << " " << r.cols << "x" << r.rows
                  << " pitch=" << r.pitch << " yaw=" << r.yaw << " zoom=" << r.zoom
                  << " haveGray=" << haveGray << " detecting=" << detecting_ << "\n";
        pendingRedetect_.emplace_back(r.cam, r.dir);
        recalcAfterRedetect_ = false;  // 1-by-1: detect this direction only, no auto recalc
        driveRedetectQueue();
    });

    watcher->setFuture(QtConcurrent::run([in]() -> AutoFrame { return searchOneDirection(in); }));
}

void Controller3dMeasurement::overlayReprojection(int pos, const std::vector<Measure3d::ReprojRow> &rows) {
    cv::Mat base = !resultDraw_[pos].empty() ? resultDraw_[pos].clone() : imageSource_[pos].clone();
    if (base.empty()) return;
    for (const auto &r : rows)
        if (!std::isnan(r.u) && !std::isnan(r.v))
            cv::circle(base, {static_cast<int>(r.u), static_cast<int>(r.v)}, 3,
                       cv::Scalar(255, 0, 0), -1);
    resultDrawOverOri_[pos] = base;
}

void Controller3dMeasurement::ensure3dViewEmbedded() {
    if (glView_) return;
    // The "3D View" tab is an empty QWidget in the .ui; give it a layout and drop
    // the GPU viewer in so it fills the tab.
    glView_ = new Point3dGlView(ui_->tab);
    auto *layout = new QVBoxLayout(ui_->tab);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(glView_);
}

// ============================ ORI_DET: 4-point plane calibration ============================

void Controller3dMeasurement::ensureOriDetTab() {
    if (oriLabel_[LEFT]) return;  // built already
    QWidget *tab = ui_->tab_2;
    if (!tab) return;

    auto *root = new QVBoxLayout(tab);

    oriStatus_ = new QLabel(tab);
    oriStatus_->setWordWrap(true);
    oriStatus_->setStyleSheet("font-weight:bold; padding:4px;");
    root->addWidget(oriStatus_);

    auto *imagesRow = new QHBoxLayout();
    for (int pos = 0; pos < 2; ++pos) {
        auto *col = new QVBoxLayout();
        oriCaption_[pos] = new QLabel(tab);
        oriCaption_[pos]->setAlignment(Qt::AlignCenter);
        oriCaption_[pos]->setMinimumHeight(34);
        col->addWidget(oriCaption_[pos]);

        auto *sa = new QScrollArea(tab);
        sa->setWidgetResizable(false);
        sa->setAlignment(Qt::AlignCenter);  // image centred between its labels
        sa->setFrameShape(QFrame::NoFrame);
        oriScroll_[pos] = sa;
        oriLabel_[pos] = new QLabel(sa);
        oriLabel_[pos]->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        oriLabel_[pos]->setMouseTracking(true);
        oriLabel_[pos]->setCursor(Qt::CrossCursor);
        oriLabel_[pos]->installEventFilter(this);
        oriLabel_[pos]->setToolTip(
            tr("Left-click places a corner point.\n"
               "Double-RIGHT-click opens this image fullscreen (full resolution)."));
        sa->setWidget(oriLabel_[pos]);

        // Direction names around the image, in the place each plane occupies:
        // SOUTH above, NORTH below, WEST left, EAST right (that really is the
        // layout -- see kPlaneCompass). CENTER gets the free top-left cell since
        // it has no edge of its own. They sit OUTSIDE the picture, so they can
        // never cover a corner point or a detection marker.
        auto *dirGrid = new QGridLayout();
        dirGrid->setContentsMargins(0, 0, 0, 0);
        dirGrid->setSpacing(2);
        auto addDir = [&](const char *plane, int row, int cclm, Qt::Alignment align) {
            auto *l = new QLabel(QString::fromLatin1(plane).toUpper(), tab);
            l->setAlignment(align);
            if (const PlaneCompass *cp = compassFor(plane))
                l->setToolTip(tr("The \"%1\" plane is the panel at the %2 of this picture.")
                                  .arg(QString::fromLatin1(plane), QString::fromLatin1(cp->where)));
            oriDirLabels_[pos][plane] = l;
            dirGrid->addWidget(l, row, cclm, align);
        };
        addDir("center", 0, 0, Qt::AlignLeft | Qt::AlignVCenter);
        addDir("south", 0, 1, Qt::AlignCenter);
        addDir("west", 1, 0, Qt::AlignRight | Qt::AlignVCenter);
        addDir("east", 1, 2, Qt::AlignLeft | Qt::AlignVCenter);
        addDir("north", 2, 1, Qt::AlignCenter);
        dirGrid->addWidget(sa, 1, 1, Qt::AlignCenter);
        dirGrid->setColumnStretch(1, 1);
        dirGrid->setRowStretch(1, 1);
        col->addLayout(dirGrid, 1);

        oriCount_[pos] = new QLabel(tab);
        oriCount_[pos]->setWordWrap(true);
        oriCount_[pos]->setStyleSheet("color:#c0392b; font-size:12px; padding:2px;");
        col->addWidget(oriCount_[pos]);
        imagesRow->addLayout(col);
    }
    root->addLayout(imagesRow, 1);

    // Per-plane clear buttons: wipe one plane (both cameras) without losing the
    // others; the workflow then jumps back to that plane to re-click it.
    auto *delRow = new QHBoxLayout();
    delRow->addWidget(new QLabel(tr("Clear plane:"), tab));
    for (const auto &plane : kOriPlaneOrder) {
        auto *b = new QPushButton(QString::fromStdString(plane), tab);
        b->setMaximumWidth(90);
        if (const PlaneCompass *cp = compassFor(plane))
            b->setToolTip(tr("Clear the \"%1\" plane — the panel at the %2 of the picture.")
                              .arg(QString::fromStdString(plane), QString::fromLatin1(cp->where)));
        connect(b, &QPushButton::clicked, this, [this, plane] { oriDeletePlane(plane); });
        delRow->addWidget(b);
    }
    delRow->addStretch(1);
    root->addLayout(delRow);

    // Detector choice. Both work on the original pixels -- nothing is resampled.
    auto *methodRow = new QHBoxLayout();
    methodRow->addWidget(new QLabel(tr("Corner detector:"), tab));
    oriMethodCombo_ = new QComboBox(tab);
    oriMethodCombo_->addItem(tr("Auto (recommended)"),
                             static_cast<int>(PanelCorner::Method::Auto));
    oriMethodCombo_->addItem(tr("libcbdetect (Geiger 2012)"),
                             static_cast<int>(PanelCorner::Method::OriginalCb));
    oriMethodCombo_->addItem(tr("OpenCV findChessboardCornersSB"),
                             static_cast<int>(PanelCorner::Method::OriginalSb));
    oriMethodCombo_->setToolTip(
        tr("Both detectors read the original image directly -- the pixels are never\n"
           "resampled, so every corner is measured once, on the data.\n\n"
           "Auto: libcbdetect first, then OpenCV SB; the first one that finds a board wins.\n\n"
           "libcbdetect grows the board from local corner predictions instead of assuming\n"
           "straight rows, so it copes with fisheye distortion on its own and needs no\n"
           "camera parameters. OpenCV SB is faster but wants a nearly straight grid, so it\n"
           "tends to give up on the off-axis panels."));
    methodRow->addWidget(oriMethodCombo_, 1);

    // Index numbers beside each red X. On a dense board they cannot be drawn
    // legibly at panel size, so "Auto" leaves them out there and lets the
    // full-resolution fullscreen view carry them; the other two settings take
    // that decision away from the code.
    methodRow->addWidget(new QLabel(tr("Index numbers:"), tab));
    oriIndexModeCombo_ = new QComboBox(tab);
    oriIndexModeCombo_->addItem(tr("Auto (hide when too dense)"), IndexAuto);
    oriIndexModeCombo_->addItem(tr("Always show"), IndexAlways);
    oriIndexModeCombo_->addItem(tr("Never show"), IndexNever);
    oriIndexModeCombo_->setToolTip(
        tr("Whether the point index is written beside each detected corner.\n\n"
           "Auto: drawn whenever the gap between corners leaves room to read it. A 23x23\n"
           "board on the 520 px panel puts its corners ~6 px apart, which is why the\n"
           "numbers vanish there while the same board shows them in the fullscreen view\n"
           "(double-right-click), where the gap is ~6x wider.\n\n"
           "Always: drawn at panel size too, overlapping if the board is dense.\n"
           "Never: only the X markers, for the cleanest view of the corners themselves."));
    connect(oriIndexModeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) {
                oriIndexMode_ = oriIndexModeCombo_->currentData().toInt();
                oriShowPanels();
            });
    methodRow->addWidget(oriIndexModeCombo_);
    root->addLayout(methodRow);

    // Overlay switches. On a dense board the marks are what stop you seeing the
    // squares underneath, so each layer can be taken off independently: the
    // detected corners (red X, with their numbers) and the reprojected pixels
    // (blue +) that Start Calculation puts back on the image.
    auto *overlayRow = new QHBoxLayout();
    overlayRow->addWidget(new QLabel(tr("Overlay:"), tab));
    oriShowCornersBox_ = new QCheckBox(tr("detected corners (X)"), tab);
    oriShowCornersBox_->setChecked(oriShowCorners_);
    oriShowCornersBox_->setToolTip(
        tr("The red X on every corner the detector recovered, with its index. Turn it off to "
           "look at the board itself -- the corners are still detected, just not drawn."));
    oriShowReprojBox_ = new QCheckBox(tr("reprojection (+)"), tab);
    oriShowReprojBox_->setChecked(oriShowReproj_);
    oriShowReprojBox_->setToolTip(
        tr("The blue + where each triangulated 3D point projects back into this image. The "
           "distance from its X is that point's reprojection error."));
    connect(oriShowCornersBox_, &QCheckBox::toggled, this, [this](bool on) {
        oriShowCorners_ = on;
        oriShowPanels();
    });
    connect(oriShowReprojBox_, &QCheckBox::toggled, this, [this](bool on) {
        oriShowReproj_ = on;
        oriShowPanels();
    });
    oriShowOutlineBox_ = new QCheckBox(tr("plane outline (green)"), tab);
    oriShowOutlineBox_->setChecked(oriShowOutline_);
    oriShowOutlineBox_->setToolTip(
        tr("The plane you marked out: the 4 clicked corners and the outline joining them "
           "(curved once the fisheye boundary is known, amber on the plane being clicked). "
           "Hiding it does not clear the clicks -- they are still there, just not drawn."));
    connect(oriShowOutlineBox_, &QCheckBox::toggled, this, [this](bool on) {
        oriShowOutline_ = on;
        oriShowPanels();
    });
    overlayRow->addWidget(oriShowCornersBox_);
    overlayRow->addWidget(oriShowReprojBox_);
    overlayRow->addWidget(oriShowOutlineBox_);
    overlayRow->addStretch(1);
    root->addLayout(overlayRow);

    auto *ctl = new QHBoxLayout();
    ctl->addWidget(new QLabel(tr("Size hint (auto-detects per plane) rows x cols:"), tab));
    oriRows_ = new QSpinBox(tab);
    oriRows_->setRange(2, 30);
    oriRows_->setValue(6);
    oriCols_ = new QSpinBox(tab);
    oriCols_->setRange(2, 30);
    oriCols_->setValue(9);
    ctl->addWidget(oriRows_);
    ctl->addWidget(new QLabel("x", tab));
    ctl->addWidget(oriCols_);
    ctl->addStretch(1);
    auto *btnReset = new QPushButton(tr("Reset points"), tab);
    auto *btnStart = new QPushButton(tr("Start Calculation"), tab);
    ctl->addWidget(btnReset);
    ctl->addWidget(btnStart);
    root->addLayout(ctl);

    // Whole-image survey: no clicking, every board, every method, side by side.
    auto *surveyRow = new QHBoxLayout();
    oriSurveyBtn_ = new QPushButton(tr("Detect All (no clicks)"), tab);
    oriSurveyBtn_->setToolTip(
        tr("Find every chessboard in BOTH images without clicking anything, then re-detect\n"
           "each one with every method and draw them on top of each other in different\n"
           "colours.\n\n"
           "The first layer is the raw corner candidates from before the board structure is\n"
           "built: a corner that is in that layer but in none of the others was found and\n"
           "then dropped by the board growth, while one that is in no layer at all was never\n"
           "measurable there. That distinction is what tells you why corners near a panel\n"
           "edge go missing."));
    surveyRow->addWidget(oriSurveyBtn_);
    surveyRow->addWidget(new QLabel(tr("Show:"), tab));
    oriSurveyLayerCombo_ = new QComboBox(tab);
    oriSurveyLayerCombo_->addItem(tr("All layers"), -1);
    oriSurveyLayerCombo_->setEnabled(false);
    surveyRow->addWidget(oriSurveyLayerCombo_, 1);
    root->addLayout(surveyRow);

    oriSurveyLegend_ = new QLabel(tab);
    oriSurveyLegend_->setWordWrap(true);
    oriSurveyLegend_->setTextFormat(Qt::RichText);
    oriSurveyLegend_->setStyleSheet("font-size:12px; padding:2px;");
    oriSurveyLegend_->hide();
    root->addWidget(oriSurveyLegend_);

    connect(btnReset, &QPushButton::clicked, this, [this] { oriResetClicks(); });
    connect(btnStart, &QPushButton::clicked, this, [this] { oriStartCalculation(); });
    connect(oriSurveyBtn_, &QPushButton::clicked, this, [this] { oriSurveyRun(); });
    connect(oriSurveyLayerCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) {
                oriSurveyShow_ = oriSurveyLayerCombo_->currentData().toInt();
                oriShowPanels();
            });

    // Changing the grid size re-recovers every already-clicked plane so the red X
    // overlay + corner counts always match the current rows x cols.
    auto reRecover = [this] {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        for (const auto &plane : kOriPlaneOrder)
            for (int c = 0; c < 2; ++c) oriRecoverPlane(plane, c);
        QApplication::restoreOverrideCursor();
        oriShowPanels();
        oriUpdateCounts();
    };
    connect(oriRows_, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [reRecover](int) { reRecover(); });
    connect(oriCols_, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [reRecover](int) { reRecover(); });
    // Same for the detector: switching method re-runs every clicked plane, so
    // the effect of the choice is visible immediately on the red X overlay.
    connect(oriMethodCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this, reRecover](int) {
                oriMethod_ = static_cast<PanelCorner::Method>(
                    oriMethodCombo_->currentData().toInt());
                reRecover();
            });

    oriResetClicks();
}

void Controller3dMeasurement::oriResetClicks() {
    oriClicks_.clear();
    oriRecovered_.clear();
    oriBoundary_.clear();
    oriReproj_[LEFT].clear();
    oriReproj_[RIGHT].clear();
    oriSurvey_[LEFT].clear();  // the survey overlay belongs to the old images too
    oriSurvey_[RIGHT].clear();
    oriSurveyShow_ = -1;
    if (oriSurveyLayerCombo_) {
        QSignalBlocker block(oriSurveyLayerCombo_);
        oriSurveyLayerCombo_->clear();
        oriSurveyLayerCombo_->addItem(tr("All layers"), -1);
        oriSurveyLayerCombo_->setEnabled(false);
    }
    if (oriSurveyLegend_) oriSurveyLegend_->hide();
    for (const auto &p : kOriPlaneOrder) oriClicks_[p];  // create empty slots
    oriAdvanceStep();  // -> first plane / LEFT
    oriShowPanels();
    oriUpdateCounts();
    oriUpdatePrompt(false);  // status text only (no popup on construction/reset)
}

void Controller3dMeasurement::oriAdvanceStep() {
    // Target the first (plane, camera) that still needs its 4 clicks, scanning in
    // order. This makes out-of-order editing (after a per-plane clear) just work.
    for (int p = 0; p < static_cast<int>(kOriPlaneOrder.size()); ++p)
        for (int c = 0; c < 2; ++c)
            if (oriClicks_[kOriPlaneOrder[p]][c].size() < 4) {
                oriStepPlane_ = p;
                oriStepCam_ = c;
                return;
            }
    oriStepPlane_ = static_cast<int>(kOriPlaneOrder.size());  // all captured
    oriStepCam_ = LEFT;
}

void Controller3dMeasurement::oriDeletePlane(const std::string &plane) {
    oriClicks_[plane][LEFT].clear();
    oriClicks_[plane][RIGHT].clear();
    oriRecovered_.erase(plane);
    oriBoundary_.erase(plane);
    oriReproj_[LEFT].clear();  // any reprojection overlay is now stale
    oriReproj_[RIGHT].clear();
    oriAdvanceStep();  // jump the target back to this (now-empty) plane
    oriShowPanels();
    oriUpdateCounts();
    oriUpdatePrompt(false);
}

namespace {
// One colour per survey layer, in the order surveyImage() emits them. The
// marker also shrinks down the list so overlapping hits from several methods
// stay tellable apart instead of painting over each other.
struct LayerStyle {
    cv::Scalar bgr;
    const char *html;
    int size;
};
const std::array<LayerStyle, 4> kSurveyStyle{{
    {cv::Scalar(190, 190, 190), "#bebebe", 5},  // corner candidates
    {cv::Scalar(0, 220, 0), "#00dc00", 15},     // libcbdetect boards, whole image
    {cv::Scalar(0, 0, 255), "#ff0000", 11},     // per-panel libcbdetect
    {cv::Scalar(255, 0, 255), "#ff00ff", 8},    // per-panel OpenCV SB
}};
const LayerStyle &styleFor(size_t i) { return kSurveyStyle[i % kSurveyStyle.size()]; }

// Outer ring of the detected corners, in order -> a boundary polyline that
// follows the board edge (curved on a fisheye) without any remap processing.
std::vector<cv::Point2f> perimeterFromCorners(const PanelCorner::PanelDetection &det) {
    std::map<std::pair<int, int>, cv::Point2f> m;
    for (const auto &c : det.corners) m[{c.row, c.col}] = c.pt;
    std::vector<cv::Point2f> poly;
    auto add = [&](int r, int c) {
        auto it = m.find({r, c});
        if (it != m.end()) poly.push_back(it->second);
    };
    for (int c = 0; c < det.cols; ++c) add(0, c);                    // top row
    for (int r = 1; r < det.rows; ++r) add(r, det.cols - 1);        // right col
    for (int c = det.cols - 2; c >= 0; --c) add(det.rows - 1, c);   // bottom row
    for (int r = det.rows - 2; r >= 1; --r) add(r, 0);              // left col
    if (!poly.empty()) poly.push_back(poly.front());               // close the ring
    return poly;
}
}  // namespace

void Controller3dMeasurement::oriRecoverPlane(const std::string &plane, int cam) {
    oriRecovered_[plane][cam] = PanelCorner::PanelDetection{};
    oriBoundary_[plane][cam].clear();
    const std::vector<cv::Point2f> &clicks = oriClicks_[plane][cam];
    if (clicks.size() != 4 || imageSource_[cam].empty()) return;
    // Spin boxes are only an optional hint; the size is auto-detected per plane.
    const int hintRows = oriRows_ ? oriRows_->value() : 0;
    const int hintCols = oriCols_ ? oriCols_->value() : 0;
    const std::array<cv::Point2f, 4> outer{clicks[0], clicks[1], clicks[2], clicks[3]};
    // Which detector runs is the user's choice in the ORI_DET tab; Auto runs the
    // measured cascade. See PanelCornerRecovery.h for what each one does and
    // when it is the right one.
    oriRecovered_[plane][cam] =
        PanelCorner::recoverPanel(imageSource_[cam], outer, hintRows, hintCols, oriMethod_);
    oriBoundary_[plane][cam] = perimeterFromCorners(oriRecovered_[plane][cam]);
}

void Controller3dMeasurement::oriSurveyRefreshLegend() {
    if (!oriSurveyLegend_) return;
    const size_t n = std::max(oriSurvey_[LEFT].size(), oriSurvey_[RIGHT].size());
    if (n == 0) {
        oriSurveyLegend_->hide();
        return;
    }
    QStringList rows;
    for (size_t i = 0; i < n; ++i) {
        const PanelCorner::SurveyLayer *l =
            i < oriSurvey_[LEFT].size() ? &oriSurvey_[LEFT][i] : nullptr;
        const PanelCorner::SurveyLayer *r =
            i < oriSurvey_[RIGHT].size() ? &oriSurvey_[RIGHT][i] : nullptr;
        const PanelCorner::SurveyLayer *any = l ? l : r;
        if (!any) continue;
        auto cell = [](const PanelCorner::SurveyLayer *s) {
            if (!s) return QString("-");
            return s->structured
                       ? QString("%1 pts / %2 boards / %3 ms")
                             .arg(s->points.size())
                             .arg(s->boards)
                             .arg(static_cast<int>(s->ms))
                       : QString("%1 pts / %2 ms").arg(s->points.size()).arg(static_cast<int>(s->ms));
        };
        rows << QString("<span style='color:%1'>&#9632;</span> <b>%2</b> &nbsp; L: %3 &nbsp;|&nbsp; "
                        "R: %4")
                    .arg(styleFor(i).html, QString::fromStdString(any->name), cell(l), cell(r));
    }
    oriSurveyLegend_->setText(rows.join("<br>"));
    oriSurveyLegend_->show();
}

void Controller3dMeasurement::oriSurveyRun() {
    if (oriSurveying_) return;
    if (imageSource_[LEFT].empty() && imageSource_[RIGHT].empty()) {
        QMessageBox::information(this, tr("Detect All"),
                                 tr("Load the images first (Open Media on the main tab)."));
        return;
    }
    oriSurveying_ = true;
    oriSurveyBtn_->setEnabled(false);
    oriSurveyBtn_->setText(tr("Detecting ..."));
    QApplication::setOverrideCursor(Qt::WaitCursor);

    // Copy what the worker needs (cv::Mat is refcounted, so this is cheap).
    struct Job {
        cv::Mat img[2];
    } job;
    for (int p = 0; p < 2; ++p) job.img[p] = imageSource_[p];

    using Result = std::array<std::vector<PanelCorner::SurveyLayer>, 2>;
    auto *watcher = new QFutureWatcher<Result>(this);
    connect(watcher, &QFutureWatcher<Result>::finished, this, [this, watcher] {
        oriSurvey_ = watcher->result();
        watcher->deleteLater();
        oriSurveying_ = false;
        QApplication::restoreOverrideCursor();
        oriSurveyBtn_->setEnabled(true);
        oriSurveyBtn_->setText(tr("Detect All (no clicks)"));

        // Rebuild the layer filter from whichever side produced more layers.
        const auto &ref = oriSurvey_[LEFT].size() >= oriSurvey_[RIGHT].size() ? oriSurvey_[LEFT]
                                                                             : oriSurvey_[RIGHT];
        QSignalBlocker block(oriSurveyLayerCombo_);
        oriSurveyLayerCombo_->clear();
        oriSurveyLayerCombo_->addItem(tr("All layers"), -1);
        for (size_t i = 0; i < ref.size(); ++i)
            oriSurveyLayerCombo_->addItem(QString::fromStdString(ref[i].name),
                                          static_cast<int>(i));
        oriSurveyLayerCombo_->setEnabled(!ref.empty());
        oriSurveyShow_ = -1;

        oriSurveyRefreshLegend();
        oriShowPanels();
    });

    watcher->setFuture(QtConcurrent::run([job]() -> Result {
        Result r;
        for (int p = 0; p < 2; ++p)
            if (!job.img[p].empty()) r[p] = PanelCorner::surveyImage(job.img[p]);
        return r;
    }));
}

void Controller3dMeasurement::oriUpdateCounts() {
    for (int pos = 0; pos < 2; ++pos) {
        if (!oriCount_[pos]) continue;
        QStringList lines;
        for (const auto &plane : kOriPlaneOrder) {
            auto it = oriRecovered_.find(plane);
            if (it == oriRecovered_.end()) continue;
            const PanelCorner::PanelDetection &det = it->second[pos];
            if (det.corners.empty()) continue;
            lines << QString("%1: %2x%3 — %4 corners")
                         .arg(QString::fromStdString(plane))
                         .arg(det.cols)
                         .arg(det.rows)
                         .arg(static_cast<int>(det.corners.size()));
        }
        oriCount_[pos]->setText(lines.isEmpty() ? tr("(no corners detected yet)")
                                                : lines.join("\n"));
    }
}

QPoint Controller3dMeasurement::oriLabelToImage(int pos, QPoint labelPos) const {
    const double s = oriScale_[pos] > 0 ? oriScale_[pos] : 1.0;
    return QPoint(static_cast<int>(std::lround(labelPos.x() * s)),
                  static_cast<int>(std::lround(labelPos.y() * s)));
}

// Draw one ORI_DET panel at an arbitrary resolution. `scale` is original px per
// rendered px: the panel itself uses origW/520, while the fullscreen viewer asks
// for 1.0 so the corners can actually be inspected. Marker sizes are divided by
// the same factor, so the overlay keeps the size it has on the panel no matter
// which resolution it is drawn at.
cv::Mat Controller3dMeasurement::oriRenderPanel(int pos, double scale) {
    const cv::Mat &src = imageSource_[pos];
    if (src.empty() || scale <= 0) return {};
    const std::string curPlane =
        oriStepPlane_ < static_cast<int>(kOriPlaneOrder.size()) ? kOriPlaneOrder[oriStepPlane_]
                                                                : std::string();
    const int w = std::max(1, static_cast<int>(std::lround(src.cols / scale)));
    const int h = std::max(1, static_cast<int>(std::lround(src.rows / scale)));
    cv::Mat disp;
    if (w == src.cols && h == src.rows)
        disp = src.clone();
    else
        cv::resize(src, disp, cv::Size(w, h), 0, 0, cv::INTER_AREA);

    // How much bigger this render is than the 520 px panel -> marker scale-up.
    const double k = std::max(1.0, (static_cast<double>(src.cols) / 520.0) / scale);
    auto sz = [k](double v) { return std::max(1, static_cast<int>(std::lround(v * k))); };
    auto at = [scale](const cv::Point2f &p) {
        return cv::Point(static_cast<int>(p.x / scale), static_cast<int>(p.y / scale));
    };

    for (const auto &kv : oriClicks_) {
        const std::string &plane = kv.first;
        const std::vector<cv::Point2f> &pts = kv.second[pos];
        const bool cur = (plane == curPlane);
        const cv::Scalar quadCol = cur ? cv::Scalar(0, 200, 255) : cv::Scalar(0, 180, 0);
        // The 4 clicked outer corners: small circles. They go with the outline --
        // both are the plane you marked out, as opposed to what was detected in it.
        if (oriShowOutline_)
            for (size_t i = 0; i < pts.size(); ++i)
                cv::circle(disp, at(pts[i]), sz(4), quadCol, sz(1));
        // Plane outline: the curved fisheye boundary if we have it, else a
        // straight quad from the 4 clicks (before the stereo recovery runs).
        auto bit = oriBoundary_.find(plane);
        if (!oriShowOutline_) {
            // hidden on request
        } else if (bit != oriBoundary_.end() && bit->second[pos].size() > 1) {
            const auto &poly = bit->second[pos];
            for (size_t i = 0; i + 1 < poly.size(); ++i)
                cv::line(disp, at(poly[i]), at(poly[i + 1]), quadCol, sz(1), cv::LINE_AA);
        } else if (pts.size() == 4) {
            for (int i = 0; i < 4; ++i)
                cv::line(disp, at(pts[i]), at(pts[(i + 1) % 4]), quadCol, sz(1));
        }
        // Recovered inner corners: red X plus its grid index. The index is
        // row * cols + col within this plane -- the same number Start
        // Calculation gives the point, and the same on both cameras, so a
        // left/right pair can be checked by eye and a missing corner shows up
        // as a gap in the sequence.
        //
        // Everything here is sized from the SPACING BETWEEN CORNERS in this
        // render, not from a constant: a dense board on a small panel would
        // otherwise be buried under its own markers. The marks deliberately take
        // only a small share of that gap -- most of it stays empty board, which
        // is what makes a crowded panel readable -- because anything too small to
        // study here can be read in the full-resolution fullscreen view
        // (double-right-click), where the gap is ~6x wider.
        auto rit = oriRecovered_.find(plane);
        if (oriShowCorners_ && rit != oriRecovered_.end()) {
            const PanelCorner::PanelDetection &det = rit->second[pos];
            const double gap = cornerSpacing(det) / scale;  // rendered px between corners
            const double sp = gap > 1.0 ? gap : disp.cols / 40.0;  // fallback: image size
            const int mkSize = std::max(3, std::min(40, static_cast<int>(std::lround(sp * 0.26))));
            const int mkThick = sp > 40 ? 2 : 1;
            // FONT_HERSHEY_SIMPLEX caps are ~22 px at scale 1: keep the number to
            // about a quarter of the gap. Under ~7 px tall it is no longer
            // readable, which is what "Auto" drops -- "Always" draws it anyway,
            // at the smallest size OpenCV still renders.
            const double textH = sp * 0.24;
            const double fscale = std::max(0.24, std::min(1.4, textH / 22.0));
            const bool numbersFit = (oriIndexMode_ == IndexAlways) ||
                                    (oriIndexMode_ == IndexAuto && textH >= 7.0);
            for (const auto &cn : det.corners) {
                const cv::Point d = at(cn.pt);
                cv::drawMarker(disp, d, cv::Scalar(0, 0, 255), cv::MARKER_TILTED_CROSS, mkSize,
                               mkThick);
                if (!numbersFit) continue;
                const std::string t =
                    std::to_string(det.cols > 0 ? cn.row * det.cols + cn.col : cn.col);
                const cv::Point tp(d.x + mkSize / 2 + 2, d.y - mkSize / 2 - 2);
                // White halo first so the number survives on a dark board.
                cv::putText(disp, t, tp, cv::FONT_HERSHEY_SIMPLEX, fscale,
                            cv::Scalar(255, 255, 255), mkThick + 2, cv::LINE_AA);
                cv::putText(disp, t, tp, cv::FONT_HERSHEY_SIMPLEX, fscale, cv::Scalar(0, 0, 255),
                            mkThick, cv::LINE_AA);
            }
        }
    }
    // Whole-image survey overlay: one colour per method, drawn on top of
    // everything else. Layer 0 (raw candidates) uses dots, the structured
    // layers use crosses, so "found but not linked into a board" reads at a
    // glance.
    for (size_t li = 0; li < oriSurvey_[pos].size(); ++li) {
        if (oriSurveyShow_ >= 0 && static_cast<int>(li) != oriSurveyShow_) continue;
        const LayerStyle &st = styleFor(li);
        const bool structured = oriSurvey_[pos][li].structured;
        for (const auto &p : oriSurvey_[pos][li].points) {
            if (structured)
                cv::drawMarker(disp, at(p), st.bgr, cv::MARKER_TILTED_CROSS, sz(st.size), sz(1),
                               cv::LINE_AA);
            else
                cv::circle(disp, at(p), sz(2), st.bgr, -1, cv::LINE_AA);
        }
    }

    // Reprojected points (after Start Calculation): blue +. Sized off the mean
    // corner gap of this panel, same reasoning as the red X above -- the + lands
    // right next to the X it should be compared with, so both have to stay well
    // inside one corner's worth of room.
    if (oriShowReproj_ && !oriReproj_[pos].empty()) {
        double gapSum = 0;
        int gapN = 0;
        for (const auto &kv : oriRecovered_) {
            const double g = cornerSpacing(kv.second[pos]);
            if (g > 1.0) {
                gapSum += g;
                ++gapN;
            }
        }
        const double panelGap = gapN ? (gapSum / gapN) / scale : 0.0;
        const double sp = panelGap > 1.0 ? panelGap : disp.cols / 40.0;
        const int rSize = std::max(3, std::min(40, static_cast<int>(std::lround(sp * 0.26))));
        for (const auto &rr : oriReproj_[pos])
            if (!std::isnan(rr.u) && !std::isnan(rr.v))
                cv::drawMarker(
                    disp, at(cv::Point2f(static_cast<float>(rr.u), static_cast<float>(rr.v))),
                    cv::Scalar(255, 0, 0), cv::MARKER_CROSS, rSize, sp > 40 ? 2 : 1);
    }

    // Nothing else is drawn over the picture: the direction compass is a widget
    // around it (see oriHighlightDirection), so no label can sit on a corner.
    return disp;
}

// The five planes always sit in the same places on a fisheye, so the direction
// names live permanently around each image. Only the highlight moves: the plane
// being asked for turns amber on both panels, the rest stay grey.
void Controller3dMeasurement::oriHighlightDirection(const std::string &curPlane) {
    for (int pos = 0; pos < 2; ++pos)
        for (const auto &kv : oriDirLabels_[pos]) {
            if (!kv.second) continue;
            const bool cur = (kv.first == curPlane);
            kv.second->setStyleSheet(
                cur ? "color:#000; background:#ffcc00; font-size:12px; font-weight:bold; "
                      "border-radius:3px; padding:2px 6px;"
                    : "color:#999; font-size:12px; font-weight:bold; padding:2px 6px;");
        }
}

void Controller3dMeasurement::oriShowPanels() {
    if (!oriLabel_[LEFT]) return;
    for (int pos = 0; pos < 2; ++pos) {
        QLabel *lab = oriLabel_[pos];
        if (!lab) continue;
        if (imageSource_[pos].empty()) {
            lab->setPixmap(QPixmap());
            lab->setText(tr("Load the %1 image first (Open Media on the main tab).")
                             .arg(pos == LEFT ? tr("LEFT") : tr("RIGHT")));
            lab->adjustSize();
            continue;
        }
        oriScale_[pos] = static_cast<double>(imageSource_[pos].cols) / 520;  // orig / displayed
        const cv::Mat disp = oriRenderPanel(pos, oriScale_[pos]);
        if (disp.empty()) continue;
        const QPixmap pix = matToPixmap(disp);
        lab->setPixmap(pix);
        lab->resize(pix.size());
        // Wrap the viewport exactly around the picture so the four direction
        // labels sit against the image edges instead of floating away from it.
        if (oriScroll_[pos]) oriScroll_[pos]->setFixedSize(pix.width(), pix.height());
    }
    for (QLabel *l : findChildren<QLabel *>()) l->update();
}

void Controller3dMeasurement::oriHandleClick(int pos, QPoint labelPos) {
    if (oriStepPlane_ >= static_cast<int>(kOriPlaneOrder.size())) return;  // all captured
    if (pos != oriStepCam_) return;  // must click the panel the prompt asks for
    if (imageSource_[pos].empty()) return;
    const std::string plane = kOriPlaneOrder[oriStepPlane_];
    std::vector<cv::Point2f> &pts = oriClicks_[plane][pos];
    if (pts.size() >= 4) return;

    const QPoint ip = oriLabelToImage(pos, labelPos);
    if (ip.x() < 0 || ip.y() < 0 || ip.x() >= imageSource_[pos].cols ||
        ip.y() >= imageSource_[pos].rows)
        return;  // outside the image
    pts.emplace_back(static_cast<float>(ip.x()), static_cast<float>(ip.y()));
    oriReproj_[LEFT].clear();  // any prior reprojection overlay is now stale
    oriReproj_[RIGHT].clear();

    const bool setComplete = (pts.size() == 4);
    if (setComplete) {
        // The recovery rectifies the panel and sweeps board sizes; it is bounded
        // but can take a moment on a panel it struggles with, so say so.
        QApplication::setOverrideCursor(Qt::WaitCursor);
        oriRecoverPlane(plane, pos);  // detect this image now -> red X immediately
        QApplication::restoreOverrideCursor();
        oriAdvanceStep();             // move to the next image still needing clicks
    }
    oriShowPanels();
    oriUpdateCounts();
    oriUpdatePrompt(setComplete);  // popup announces the next set
    scheduleAutosave();            // clicked corners are worth keeping too
}

void Controller3dMeasurement::oriUpdatePrompt(bool popup) {
    if (!oriStatus_) return;

    // Refresh the big LEFT / RIGHT headers: count completed planes per side and
    // highlight the panel the user should be clicking right now.
    const int activeCam =
        (oriStepPlane_ < static_cast<int>(kOriPlaneOrder.size())) ? oriStepCam_ : -1;
    for (int pos = 0; pos < 2; ++pos) {
        if (!oriCaption_[pos]) continue;
        int done = 0;
        for (const auto &p : kOriPlaneOrder)
            if (oriClicks_[p][pos].size() == 4) ++done;
        const QString side = (pos == LEFT) ? tr("LEFT IMAGE") : tr("RIGHT IMAGE");
        const bool active = (pos == activeCam);
        oriCaption_[pos]->setText(
            QString("%1%2  —  %3/%4 planes")
                .arg(active ? "▶ " : "", side)
                .arg(done)
                .arg(static_cast<int>(kOriPlaneOrder.size())));
        oriCaption_[pos]->setStyleSheet(
            active ? "font-size:16px; font-weight:bold; color:white; background:#c0392b; "
                     "border-radius:4px; padding:4px;"
                   : "font-size:16px; font-weight:bold; color:#888; background:#eee; "
                     "border-radius:4px; padding:4px;");
    }

    if (oriStepPlane_ >= static_cast<int>(kOriPlaneOrder.size())) {
        const QString msg = tr("All 5 planes captured on both cameras. Press \"Start Calculation\".");
        oriStatus_->setText(msg);
        oriHighlightDirection(std::string());  // nothing pending: no plane highlighted
        if (popup) QMessageBox::information(this, tr("ORI_DET"), msg);
        return;
    }
    const std::string plane = kOriPlaneOrder[oriStepPlane_];
    oriHighlightDirection(plane);
    const int cam = oriStepCam_;
    const std::vector<cv::Point2f> &pts = oriClicks_[plane][cam];
    const int nextIdx = std::min<int>(static_cast<int>(pts.size()), 3);
    const QString side = (cam == LEFT) ? tr("LEFT") : tr("RIGHT");
    // Say WHERE the plane is, not just its name: the panel positions are fixed
    // by the optics, and the compass drawn on the image marks the same spot.
    const PlaneCompass *cp = compassFor(plane);
    const QString where = cp ? tr(" (%1 of the picture)").arg(QString::fromLatin1(cp->where))
                             : QString();
    oriStatus_->setText(
        tr("Plane \"%1\"%2 — click 4 corners on %3 (top-left, top-right, bottom-right, "
           "bottom-left). Next: %4 (%5/4)")
            .arg(QString::fromStdString(plane), where, side,
                 QString::fromLatin1(kOriCornerLabels[nextIdx]))
            .arg(static_cast<int>(pts.size()) + 1));
    if (popup && pts.empty())
        QMessageBox::information(
            this, tr("ORI_DET"),
            tr("Click 4 points on the %1 image for plane \"%2\"%3.\n"
               "The compass beside the prompt highlights which one that is.\n"
               "Order: top-left, top-right, bottom-right, bottom-left.")
                .arg(side, QString::fromStdString(plane), where));
}

void Controller3dMeasurement::oriStartCalculation() {
    using Measure3d::DetPoint;
    if (!moil_[LEFT].valid() || !moil_[RIGHT].valid() || imageSource_[LEFT].empty() ||
        imageSource_[RIGHT].empty()) {
        QMessageBox::warning(this, tr("ORI_DET"),
                             tr("Load the left + right images and their parameter JSON first."));
        return;
    }
    // For every fully-captured plane: recover the inner-corner grid on each
    // original image, pair L<->R by (row,col) topology, convert to alpha/beta.
    std::vector<DetPoint> dfL, dfR;
    std::map<std::string, std::pair<int, int>> patternForShow;
    QStringList skipped;
    int planesUsed = 0;
    for (const auto &plane : kOriPlaneOrder) {
        const auto &cl = oriClicks_[plane][LEFT];
        const auto &cr = oriClicks_[plane][RIGHT];
        if (cl.size() != 4 || cr.size() != 4) {
            skipped << QString::fromStdString(plane);
            continue;
        }
        // Reuse the grid detected when each image was clicked (the red X overlay).
        oriRecoverPlane(plane, LEFT);
        oriRecoverPlane(plane, RIGHT);
        const auto &cornersL = oriRecovered_[plane][LEFT].corners;
        const auto &cornersR = oriRecovered_[plane][RIGHT].corners;

        std::map<std::pair<int, int>, cv::Point2f> byRcRight;
        for (const auto &c : cornersR) byRcRight[{c.row, c.col}] = c.pt;

        // Point id = the grid index inside this plane, exactly the number drawn
        // beside each red X on the panels (see oriRenderPanel). It is unique per
        // plane, identical on both cameras, and survives a corner going missing,
        // so what the 3D view calls "Point 17 · center" is the 17 on the image.
        const int gridCols = oriRecovered_[plane][LEFT].cols;

        int paired = 0;
        for (const auto &c : cornersL) {
            auto it = byRcRight.find({c.row, c.col});
            if (it == byRcRight.end()) continue;
            bool okL = false, okR = false;
            const auto abL = moil_[LEFT].getAlphaBeta(c.pt.x, c.pt.y, 1, okL);
            const auto abR = moil_[RIGHT].getAlphaBeta(it->second.x, it->second.y, 1, okR);
            if (!okL || !okR) continue;
            const int pid = gridCols > 0 ? c.row * gridCols + c.col : c.col;
            DetPoint dl;
            dl.direction = plane;
            dl.pointId = pid;
            dl.xFish = static_cast<int>(std::lround(c.pt.x));
            dl.yFish = static_cast<int>(std::lround(c.pt.y));
            dl.alpha = abL.first;
            dl.beta = abL.second;
            DetPoint dr;
            dr.direction = plane;
            dr.pointId = pid;
            dr.xFish = static_cast<int>(std::lround(it->second.x));
            dr.yFish = static_cast<int>(std::lround(it->second.y));
            dr.alpha = abR.first;
            dr.beta = abR.second;
            dfL.push_back(dl);
            dfR.push_back(dr);
            ++paired;
        }
        if (paired > 0) {
            ++planesUsed;
            const auto &det = oriRecovered_[plane][LEFT];
            // (cols, rows): the grid-edge CSV splits point_id with .first as the
            // column count, which is how the ids above were built.
            patternForShow[plane] = {det.cols, det.rows};
        } else {
            skipped << QString::fromStdString(plane);
        }
    }
    if (dfL.empty()) {
        QMessageBox::warning(
            this, tr("ORI_DET"),
            tr("No planes could be recovered. Capture 4 corners per plane on both cameras "
               "and set the correct grid size (rows x cols)."));
        return;
    }

    // Camera positions come from the same spin boxes the main flow uses.
    auto posVal = [this](const char *axis, int pos) -> double {
        auto *sb = findChild<QSpinBox *>(
            QString("spinBox_%1_%2").arg(axis).arg(QString::fromStdString(kCameras[pos])));
        return sb ? sb->value() : 0.0;
    };
    const Eigen::Vector3d camL(posVal("x", LEFT), posVal("y", LEFT), posVal("z", LEFT));
    const Eigen::Vector3d camR(posVal("x", RIGHT), posVal("y", RIGHT), posVal("z", RIGHT));

    oriPoints3d_ = AnypointChessboard::compute3dPoints(dfL, dfR, camL, camR, "ori_det", nullptr);
    if (oriPoints3d_.empty()) {
        QMessageBox::warning(this, tr("ORI_DET"), tr("Triangulation produced no points."));
        return;
    }
    oriCamL_ = camL;
    oriCamR_ = camR;

    std::vector<Measure3d::Point3d> copy = oriPoints3d_;
    PlaneFit3dViz::VizResult vr =
        PlaneFit3dViz::show3dPoint2camOriVisualization(copy, camL, camR, &patternForShow, "ori_det");

    PlaneFit3dViz::ReprojCompare rc = PlaneFit3dViz::compareReprojectionWithOriginal(
        oriPoints3d_, dfL, dfR, camL, camR, &moil_[LEFT], &moil_[RIGHT], nullptr);
    auto meanRms = [](const std::vector<Measure3d::ReprojRow> &rws, double &mean, double &rms) {
        double s = 0, sq = 0;
        int n = 0;
        for (const auto &x : rws)
            if (!std::isnan(x.error)) {
                s += x.error;
                sq += x.error * x.error;
                ++n;
            }
        mean = n ? s / n : Measure3d::nan();
        rms = n ? std::sqrt(sq / n) : Measure3d::nan();
    };
    double mL, rL, mR, rR;
    meanRms(rc.left, mL, rL);
    meanRms(rc.right, mR, rR);
    // Keep every number this tab shows, so a restored session displays them
    // without re-recovering the corner grids (which costs seconds per plane).
    oriAngle_ = vr.angleMap;
    oriInterRay_ = vr.meanMaps;
    oriThickness_ = vr.meanPlaneDist;
    oriDepth_ = vr.depthOriginPerDir;
    oriMeanErrL_ = mL;
    oriRmsL_ = rL;
    oriMeanErrR_ = mR;
    oriRmsR_ = rR;

    // Draw the reprojected pixels (blue +) back onto the ORI_DET images.
    oriReproj_[LEFT] = rc.left;
    oriReproj_[RIGHT] = rc.right;
    oriShowPanels();

    ensureOriResultTab();
    if (oriGlView_) {
        oriGlView_->setCameraFov(moil_[LEFT].cameraFov(), moil_[RIGHT].cameraFov());
        oriGlView_->setPoints(oriPoints3d_, camL, camR);
    }
    oriShowResults(vr.angleMap, vr.meanMaps, vr.meanPlaneDist, vr.depthOriginPerDir, mL, rL, mR, rR);
    if (auto *tw = findChild<QTabWidget *>("tabWidget"))
        if (ui_->tab_3) tw->setCurrentWidget(ui_->tab_3);

    scheduleAutosave();  // a finished ORI_DET calculation is worth remembering
    if (!skipped.isEmpty())
        QMessageBox::information(this, tr("ORI_DET"),
                                 tr("Calculated %1 plane(s). Skipped (incomplete/no match): %2.")
                                     .arg(planesUsed)
                                     .arg(skipped.join(", ")));
}

void Controller3dMeasurement::ensureOriResultTab() {
    if (oriGlView_) return;  // built already
    QWidget *tab = ui_->tab_3;
    if (!tab) return;
    auto *row = new QHBoxLayout(tab);
    oriGlView_ = new Point3dGlView(tab);
    row->addWidget(oriGlView_, 1);

    // Fixed-width side panel: it sits BESIDE the GL view (a separate widget), so
    // the numbers never move when the user orbits/zooms the 3D scene.
    auto *sa = new QScrollArea(tab);
    sa->setFixedWidth(300);
    sa->setWidgetResizable(true);
    oriMetricsHost_ = new QWidget(sa);
    new QVBoxLayout(oriMetricsHost_);
    sa->setWidget(oriMetricsHost_);
    row->addWidget(sa);
}

void Controller3dMeasurement::oriShowResults(const std::map<std::string, double> &angles,
                                             const std::map<std::string, double> &interRay,
                                             const std::map<std::string, double> &thickness,
                                             const std::map<std::string, double> &depth,
                                             double meanErrL, double rmsL, double meanErrR,
                                             double rmsR) {
    if (!oriMetricsHost_) return;
    auto *lay = qobject_cast<QVBoxLayout *>(oriMetricsHost_->layout());
    if (!lay) return;
    // Clear any previous result.
    while (QLayoutItem *it = lay->takeAt(0)) {
        if (it->widget()) it->widget()->deleteLater();
        delete it;
    }

    auto fmt = [](double v, const char *unit) {
        return std::isnan(v) ? QString("N/A") : QString::number(v, 'f', 2) + unit;
    };
    // Every row reads "<what> : <value>"; without the colon a bare
    // "center 14.64 mm" looks like one phrase instead of a name and its number.
    auto rowLabel = [](const QString &name) { return name + " :"; };
    auto addSection = [&](const QString &title, const std::map<std::string, double> &m,
                          const char *unit) {
        auto *gb = new QGroupBox(title, oriMetricsHost_);
        auto *f = new QFormLayout(gb);
        for (const auto &kv : m)
            f->addRow(rowLabel(QString::fromStdString(kv.first)),
                      new QLabel(fmt(kv.second, unit), gb));
        lay->addWidget(gb);
    };
    // Inter-plane angles arrive under both orderings ("cn" and "nc") because the
    // anypoint tab looks the pair up by whichever code its line edit uses. The
    // angle between two planes is one number, so collapse each pair onto its
    // sorted code: 5 planes -> at most 10 rows, spelled out in full.
    {
        const std::map<char, const char *> planeName = {
            {'c', "center"}, {'n', "north"}, {'s', "south"}, {'w', "west"}, {'e', "east"}};
        std::map<std::string, double> pairOnce;
        for (const auto &kv : angles) {
            if (kv.first.size() != 2) continue;
            std::string canon = kv.first;
            if (canon[0] > canon[1]) std::swap(canon[0], canon[1]);
            pairOnce.emplace(canon, kv.second);  // keeps the first, drops the mirror
        }
        auto *gb = new QGroupBox(tr("Inter-plane angle"), oriMetricsHost_);
        auto *f = new QFormLayout(gb);
        for (const auto &kv : pairOnce) {
            auto a = planeName.find(kv.first[0]);
            auto b = planeName.find(kv.first[1]);
            const QString label = (a != planeName.end() && b != planeName.end())
                                      ? QString("%1 - %2").arg(a->second, b->second)
                                      : QString::fromStdString(kv.first);
            f->addRow(rowLabel(label), new QLabel(fmt(kv.second, "°"), gb));
        }
        lay->addWidget(gb);
    }
    addSection(tr("Shortest inter-ray (mean gap)"), interRay, " mm");
    addSection(tr("Plane thickness"), thickness, " mm");
    addSection(tr("Plane depth"), depth, " mm");

    auto *gb = new QGroupBox(tr("Mean reprojection error / RMS"), oriMetricsHost_);
    auto *f = new QFormLayout(gb);
    f->addRow(rowLabel(tr("Left mean")), new QLabel(fmt(meanErrL, " px"), gb));
    f->addRow(rowLabel(tr("Left RMS")), new QLabel(fmt(rmsL, " px"), gb));
    f->addRow(rowLabel(tr("Right mean")), new QLabel(fmt(meanErrR, " px"), gb));
    f->addRow(rowLabel(tr("Right RMS")), new QLabel(fmt(rmsR, " px"), gb));
    lay->addWidget(gb);
    lay->addStretch(1);
}

// ============================ display ============================

void Controller3dMeasurement::show_image_to_label(QLabel *label, const cv::Mat &bgr, int width) {
    if (!label) return;
    if (bgr.empty()) {
        label->clear();
        return;
    }
    const QPixmap pix = matToPixmap(bgr);
    fullPixmap_[label] = pix;  // keep full-res for the fullscreen viewer
    label->setPixmap(pix.scaled(width, width, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void Controller3dMeasurement::showToUi() {
    // Freeze painting for the whole batch: each setPixmap below would otherwise
    // repaint on its own, and the cascade of repaints is what makes the dialog
    // flicker when Calculate/Detect refresh many labels at once.
    setUpdatesEnabled(false);
    for (int pos = 0; pos < 2; ++pos) {
        const std::string cam = kCameras[pos];
        if (!imageSource_[pos].empty())
            if (auto *l = findChild<QLabel *>(QString::fromStdString("label_original_" + cam)))
                show_image_to_label(l, imageSource_[pos], l->width() > 0 ? l->width() : 480);
        for (const auto &dir : kDirections) {
            auto *l = anyLabel(cam, dir);
            const auto &img = anypointImages_[cam][dir];
            if (l) {
                if (!img.empty())
                    show_image_to_label(l, img, l->width() > 0 ? l->width() : 320);
                else
                    l->clear();
            }
        }
        if (auto *l = findChild<QLabel *>(QString::fromStdString("label_" + cam + "_result")))
            if (!resultDraw_[pos].empty()) show_image_to_label(l, resultDraw_[pos], l->width() > 0 ? l->width() : 480);
        if (auto *l = findChild<QLabel *>(QString::fromStdString("label_overlay_original_" + cam)))
            if (!resultDrawOverOri_[pos].empty())
                show_image_to_label(l, resultDrawOverOri_[pos], l->width() > 0 ? l->width() : 480);
    }
    oriShowPanels();  // keep the ORI_DET panels in sync with freshly loaded images

    setUpdatesEnabled(true);  // single repaint now, no flicker

    // setUpdatesEnabled(true) only schedules a repaint of the dialog itself. The
    // anypoint labels live inside inactive QScrollArea viewports in the tab
    // widget, and those children are not reliably included in that repaint -- so
    // a freshly set overlay (the detected X marks) would not appear until a later
    // expose event (e.g. opening/closing the fullscreen viewer) forced a full
    // repaint. Explicitly update every label so the markers show immediately.
    for (QLabel *l : findChildren<QLabel *>()) l->update();
}

void Controller3dMeasurement::showPathToUi(int pos) {
    const std::string key = kCameras[pos];
    if (auto *le = findChild<QLineEdit *>(QString::fromStdString("lineedit_" + key + "_image_path")))
        le->setText(QString::fromStdString(imagePath_[pos]));
    if (auto *le = findChild<QLineEdit *>(QString::fromStdString("lineedit_" + key + "_parameter_path")))
        le->setText(QString::fromStdString(paramPath_[pos]));
}

void Controller3dMeasurement::updateAngleToUi(const std::map<std::string, double> &m) {
    // Python angle_ui_map (note the mismatched suffixes es->se, ws->sw, cs->sc, ns->sn).
    const std::map<std::string, std::string> map = {
        {"wc", "lineEdit_angel_3d_wc"}, {"wn", "lineEdit_angel_3d_wn"},
        {"cn", "lineEdit_angel_3d_cn"}, {"ec", "lineEdit_angel_3d_ec"},
        {"en", "lineEdit_angel_3d_en"}, {"es", "lineEdit_angel_3d_se"},
        {"ws", "lineEdit_angel_3d_sw"}, {"cs", "lineEdit_angel_3d_sc"},
        {"ns", "lineEdit_angel_3d_sn"}, {"we", "lineEdit_angel_3d_we"}};
    for (const auto &kv : map)
        if (auto *e = findChild<QLineEdit *>(QString::fromStdString(kv.second))) {
            auto it = m.find(kv.first);
            e->setText(it != m.end() ? QString::number(it->second, 'f', 2) + "°" : "N/A");
        }
}

void Controller3dMeasurement::updateMeanDistToUi(const std::map<std::string, double> &m) {
    for (const auto &d : {"center", "east", "west", "south", "north", "all"})
        if (auto *e = findChild<QLineEdit *>(QString::fromStdString(std::string("lineEdit_mean_dist_pq_") + d))) {
            auto it = m.find(d);
            e->setText(it != m.end() ? QString::number(it->second, 'f', 2) + " mm" : "N/A");
        }
}

void Controller3dMeasurement::updatePlaneDistToUi(const std::map<std::string, double> &m) {
    for (const auto &d : {"center", "north", "south", "west", "east", "all"})
        if (auto *e = findChild<QLineEdit *>(QString::fromStdString(std::string("lineEdit_plane_dist_") + d))) {
            auto it = m.find(d);
            e->setText(it != m.end() && !std::isnan(it->second)
                           ? QString::number(it->second, 'f', 2) + " mm"
                           : "N/A");
        }
}

void Controller3dMeasurement::updateDepthToUi(const std::map<std::string, double> &m) {
    for (const auto &d : {"center", "north", "south", "east", "west"})
        if (auto *e = findChild<QLineEdit *>(QString::fromStdString(std::string("lineEdit_plane_depth_") + d))) {
            auto it = m.find(d);
            e->setText(it != m.end() && !std::isnan(it->second)
                           ? QString::number(it->second, 'f', 2) + " mm"
                           : "N/A");
        }
}

// ============================ fullscreen viewer ============================

void Controller3dMeasurement::showFullscreenImage(QLabel *label) {
    fsSa_ = uiFull_->scrollArea;
    fsLab_ = uiFull_->label_full_screen;
    // The ORI_DET panels are drawn at 520 px wide, which is useless blown up to
    // a full screen: re-render that panel at full resolution instead, overlay
    // and all, so the clicked corners can be checked against the pixels.
    if (label == oriLabel_[LEFT] || label == oriLabel_[RIGHT]) {
        const int pos = (label == oriLabel_[LEFT]) ? LEFT : RIGHT;
        const cv::Mat full = oriRenderPanel(pos, 1.0);
        fsPixmap_ = full.empty() ? label->pixmap() : matToPixmap(full);
    } else {
        auto it = fullPixmap_.find(label);
        fsPixmap_ = (it != fullPixmap_.end() && !it->second.isNull()) ? it->second
                                                                     : (label->pixmap());
    }
    if (fsPixmap_.isNull()) return;

    fsLab_->setScaledContents(false);
    fsLab_->setAlignment(Qt::AlignCenter);
    fullscreenWindow_->showFullScreen();
    fullscreenWindow_->raise();
    fullscreenWindow_->activateWindow();

    fsZoom_ = 1.0;
    fsDragging_ = false;
    QTimer::singleShot(0, this, [this] { fsFitToView(); });

    fsSa_->viewport()->setFocusPolicy(Qt::StrongFocus);
    fsSa_->viewport()->installEventFilter(this);
    fsLab_->installEventFilter(this);
}

void Controller3dMeasurement::fsFitToView() {
    if (!fsSa_ || !fsLab_ || fsPixmap_.isNull()) return;
    const int vw = fsSa_->viewport()->width();
    const int vh = fsSa_->viewport()->height();
    const double fac = std::min(double(vw) / fsPixmap_.width(), double(vh) / fsPixmap_.height());
    fsZoom_ = fac;
    const int nw = int(fsPixmap_.width() * fac), nh = int(fsPixmap_.height() * fac);
    fsLab_->resize(nw, nh);
    fsLab_->setPixmap(fsPixmap_.scaled(nw, nh, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    fsSa_->widget()->resize(nw, nh);
}

void Controller3dMeasurement::fsZoomImage(double factor, QPoint anchor) {
    if (!fsSa_ || !fsLab_ || fsPixmap_.isNull()) return;
    const double newZoom = std::max(0.05, std::min(20.0, fsZoom_ * factor));
    if (std::abs(newZoom - fsZoom_) < 1e-6) return;
    auto *hbar = fsSa_->horizontalScrollBar();
    auto *vbar = fsSa_->verticalScrollBar();
    const double relX = (hbar->value() + anchor.x()) / double(std::max(1, fsLab_->width()));
    const double relY = (vbar->value() + anchor.y()) / double(std::max(1, fsLab_->height()));
    fsZoom_ = newZoom;
    const int nw = int(fsPixmap_.width() * newZoom), nh = int(fsPixmap_.height() * newZoom);
    fsLab_->resize(nw, nh);
    fsLab_->setPixmap(fsPixmap_.scaled(nw, nh, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    fsSa_->widget()->resize(nw, nh);
    hbar->setValue(int(relX * nw) - anchor.x());
    vbar->setValue(int(relY * nh) - anchor.y());
}

void Controller3dMeasurement::closeFullscreenImage() {
    if (fullscreenWindow_) fullscreenWindow_->close();
    fsSa_ = nullptr;
    fsLab_ = nullptr;
    fsPixmap_ = QPixmap();
}

bool Controller3dMeasurement::eventFilter(QObject *source, QEvent *event) {
    // ORI_DET panels: a single left-click places a calibration corner. A fast
    // second left-click has already placed its corner on the press, so consume
    // the left double-click that follows. A RIGHT double-click is not consumed:
    // it falls through to the fullscreen viewer below.
    if (source == oriLabel_[LEFT] || source == oriLabel_[RIGHT]) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton) {
                oriHandleClick(source == oriLabel_[LEFT] ? LEFT : RIGHT, me->position().toPoint());
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonDblClick) {
            auto *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton) return true;
        }
    }

    // Double-RIGHT-click opens fullscreen. The left button is taken by the
    // ORI_DET corner picking, so the gesture uses the right button and then
    // works on every image panel, the ORI_DET ones included. The fullscreen
    // window itself closes on a double-click with either button.
    if (event->type() == QEvent::MouseButtonDblClick) {
        auto *me = static_cast<QMouseEvent *>(event);
        if (auto *lbl = qobject_cast<QLabel *>(source)) {
            if (lbl == uiFull_->label_full_screen) {
                closeFullscreenImage();
                return true;
            }
            if (me->button() == Qt::RightButton && !lbl->pixmap().isNull()) {
                showFullscreenImage(lbl);
                return true;
            }
        }
    }

    // Fullscreen zoom/pan.
    if (fsSa_ && fsLab_) {
        QWidget *vp = fsSa_->viewport();
        if (source == vp || source == fsLab_) {
            if (event->type() == QEvent::Wheel) {
                auto *we = static_cast<QWheelEvent *>(event);
                const int delta = we->angleDelta().y();
                if (delta != 0) {
                    fsZoomImage(delta > 0 ? 1.25 : 0.8, we->position().toPoint());
                    return true;
                }
            } else if (event->type() == QEvent::MouseButtonPress) {
                auto *me = static_cast<QMouseEvent *>(event);
                if (me->button() == Qt::LeftButton) {
                    fsDragging_ = true;
                    fsLastMouse_ = me->position().toPoint();
                    return true;
                }
            } else if (event->type() == QEvent::MouseMove && fsDragging_) {
                auto *me = static_cast<QMouseEvent *>(event);
                const QPoint cur = me->position().toPoint();
                const int dx = cur.x() - fsLastMouse_.x();
                const int dy = cur.y() - fsLastMouse_.y();
                fsLastMouse_ = cur;
                fsSa_->horizontalScrollBar()->setValue(fsSa_->horizontalScrollBar()->value() - dx);
                fsSa_->verticalScrollBar()->setValue(fsSa_->verticalScrollBar()->value() - dy);
                return true;
            } else if (event->type() == QEvent::MouseButtonRelease) {
                fsDragging_ = false;
                return true;
            }
        }
    }
    return QDialog::eventFilter(source, event);
}

// ---- Exact Python API (snake_case 1:1): live-capture path -----------------
// The C++ port replaces Python's axis-driven live capture with an offline,
// image-file workflow (see onclickOpenMedia / handleStartCalculation). These
// methods keep the Python names; they require the axis/camera/monitor hardware
// servers, so offline they are structural no-ops.
void Controller3dMeasurement::onclick_btn_start() { handleStartCalculation(); }
void Controller3dMeasurement::auto_3d_measurement(const std::string &, int) {}
void Controller3dMeasurement::onclick_btn_stop() {}
void Controller3dMeasurement::set_moniter_viewer(const std::string &) {}
void Controller3dMeasurement::control_cam_direction(const std::string &, int) {}

std::pair<int, int> Controller3dMeasurement::get_random_point(int x_lower_limit, int x_upper_limit,
                                                              int y_lower_limit, int y_upper_limit) {
    // Deterministic midpoint (Math.random is unavailable / non-reproducible here).
    return {(x_lower_limit + x_upper_limit) / 2, (y_lower_limit + y_upper_limit) / 2};
}

QImage Controller3dMeasurement::cv2_image_to_q_image(const cv::Mat &bgr_image) {
    if (bgr_image.empty()) return {};
    cv::Mat rgb;
    cv::cvtColor(bgr_image, rgb, cv::COLOR_BGR2RGB);
    return QImage(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step), QImage::Format_RGB888)
        .copy();
}
