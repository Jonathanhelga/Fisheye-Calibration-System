#pragma once

#include <QString>

// Everything the app needs to know about the hardware wired to THIS machine.
//
// This replaces the three ROS parameter files -- moil_camera.yaml, moil_axis.yaml
// and moil_monitor.yaml -- that used to be loaded by three separate nodes. There
// is one process now, so there is one file: config/devices.json next to the
// binary. Missing file, missing key and unparsable file all mean "use the default
// below", because a rig that has never been configured must still start: the
// operator needs the window in order to be told what is wrong.
//
// The defaults are the values the rig was actually running with, taken from the
// modules themselves rather than from the YAML comments -- see the notes on
// arduinoPort/cruxPort, where the two disagreed.
struct DeviceConfig {
    // ---- axis ---------------------------------------------------------------
    // "yuanman" - Arduino Mega (X/Y/Z) + KOHZU CRUX (yaw/pitch), 2 COM ports
    // "yinda"   - 5 COM ports, one per axis
    QString axisSystem = QStringLiteral("yuanman");

    // yuanman. NOTE: axis_module_yuanman.py's own defaults are COM4 for the
    // Arduino and COM5 for the CRUX, and those are what the FastAPI server and
    // then the ROS node instantiated. moil_axis.yaml's comment claims COM3 =
    // Arduino / COM4 = CRUX, which contradicts the code that ran. The code wins;
    // if this rig enumerates differently, set it here.
    QString arduinoPort = QStringLiteral("COM4");
    int arduinoBaud = 115200;
    QString cruxPort = QStringLiteral("COM5");
    int cruxBaud = 9600;

    // yinda: one port per axis.
    QString yawPort = QStringLiteral("COM1");
    QString pitchPort = QStringLiteral("COM2");
    QString xPort = QStringLiteral("COM3");
    QString yPort = QStringLiteral("COM4");
    QString zPort = QStringLiteral("COM5");
    int yindaBaud = 9600;

    // How long to keep collecting a serial reply before giving up on it. The
    // Python driver slept a fixed 100 ms (CRUX) / 10 ms (Arduino) and then read
    // whatever pyserial had buffered; these are the same two windows, except that
    // a reply ending in a newline is taken as complete and does not wait out the
    // rest. See AxisDevice for why that is a fix and not a behaviour change.
    int cruxReplyMs = 100;
    int arduinoReplyMs = 10;

    // WRP ("write present position") zeroes the axis counter after a home. The
    // ROS client made it a no-op, and the Python driver's range guards were
    // inverted so it never sent the command either -- meaning NO rig in this
    // system's history has actually received a WRP. Enabling it therefore moves
    // real hardware in a way that has never been exercised, so it is off unless
    // asked for. read_position has no such risk and is always on.
    bool enableWritePosition = false;

    // User units -- millimetres on x/y/z, degrees on yaw/pitch -- per one count
    // of the controller's position counter. SIGNED: the sign carries which way
    // the counter runs against the button that commands the move, so that
    // "right / up / forward" reads positive on the coordinate display.
    //
    // Nothing applied a conversion here before: read_position returned the raw
    // counter and the coordinate field printed it verbatim, so the readout was
    // in whatever the controller counts in while the spin boxes beside it were
    // in mm and degrees. These defaults are the inverse of the gearing in
    // AxisDevice::move(), and they differ per rig because the two backends do
    // not command in the same units:
    //
    //   yuanman  moves are sent in STEPS (mm / 0.002 * 2 on x/y, / 0.005 * 2 on
    //            z, deg / 0.00067 yaw, / 0.00084 pitch) and RDP reads that same
    //            counter back, so one count is one step.
    //   yinda    "MR" is handed the distance in user units and "PR P" reads it
    //            back the same way, so one count is one unit: +-1.
    //
    // The MAGNITUDES come from the gearing and are as good as the gearing is.
    // The SIGNS assume the counter follows the sign of the step amount it was
    // given, which is the obvious behaviour but is not something any rig here
    // has confirmed -- there was no conversion to be right or wrong before. If
    // an axis reads backwards or off by a constant factor, correct it here: it
    // is a config edit, not a rebuild.
    double unitsPerCountX = -0.001;
    double unitsPerCountY = -0.001;
    double unitsPerCountZ = -0.0025;
    double unitsPerCountYaw = -0.00067;
    double unitsPerCountPitch = -0.00084;

    // The above for one axis name ("x".."pitch"), or 0.0 for anything else.
    // Zero is the honest answer for an unknown axis: it has no gearing here.
    double unitsPerCount(const QString &axis) const;

    // How far a "drive to that end" button asks the axis to travel, in the same
    // user units as above. The move is deliberately LONGER than the axis can
    // actually go: it is the controller's limit switch that ends it, and asking
    // for more than the remaining travel is what guarantees the switch is the
    // thing that stops it rather than the request running out.
    //
    // So these are not travel measurements and do not need to match the rig.
    // Too large is harmless while the firmware honours its limits; too small
    // just stops the axis short of the end, which is visible and fixable here.
    // The ceilings are the protocol's own: AxisDevice's RPS refuses more than
    // 195000 linear steps (195 mm on x/y, 487 mm on z) or 16777215 rotary
    // counts, and answers "amount over limit" without moving.
    // Each is at or near its protocol ceiling on purpose. A value that merely
    // looks plausible for the rig is the wrong kind of guess here: too small
    // and the button stops the axis mid-travel, which is indistinguishable
    // from the machine having reached its end. Z started at 200, which was
    // shorter than this rig's Z travel and did exactly that.
    double travelToLimitX = 195.0;    // 195000 steps, the linear ceiling
    double travelToLimitY = 195.0;
    double travelToLimitZ = 480.0;    // ceiling is 487.5 at 0.005 mm/half-step
    double travelToLimitYaw = 360.0;  // a full turn; ceiling is ~11240 deg
    double travelToLimitPitch = 360.0;

    // The above for one axis name, or 0.0 for anything else -- which commands
    // no motion, the safe answer for an axis this does not know.
    double travelToLimit(const QString &axis) const;

    // ---- camera -------------------------------------------------------------
    int cameraId = 0;
    // "msmf" measured 21.3 fps at 3040x3040 against dshow's 1.1 (dshow pins the
    // stream to uncompressed YUY2). Do not change without re-measuring. Ignored
    // off Windows, where the default backend is used.
    QString cameraBackend = QStringLiteral("msmf");
    int imageWidth = 3040;   // square fisheye mode of the IMX577
    int imageHeight = 3040;

    // How captures are handed to the rest of the app.
    //
    // In-process there is no transport to pay for, so lossless ("png") is now
    // possible -- and it is NOT the default. The camera node published JPEG q=80
    // and every detection threshold in this system was tuned against frames that
    // had been through that encoder. Changing what the detectors see is a
    // calibration change disguised as a cleanup, so the default reproduces the
    // old images and "png" is there for whoever re-tunes.
    //
    // It is also the faster of the two: a 3040x3040 PNG costs on the order of a
    // second to encode, against tens of milliseconds for the JPEG.
    QString captureFormat = QStringLiteral("jpeg");
    int jpegQuality = 80;

    // ---- monitor ------------------------------------------------------------
    // Direction -> display number ("1".."16", or "DISPLAY1"). Empty means the
    // operator has not run the mapping dialog yet, and patterns are refused with
    // that as the reason. Persisted by MonitorDevice when the dialog applies a
    // mapping, exactly as the monitor node persisted it.
    QString displayTop;
    QString displayN;
    QString displayW;
    QString displayS;
    QString displayE;

    // Load the rig description. Never fails: anything unreadable yields the
    // defaults above, with the reason on stderr.
    //
    // Two locations, and the split matters on Windows. The app is installed under
    // Program Files, which a normal user cannot write to, so the copy next to the
    // binary can only ever be a shipped template. Anything the app itself saves --
    // the display mapping, which the operator sets from a dialog -- has to go
    // somewhere per-user, or applying the mapping silently fails and the operator
    // is asked to redo it at every launch.
    static DeviceConfig load();

    // Write the current values to the per-user file. Returns false (and says why
    // on stderr) if it cannot be written.
    bool save() const;

    // Where load() actually read from, for messages.
    static QString path();

    // Per-user, writable: %LOCALAPPDATA%/MoilLab/FisheyeCalisys/devices.json.
    // What save() writes and what load() prefers.
    static QString userPath();

    // Next to the binary: the shipped template, read-only in a real install.
    static QString bundledPath();
};
