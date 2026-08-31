#include "controller_center_setup.h"

#include <algorithm>
#include <cmath>

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPixmap>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QTimer>
#include <QtConcurrent>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "Help.h"
#include "ui_center_setup.h"

namespace {
QImage matToQImage(const cv::Mat &bgr) {
    if (bgr.empty()) return {};
    cv::Mat rgb;
    cv::cvtColor(bgr, rgb, cv::COLOR_BGR2RGB);
    return QImage(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step),
                  QImage::Format_RGB888)
        .copy();
}
}  // namespace

ControllerCenterSetup::ControllerCenterSetup(QWidget *parent)
    : QDialog(parent), ui_(new Ui::Dialog_Center_Setup) {
    buildUi();

    rebuildTimer_ = new QTimer(this);
    rebuildTimer_->setSingleShot(true);
    rebuildTimer_->setInterval(150);
    connect(rebuildTimer_, &QTimer::timeout, this, [this] { rebuild(); });

    // No parameter JSON is auto-loaded: every camera carries its own file, so
    // guessing one from the working directory would silently calibrate against
    // the wrong camera. The path arrives via setParameterFile() from the host
    // app, or from the user through "Open camera-parameter JSON".
    updateModeLabels();

    Help::attach(this, "center_setup");
}

ControllerCenterSetup::~ControllerCenterSetup() { delete ui_; }

// The layout, texts, ranges and styling all live in ui/center_setup.ui. What is
// left here is the part uic cannot express: aliasing the form's widgets onto the
// member pointers the rest of the class uses, and the signal wiring.
void ControllerCenterSetup::buildUi() {
    ui_->setupUi(this);

    oriLabel_ = ui_->oriLabel;
    panoLabel_ = ui_->panoLabel;
    anyLabel_ = ui_->anyLabel;
    panoSlider_ = ui_->panoSlider;
    imagePathEdit_ = ui_->imagePathEdit;
    paramPathEdit_ = ui_->paramPathEdit;
    resLabel_ = ui_->resLabel;
    spinIcx_ = ui_->spinIcx;
    spinIcy_ = ui_->spinIcy;
    btnCaptureCamera_ = ui_->btnCaptureCamera;
    btnPanorama_ = ui_->btnPanorama;
    btnAnypoint_ = ui_->btnAnypoint;
    guideCheck_ = ui_->guideCheck;
    guideAlphasEdit_ = ui_->guideAlphasEdit;
    modeCombo_ = ui_->modeCombo;
    lblP1_ = ui_->lblP1;
    lblP2_ = ui_->lblP2;
    spinP1_ = ui_->spinP1;
    spinP2_ = ui_->spinP2;
    spinZoom_ = ui_->spinZoom;

    // Clicking the Original view aims the anypoint preview.
    oriLabel_->installEventFilter(this);

    connect(panoSlider_, &QSlider::valueChanged, this, [this] { renderPanorama(); });
    connect(ui_->btnOpenImage, &QPushButton::clicked, this, [this] { openImage(); });
    connect(btnCaptureCamera_, &QPushButton::clicked, this, [this] { captureFromCamera(); });
    connect(ui_->btnOpenParam, &QPushButton::clicked, this, [this] { browseParameterFile(); });
    connect(ui_->btnSaveCenter, &QPushButton::clicked, this, [this] { saveCenter(); });

    for (QSpinBox *s : {spinIcx_, spinIcy_})
        connect(s, QOverload<int>::of(&QSpinBox::valueChanged), this,
                [this] { if (!updatingSpins_) scheduleRebuild(); });

    connect(btnPanorama_, &QPushButton::toggled, this, [this] { updatePanoramaView(); });
    connect(btnAnypoint_, &QPushButton::toggled, this, [this] { updateAnypointView(); });
    connect(guideCheck_, &QCheckBox::toggled, this, [this] { updateOriginalView(); });
    connect(guideAlphasEdit_, &QLineEdit::editingFinished, this, [this] { updateOriginalView(); });
    connect(modeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this] { updateModeLabels(); scheduleRebuild(); });
    for (QDoubleSpinBox *s : {spinP1_, spinP2_, spinZoom_})
        connect(s, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                [this] { scheduleRebuild(); });
}

void ControllerCenterSetup::updateModeLabels() {
    const bool mode2 = modeCombo_->currentIndex() == 1;
    lblP1_->setText(mode2 ? tr("Pitch (°):") : tr("Alpha (°):"));
    lblP2_->setText(mode2 ? tr("Yaw (°):") : tr("Beta (°):"));
    const double p1 = spinP1_->value(), p2 = spinP2_->value();
    spinP1_->setRange(mode2 ? -90 : -110, mode2 ? 90 : 110);
    spinP2_->setRange(mode2 ? -180 : 0, mode2 ? 180 : 360);
    spinP1_->setValue(std::min(spinP1_->maximum(), std::max(spinP1_->minimum(), p1)));
    spinP2_->setValue(std::min(spinP2_->maximum(), std::max(spinP2_->minimum(), p2)));
}

// -------------------------------------------------------------- seeding ----

void ControllerCenterSetup::setImageMat(const cv::Mat &bgr) {
    if (bgr.empty()) return;
    rawBgr_ = bgr.clone();
    imagePathEdit_->setText(tr("(from host app)"));
    configureForImage();
}

void ControllerCenterSetup::setParameterFile(const QString &jsonPath) {
    if (!jsonPath.isEmpty()) loadParameterFile(jsonPath);
}

void ControllerCenterSetup::setCaptureProvider(std::function<QByteArray()> provider) {
    captureProvider_ = std::move(provider);
    // The button is disabled in the form, so a dialog opened without a host app
    // (no camera to talk to) never offers a capture that cannot work.
    if (btnCaptureCamera_) btnCaptureCamera_->setEnabled(static_cast<bool>(captureProvider_));
}

// --------------------------------------------------------------- source ----

void ControllerCenterSetup::openImage() {
    const QString f = QFileDialog::getOpenFileName(
        this, tr("Open fisheye image"), QDir::currentPath(),
        tr("Images (*.png *.jpg *.jpeg *.bmp)"));
    if (f.isEmpty()) return;
    cv::Mat img = cv::imread(f.toStdString(), cv::IMREAD_COLOR);
    if (img.empty()) {
        QMessageBox::warning(this, tr("Open Image"), tr("Could not read the image."));
        return;
    }
    rawBgr_ = img;
    imagePathEdit_->setText(f);
    ensureParametersThenConfigure();
}

// Grabs a frame from the rig's camera server -- the same capture the main
// window's Capture button performs -- so the centre can be set on a live shot
// without saving a file and browsing back to it.
void ControllerCenterSetup::captureFromCamera() {
    if (!captureProvider_) return;
    // The capture blocks for seconds (fresh grab on a 3040x3040 stream, encode,
    // transfer), so it runs off the UI thread exactly like the main window's.
    btnCaptureCamera_->setEnabled(false);
    btnCaptureCamera_->setText(tr("Capturing…"));
    auto provider = captureProvider_;
    (void)QtConcurrent::run([this, provider] {
        const QByteArray bytes = provider();
        // Queued: if the dialog is destroyed first (Reset recreates the
        // sub-windows), ~QObject drops this call rather than delivering it.
        QMetaObject::invokeMethod(this, [this, bytes] { handleCapturedBytes(bytes); });
    });
}

void ControllerCenterSetup::handleCapturedBytes(const QByteArray &bytes) {
    btnCaptureCamera_->setText(tr("Capture from Camera"));
    btnCaptureCamera_->setEnabled(true);
    if (bytes.isEmpty()) {
        QMessageBox::warning(this, tr("Capture failed"),
                             tr("No image received from the camera server.\n\n"
                                "Make sure the camera server is running and reachable."));
        return;
    }
    const std::vector<uchar> buf(bytes.begin(), bytes.end());
    const cv::Mat img = cv::imdecode(buf, cv::IMREAD_COLOR);
    if (img.empty()) {
        QMessageBox::warning(this, tr("Capture failed"),
                             tr("Received data could not be decoded as an image."));
        return;
    }
    rawBgr_ = img;
    imagePathEdit_->setText(tr("(live capture from camera)"));
    // Deliberately NOT chained to the parameter prompt the way "Open Image" is:
    // this button must open the camera, not a file dialog. The frame is shown
    // straight away; the JSON is picked separately whenever the user wants the
    // rings, panorama and anypoint.
    configureForImage();
}

// Shared by "Open Image" and "Capture from Camera". Mirrors the 3D measurement
// tool's "open media": a fisheye on its own renders nothing, so ask for this
// camera's parameter JSON right away when none is loaded yet. An already-loaded
// JSON is reused -- switch cameras with "Open Parameter JSON", which stays
// available.
void ControllerCenterSetup::ensureParametersThenConfigure() {
    if (paramPath_.isEmpty()) {
        browseParameterFile();  // on success this loads the JSON and rebuilds
        if (!paramPath_.isEmpty()) return;
    }
    configureForImage();  // no JSON (cancelled): shows the "open the JSON" hint
}

// ----------------------------------------------------------- parameters ----

void ControllerCenterSetup::browseParameterFile() {
    const QString f = QFileDialog::getOpenFileName(
        this, tr("Open camera-parameter JSON"), QDir::currentPath(), tr("JSON (*.json)"));
    if (!f.isEmpty()) loadParameterFile(f);
}

void ControllerCenterSetup::loadParameterFile(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("Parameter JSON"), tr("Could not open:\n%1").arg(path));
        return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (!doc.isObject()) {
        QMessageBox::warning(this, tr("Parameter JSON"), tr("Not a valid JSON object."));
        return;
    }
    paramPath_ = path;
    paramPathEdit_->setText(path);
    const QJsonObject root = doc.object();

    // Single-camera file => use it top-level; multi-camera dict => first entry.
    if (root.contains("cameraName")) {
        cameraType_.clear();
    } else {
        cameraType_.clear();
        for (auto it = root.begin(); it != root.end(); ++it)
            if (it.value().isObject()) { cameraType_ = it.key(); break; }
        if (cameraType_.isEmpty()) {
            QMessageBox::warning(this, tr("Parameter JSON"),
                                 tr("No camera parameters found in this file."));
            paramPath_.clear();  // unusable: don't let it count as "loaded"
            paramPathEdit_->clear();
            return;
        }
    }
    configureForImage();
}

// ---------------------------------------------------- (re)build Moildev ----

void ControllerCenterSetup::configureForImage() {
    if (paramPath_.isEmpty()) {
        showRawPreview();  // no fisheye model yet: plain image, no rings/panorama
        return;
    }
    // First load at native resolution to learn the calibration size.
    if (!moil_.load(paramPath_.toStdString(), cameraType_.toStdString(), 1.0) || !moil_.valid()) {
        resLabel_->setText(tr("Resolution: — (invalid parameters)"));
        return;
    }
    const int nativeW = moil_.imageWidth(), nativeH = moil_.imageHeight();

    // Build the anypoint/panorama maps at a reduced resolution so opening + each
    // centre nudge stay fast (the Python app likewise works at display size).
    constexpr int kWorkMax = 1400;
    const int longSide = std::max(nativeW, nativeH);
    workRatio_ = longSide > kWorkMax ? static_cast<double>(kWorkMax) / longSide : 1.0;
    if (workRatio_ != 1.0)
        moil_.load(paramPath_.toStdString(), cameraType_.toStdString(), workRatio_);

    const int W = moil_.imageWidth(), H = moil_.imageHeight();  // working resolution
    resLabel_->setText(tr("Resolution: %1 × %2   ·  fov %3°")
                           .arg(nativeW).arg(nativeH).arg(moil_.cameraFov(), 0, 'f', 0));

    updatingSpins_ = true;
    spinIcx_->setRange(0, W - 1);
    spinIcy_->setRange(0, H - 1);
    spinIcx_->setValue(moil_.icx());
    spinIcy_->setValue(moil_.icy());
    updatingSpins_ = false;

    if (!rawBgr_.empty()) {
        if (rawBgr_.cols == W && rawBgr_.rows == H)
            srcBgr_ = rawBgr_;
        else
            cv::resize(rawBgr_, srcBgr_, cv::Size(W, H));
    }
    rebuild();
}

// Shows whatever fisheye is loaded even with no parameter JSON yet. Without
// parameters there is no fisheye model, so this is a plain preview: no guide
// rings, no panorama and no anypoint. The centre crosshair still tracks the spin
// boxes, so the frame is useful the moment it arrives from the camera.
void ControllerCenterSetup::showRawPreview() {
    if (rawBgr_.empty()) return;
    srcBgr_ = rawBgr_;
    updatingSpins_ = true;
    spinIcx_->setRange(0, srcBgr_.cols - 1);
    spinIcy_->setRange(0, srcBgr_.rows - 1);
    spinIcx_->setValue(srcBgr_.cols / 2);
    spinIcy_->setValue(srcBgr_.rows / 2);
    updatingSpins_ = false;
    resLabel_->setText(tr("Resolution: %1 × %2  ·  open this camera's parameter JSON")
                           .arg(srcBgr_.cols)
                           .arg(srcBgr_.rows));
    updateOriginalView();
}

void ControllerCenterSetup::scheduleRebuild() {
    if (rebuildTimer_) rebuildTimer_->start();
}

void ControllerCenterSetup::rebuild() {
    if (!moil_.valid()) {  // no parameters: only the plain preview can be redrawn
        updateOriginalView();
        return;
    }
    moil_.setCenter(spinIcx_->value(), spinIcy_->value());
    updateOriginalView();
    updatePanoramaView();
    updateAnypointView();
}

std::vector<double> ControllerCenterSetup::guideAlphas() const {
    std::vector<double> out;
    const QStringList parts = guideAlphasEdit_->text().split(',', Qt::SkipEmptyParts);
    for (const QString &p : parts) {
        bool ok = false;
        const double v = p.trimmed().toDouble(&ok);
        if (ok && v > 0) out.push_back(v);
    }
    return out;
}

void ControllerCenterSetup::updateOriginalView() {
    if (srcBgr_.empty()) return;
    cv::Mat img = srcBgr_.clone();
    const int icx = spinIcx_->value(), icy = spinIcy_->value();
    const double fov = moil_.valid() ? moil_.cameraFov() : 0.0;
    const cv::Scalar red(0, 0, 255), yellow(0, 255, 255), cyan(255, 255, 0);
    const int th = std::max(1, img.cols / 700);

    cv::line(img, {0, icy}, {img.cols, icy}, red, th);
    cv::line(img, {icx, 0}, {icx, img.rows}, red, th);
    const int L = img.cols + img.rows;
    cv::line(img, {icx - L, icy - L}, {icx + L, icy + L}, red, th);
    cv::line(img, {icx - L, icy + L}, {icx + L, icy - L}, red, th);
    if (guideCheck_->isChecked() && moil_.valid()) {
        for (double a : guideAlphas()) {
            if (a > fov / 2.0 + 1e-6) continue;
            const int rho = static_cast<int>(std::lround(moil_.getRhoFromAlpha(a)));
            if (rho > 0)
                cv::circle(img, {icx, icy}, rho, (std::abs(a - fov / 2.0) < 1.0 ? cyan : yellow), th);
        }
    }
    cv::circle(img, {icx, icy}, std::max(3, th * 3), red, -1);

    const QImage q = matToQImage(img);
    const QPixmap scaled = QPixmap::fromImage(q).scaled(
        oriLabel_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    oriScale_ = q.width() > 0 ? static_cast<double>(scaled.width()) / q.width() : 1.0;
    oriOffX_ = (oriLabel_->width() - scaled.width()) / 2;
    oriOffY_ = (oriLabel_->height() - scaled.height()) / 2;
    oriLabel_->setPixmap(scaled);
}

void ControllerCenterSetup::updatePanoramaView() {
    if (!btnPanorama_->isChecked()) {
        panoMat_.release();
        panoLabel_->setText(tr("Panorama off"));
        return;
    }
    if (srcBgr_.empty() || !moil_.valid()) return;
    const double alphaMax = std::round(moil_.cameraFov() / 2.0);
    cv::Mat mapX, mapY, pano;
    moil_.mapsPanoramaCar(alphaMax, 0, 0, false, mapX, mapY);
    if (mapX.empty()) return;
    cv::remap(srcBgr_, pano, mapX, mapY, cv::INTER_LINEAR);
    cv::resize(pano, panoMat_, cv::Size(pano.cols * 2, pano.rows));  // 2x wide, like Python
    renderPanorama();
}

void ControllerCenterSetup::renderPanorama() {
    if (panoMat_.empty()) return;
    cv::Mat img = panoMat_.clone();
    const int x = static_cast<int>(panoSlider_->value() / 100.0 * (img.cols - 1));
    cv::line(img, {x, 0}, {x, img.rows}, cv::Scalar(0, 255, 0), std::max(1, img.cols / 600));
    panoLabel_->setPixmap(QPixmap::fromImage(matToQImage(img))
                              .scaled(panoLabel_->size(), Qt::KeepAspectRatio,
                                      Qt::SmoothTransformation));
}

void ControllerCenterSetup::updateAnypointView() {
    if (!btnAnypoint_->isChecked()) {
        anyLabel_->setText(tr("Anypoint off"));
        return;
    }
    if (srcBgr_.empty() || !moil_.valid()) return;
    cv::Mat mapX, mapY, any;
    if (modeCombo_->currentIndex() == 1)
        moil_.mapsAnypointMode2(spinP1_->value(), spinP2_->value(), spinZoom_->value(), mapX, mapY);
    else
        moil_.mapsAnypointMode1(spinP1_->value(), spinP2_->value(), spinZoom_->value(), mapX, mapY);
    if (mapX.empty()) return;
    cv::remap(srcBgr_, any, mapX, mapY, cv::INTER_LINEAR);
    anyLabel_->setPixmap(QPixmap::fromImage(matToQImage(any))
                             .scaled(anyLabel_->size(), Qt::KeepAspectRatio,
                                     Qt::SmoothTransformation));
}

// ----------------------------------------------------------------- save ----

void ControllerCenterSetup::saveCenter() {
    if (paramPath_.isEmpty() || !moil_.valid()) {
        QMessageBox::information(this, tr("Save Center"),
                                 tr("Load a camera parameter JSON first."));
        return;
    }
    QFile file(paramPath_);
    if (!file.open(QIODevice::ReadOnly)) return;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (!doc.isObject()) return;
    QJsonObject root = doc.object();

    // Spin values are in working resolution; convert back to native for the file.
    const int icx = static_cast<int>(std::lround(spinIcx_->value() / workRatio_));
    const int icy = static_cast<int>(std::lround(spinIcy_->value() / workRatio_));
    if (cameraType_.isEmpty()) {
        root["iCx"] = icx;
        root["iCy"] = icy;
    } else if (root.contains(cameraType_) && root.value(cameraType_).isObject()) {
        QJsonObject cam = root.value(cameraType_).toObject();
        cam["iCx"] = icx;
        cam["iCy"] = icy;
        root[cameraType_] = cam;
    } else {
        QMessageBox::warning(this, tr("Save Center"), tr("Camera entry not found in JSON."));
        return;
    }

    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, tr("Save Center"), tr("Could not write:\n%1").arg(paramPath_));
        return;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.close();
    QMessageBox::information(this, tr("Save Center"),
                             tr("Saved iCx=%1, iCy=%2 to:\n%3").arg(icx).arg(icy).arg(paramPath_));
}

// -------------------------------------------------------- click-to-set ----

bool ControllerCenterSetup::eventFilter(QObject *obj, QEvent *ev) {
    if (obj == oriLabel_ && ev->type() == QEvent::MouseButtonPress && !srcBgr_.empty()) {
        auto *e = static_cast<QMouseEvent *>(ev);
        if (e->button() == Qt::LeftButton && oriScale_ > 0) {
            const int ix = static_cast<int>((e->pos().x() - oriOffX_) / oriScale_);
            const int iy = static_cast<int>((e->pos().y() - oriOffY_) / oriScale_);
            if (ix >= 0 && ix < srcBgr_.cols && iy >= 0 && iy < srcBgr_.rows) {
                updatingSpins_ = true;
                spinIcx_->setValue(ix);
                spinIcy_->setValue(iy);
                updatingSpins_ = false;
                rebuild();
            }
            return true;
        }
    }
    return QDialog::eventFilter(obj, ev);
}
