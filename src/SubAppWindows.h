#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>

class Controller3dMeasurement;
class ControllerCenterSetup;

class SubAppWindows : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit SubAppWindows(QObject *parent = nullptr) : QObject(parent) {}
    ~SubAppWindows();

    // Opens the 3D Verification sub-app. That screen is still the old QWidget +
    // uic dialog, copied unchanged from v2.0_2026_main-cpp-ros, so it appears as
    // its own top-level window rather than inside the QML scene.
    Q_INVOKABLE void openMeasure3d();
    Q_INVOKABLE void openCenterSetup();

private:
    Controller3dMeasurement *measure3d_ = nullptr;
    ControllerCenterSetup *centerSetup_ = nullptr;
};
