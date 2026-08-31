#include "Bridge.h"

#ifdef FISHEYE_SUBAPPS_ENABLED

#include "controller_auto_3d_measurement.h"
#include "controller_center_setup.h"

Bridge::~Bridge() { 
    delete measure3d_; 
    delete centerSetup_;
}

void Bridge::openMeasure3d() {
    if (!measure3d_) {
       
        measure3d_ = new Controller3dMeasurement(nullptr, nullptr, nullptr);
    }

    measure3d_->show();
    measure3d_->raise();
    measure3d_->activateWindow();
}

void Bridge::openCenterSetup() {
    if (!centerSetup_) {
        centerSetup_ = new ControllerCenterSetup(nullptr);
    }
    centerSetup_->show();
    centerSetup_->raise();
    centerSetup_->activateWindow();
}

#else

#include <QDebug>

Bridge::~Bridge() = default;

void Bridge::openMeasure3d() {
    qWarning() << "3D Verification is unavailable: this build was configured with "
                  "FISHEYE_ENABLE_SUBAPPS=OFF or without Qt Widgets / OpenCV 4 / Eigen3.";
}

void Bridge::openCenterSetup() {
    qWarning() << "Center Setup is unavailable: this build was configured with "
                  "FISHEYE_ENABLE_SUBAPPS=OFF or without Qt Widgets / OpenCV 4 / Eigen3.";
}

#endif
