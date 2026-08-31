#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>

class Controller3dMeasurement;
class ControllerCenterSetup;

// The C++ side of the QML <-> C++ conversation, same role ControllerMain
// plays in the old widgets app: QML calls Q_INVOKABLE methods, and reads/
// writes Q_PROPERTY values.
class Bridge : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(int clickCount READ clickCount NOTIFY clickCountChanged)

public:
    explicit Bridge(QObject *parent = nullptr) : QObject(parent) {}
    ~Bridge();

    int clickCount() const { return clickCount_; }

    Q_INVOKABLE void handleClick() {
        ++clickCount_;
        emit clickCountChanged();
    }

    // Opens the 3D Verification sub-app. That screen is still the old QWidget +
    // uic dialog, copied unchanged from v2.0_2026_main-cpp-ros, so it appears as
    // its own top-level window rather than inside the QML scene.
    Q_INVOKABLE void openMeasure3d();
    Q_INVOKABLE void openCenterSetup();

signals:
    void clickCountChanged();

private:
    int clickCount_ = 0;
    Controller3dMeasurement *measure3d_ = nullptr;
    ControllerCenterSetup *centerSetup_ = nullptr;
};
