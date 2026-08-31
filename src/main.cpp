#ifdef FISHEYE_SUBAPPS_ENABLED
#include <QApplication>
using FisheyeApplication = QApplication;
#else
#include <QGuiApplication>
using FisheyeApplication = QGuiApplication;
#endif

#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QStyleHints>
#include "SubAppWindows.h"
#include "ServerProbe.h"


int main(int argc, char *argv[]) {
    
    FisheyeApplication app(argc, argv);
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
