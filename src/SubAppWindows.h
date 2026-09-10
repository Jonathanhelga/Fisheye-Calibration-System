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

    // Opens 3D Verification as its own window.
    Q_INVOKABLE void openMeasure3d();
    Q_INVOKABLE void openCenterSetup();

private:
    Controller3dMeasurement *measure3d_ = nullptr;
    ControllerCenterSetup *centerSetup_ = nullptr;
};
