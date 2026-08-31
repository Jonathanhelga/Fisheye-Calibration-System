#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QStyleHints>
#include "Bridge.h"
#include "ServerProbe.h"


int main(int argc, char *argv[]) {
    
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
