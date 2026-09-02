#pragma once

#include <memory>
#include <mutex>

#include <QString>
#include <QtGlobal>

#include "axis_device.h"
#include "axis_origin.h"
#include "camera_device.h"
#include "compute.h"
#include "device_config.h"
#include "monitor_device.h"
#include "session_store.h"

// Everything the server owns, in one place, constructed once.
//
// There is exactly one of each device because each holds an OS resource for the
// life of the process -- the axis its two COM ports, the camera its device. A
// second AxisDevice would fail to open ports this one already has, and the
// failure reads as "COM3 is not found", which sends whoever sees it looking at
// the cable.
//
// This is also why the whole server is ONE process rather than the three the rig
// ran in V2.0.0. The job orchestration (auto-calibrate, drive-to-limit) needs the
// axis, the camera, the compute pipeline and the session together, in a loop
// tight enough to stop an axis on a sensor reading. Split across processes that
// loop would run over DDS -- the server talking to itself, with the stage's
// safety backstop depending on the round trip.
//
// Threading: the executor is multi-threaded, so several service callbacks can be
// in here at once. What makes that safe is that the device classes were already
// built for it -- AxisDevice serialises every call onto the one thread that owns
// the serial ports, MonitorDevice marshals to the GUI thread, CameraDevice's grab
// thread is independent of its readers. `computeMutex` below covers the one thing
// they do not: MoilCali's noise-cleaning flag is process-wide state inside the
// analysis code, so two detect ops running at once would set it for each other.
struct ServerContext {
    // Loaded once at start-up; the devices are constructed from this same copy so
    // they cannot disagree about which ports they were told to open.
    DeviceConfig config;

    std::unique_ptr<AxisDevice> axis;
    std::unique_ptr<CameraDevice> camera;
    std::unique_ptr<MonitorDevice> monitor;

    // The calibration maths. Stateless; shared because everything reaches it.
    Compute compute;

    // The software zero, and the pose to restore at start-up. Lives on the SERVER
    // now: it is a property of this stage, not of whoever is looking at it, and
    // two clients must not each keep their own idea of where zero is.
    AxisOrigin origins;

    // Sessions on this machine's disk.
    std::unique_ptr<SessionStore> sessions;

    // Serialises the analysis ops. See the note above about the process-wide
    // noise-cleaning flag.
    std::mutex computeMutex;

    // Set once the constructor chain has finished, so the supervisor can tell
    // "still starting" from "failed to start".
    bool ready = false;

    // Why `ready` is false. Empty when it is true.
    QString startupError;

    // Wall-clock start, for SystemStatus::uptime.
    qint64 startedAtMs = 0;

    // Build every device and report what each one found, in the same words the
    // v2.0 app printed at start-up -- those lines are the first thing to read
    // when something is wrong, and they should not change just because the app
    // moved machines:
    //
    //   [axis] CRUX serial connect error: COM5 is not found
    //   [camera] open: id=0 mf 3040x3040
    //   [monitor] no saved display mapping -- directions are UNSET
    void start();

    // Ports and devices, for RigInfo and for the console banner.
    QString axisUrl() const;
    QString cameraUrl() const;
    QString monitorUrl() const;
};
