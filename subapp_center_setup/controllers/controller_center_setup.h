#pragma once

#include <functional>

#include <QByteArray>
#include <QDialog>

#include <opencv2/core.hpp>

#include "Moildev.h"

class QLabel;
class QSpinBox;
class QDoubleSpinBox;
class QComboBox;
class QLineEdit;
class QPushButton;
class QCheckBox;
class QSlider;
class QTimer;

QT_BEGIN_NAMESPACE
namespace Ui {
class Dialog_Center_Setup;
}
QT_END_NAMESPACE

// "Setup Center" tool (port of the Python SetupCenterImage app). Input is limited
// to a fisheye image + a Moildev camera-parameter JSON (no live camera/video).
// Nudge the centre (iCx/iCy) — by clicking the Original view or via the spin boxes
// — while the guide rings, the panorama and the anypoint previews confirm the
// centre. Save Center writes iCx/iCy back into the JSON.
class ControllerCenterSetup : public QDialog {
    Q_OBJECT
public:
    explicit ControllerCenterSetup(QWidget *parent = nullptr);
    ~ControllerCenterSetup() override;

    void setImageMat(const cv::Mat &bgr);            // load a fisheye directly
    void setParameterFile(const QString &jsonPath);  // preselect a camera JSON

    // Lets the dialog grab a frame from the rig instead of only reading files.
    // The host app supplies the callback (it owns the camera client), which keeps
    // this dialog free of ROS and avoids a second camera node. Setting it enables
    // the "Capture from Camera" button; without it that button stays disabled.
    // The callback is invoked on a worker thread and must block until it has the
    // encoded image bytes (empty on failure).
    void setCaptureProvider(std::function<QByteArray()> provider);

protected:
    bool eventFilter(QObject *obj, QEvent *ev) override;

private:
    void buildUi();  // setupUi + the wiring uic cannot express
    void openImage();
    void captureFromCamera();
    void handleCapturedBytes(const QByteArray &bytes);  // UI thread
    void ensureParametersThenConfigure();
    void showRawPreview();  // display the frame before any JSON is chosen
    void browseParameterFile();
    void loadParameterFile(const QString &path);
    void configureForImage();
    void updateModeLabels();
    void scheduleRebuild();
    void rebuild();
    void updateOriginalView();
    void updatePanoramaView();
    void renderPanorama();     // draw slider line + scale the cached panorama
    void updateAnypointView();
    void saveCenter();
    std::vector<double> guideAlphas() const;

    // ---- state ----
    cv::Mat rawBgr_;           // fisheye as loaded (native pixels)
    cv::Mat srcBgr_;           // fisheye resized to the camera's calib resolution
    cv::Mat panoMat_;          // cached panorama (BGR, 2x-stretched) sans slider line
    Moildev moil_;
    std::function<QByteArray()> captureProvider_;
    QString paramPath_;
    QString cameraType_;
    double workRatio_ = 1.0;   // maps built at a reduced resolution for speed

    double oriScale_ = 1.0;
    int oriOffX_ = 0, oriOffY_ = 0;
    bool updatingSpins_ = false;

    // ---- widgets ----
    // Owned by the form (ui/center_setup.ui); the pointers below just alias the
    // members of `ui_` so the rest of the class reads the same as before.
    Ui::Dialog_Center_Setup *ui_ = nullptr;
    QLabel *oriLabel_ = nullptr;
    QLabel *panoLabel_ = nullptr;
    QLabel *anyLabel_ = nullptr;
    QLineEdit *imagePathEdit_ = nullptr;
    QLineEdit *paramPathEdit_ = nullptr;
    QLabel *resLabel_ = nullptr;
    QSpinBox *spinIcx_ = nullptr;
    QSpinBox *spinIcy_ = nullptr;
    QPushButton *btnCaptureCamera_ = nullptr;
    QPushButton *btnPanorama_ = nullptr;
    QPushButton *btnAnypoint_ = nullptr;
    QCheckBox *guideCheck_ = nullptr;
    QLineEdit *guideAlphasEdit_ = nullptr;
    QComboBox *modeCombo_ = nullptr;
    QLabel *lblP1_ = nullptr;
    QLabel *lblP2_ = nullptr;
    QDoubleSpinBox *spinP1_ = nullptr;   // alpha (mode1) / pitch (mode2)
    QDoubleSpinBox *spinP2_ = nullptr;   // beta  (mode1) / yaw   (mode2)
    QDoubleSpinBox *spinZoom_ = nullptr;
    QSlider *panoSlider_ = nullptr;
    QTimer *rebuildTimer_ = nullptr;
};
