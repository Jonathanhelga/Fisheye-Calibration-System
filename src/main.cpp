#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QStyleHints>
#include "Bridge.h"
#include "ServerProbe.h"

// Ground-up QML rebuild of the calibration app (see moil-fisheye-calibration-system/cpp).
// No QMainWindow/QQuickWidget host here on purpose: the old app was Widgets-first
// with QML bolted on; this one is QML-first from the start.
int main(int argc, char *argv[]) {
    // QApplication, not QGuiApplication: the 3D Verification sub-app is still
    // QWidget-based, and widgets need the QtWidgets application object.
    QApplication app(argc, argv);
    QQuickStyle::setStyle("Basic");

    app.styleHints()->setColorScheme(Qt::ColorScheme::Light);

    QQmlApplicationEngine engine;

    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, [] { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    engine.loadFromModule("FisheyeCaliJojo", "Main");

    return app.exec();
}
