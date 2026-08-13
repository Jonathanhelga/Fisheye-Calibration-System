#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include "Bridge.h"

// Ground-up QML rebuild of the calibration app (see moil-fisheye-calibration-system/cpp).
// No QMainWindow/QQuickWidget host here on purpose: the old app was Widgets-first
// with QML bolted on; this one is QML-first from the start.
int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;

    //  one bridge per panel (AxisBridge, CameraBridge, ...) exposed the same way.
    Bridge bridge;
    engine.rootContext()->setContextProperty("bridge", &bridge);

    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, [] { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    engine.loadFromModule("FisheyeCaliJojo", "Main");

    return app.exec();
}
