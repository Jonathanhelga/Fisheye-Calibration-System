#include "server_context.h"

#include <cstdio>

#include <QDateTime>
#include <QStringList>

#include "ComputeOps.h"

void ServerContext::start() {
    startedAtMs = QDateTime::currentMSecsSinceEpoch();

    config = DeviceConfig::load();
    origins = AxisOrigin::load();

    // Order matters only for the log: the axis is the device most likely to fail
    // to open, and the operator reads the first lines.
    axis = std::make_unique<AxisDevice>(config);
    if (!axis->isOpen())
        std::fprintf(stderr, "[axis] %s\n", qPrintable(axis->lastError()));
    else
        std::fprintf(stderr, "[axis] open: %s\n", qPrintable(axis->url()));

    camera = std::make_unique<CameraDevice>(config);
    std::fprintf(stderr, "[camera] %s: %s\n", camera->isOpen() ? "open" : "not open",
                 qPrintable(camera->url()));

    monitor = std::make_unique<MonitorDevice>(config);
    const QStringList screens = monitor->describeScreens();
    std::fprintf(stderr, "[monitor] %s\n", qPrintable(monitorUrl()));
    for (const QString &s : screens) std::fprintf(stderr, "[monitor]   %s\n", qPrintable(s));

    // Tell the compute engine where the prepared patterns live, so auto_center can
    // count the rings of the picture that was on the glass rather than guess from
    // the capture. A path, not the device: runDetectOp is called from standalone
    // test binaries too, and a MonitorDevice parameter would drag the device layer
    // into all of them. Set once here, before any request is served.
    ComputeOps::setPreparedDir(monitor->preparedDir());
    std::fprintf(stderr, "[monitor] prepared patterns in %s\n",
                 qPrintable(monitor->preparedDir()));

    sessions = std::make_unique<SessionStore>(camera.get());
    std::fprintf(stderr, "[session] store at %s\n", qPrintable(sessions->sessionsDir()));

    // The origins were restored from disk, which means they carry the caveat
    // AxisOrigin documents: nothing in the protocol says whether the controller
    // lost power while the server was down. Say so once, here, rather than
    // letting five coordinate fields quietly claim a reference they may not have.
    if (origins.restored)
        std::fprintf(stderr,
                     "[axis] restored software zeros from disk -- PROVISIONAL until re-homed\n");

    // Nothing above throws on failure: a rig with an unplugged camera must still
    // serve the axis, and a client must be able to connect and be TOLD what is
    // missing. `ready` means the server is answering, not that the rig is whole
    // -- SystemStatus carries the per-subsystem truth.
    ready = true;
}

QString ServerContext::axisUrl() const { return axis ? axis->url() : QStringLiteral("(no axis)"); }

QString ServerContext::cameraUrl() const {
    return camera ? camera->url() : QStringLiteral("(no camera)");
}

QString ServerContext::monitorUrl() const {
    if (!monitor) return QStringLiteral("(no monitor)");
    const int screens = monitor->describeScreens().size();
    int mapped = 0;
    for (const char *d : {"top", "n", "w", "s", "e"})
        if (!monitor->get_display_direction(QString::fromLatin1(d)).isEmpty()) ++mapped;
    if (mapped == 0)
        return QStringLiteral("%1 screens, no saved display mapping -- directions are UNSET")
            .arg(screens);
    return QStringLiteral("%1 screens, %2 mapped").arg(screens).arg(mapped);
}
