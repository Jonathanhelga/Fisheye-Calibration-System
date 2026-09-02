#pragma once

#include <QHash>
#include <QString>

// Where each axis' zero is, in raw controller counts, and where the stage was
// standing when the app last closed.
//
// The controllers keep a position counter and this app can read it, but nothing
// ever set that counter to zero. WRP -- the command that would -- is off by
// default and has never been sent by any rig in this system's history (see
// DeviceConfig::enableWritePosition), so a HOME left the counter wherever it
// happened to be while the coordinate field merely *displayed* 0.000 until the
// next poll overwrote it with the raw count again.
//
// So the zero lives here instead, in software. HOME records the counter it
// lands on and every reading afterwards is reported relative to it. Nothing new
// is sent to the hardware, which is the point: the alternative was to start
// exercising a command no rig here has ever received.
//
// Persisted next to devices.json so the zero survives a restart -- a counter
// that has not lost power is still counting from where it was yesterday.
//
// THE ONE THING THIS CANNOT KNOW is whether the controller lost power while the
// app was closed. If it did, the counter restarted at its own zero and a stored
// origin is meaningless. Nothing in the protocol distinguishes that from a
// stage that simply has not moved, so this does not guess: it keeps the
// timestamp of when each zero was taken, reports the restored pose as
// provisional, and leaves re-homing to the operator. A wrong zero presented
// confidently is worse than a stale one presented honestly.
//
// Threading: plain data, no locking. ControllerMain touches it on the GUI
// thread only -- the monitor workers read the counter and hand the raw text
// back before any of this is consulted.
struct AxisOrigin {
    // axis name ("x".."pitch") -> the raw counter that axis last homed on.
    // Absent means "never homed this run and none was restored": readings are
    // then reported against a zero of 0, which is the raw counter itself.
    QHash<QString, double> zeroCount;

    // axis -> ISO-8601 local time the zero above was taken. Same keys as
    // zeroCount; kept beside it so an operator can see how old a zero is.
    QHash<QString, QString> zeroTakenAt;

    // axis -> last displayed position in user units. Restored at launch so the
    // window opens showing where the stage was left rather than "?".
    QHash<QString, double> lastPos;

    // True when the values above came off disk rather than from a HOME in this
    // run, i.e. they carry the power-cycle caveat above.
    bool restored = false;

    // Whether this axis has a zero to measure against.
    bool hasZero(const QString &axis) const;

    // The zero for an axis, or 0.0 when it has none -- which reports the raw
    // counter unshifted, the same thing the display did before any of this.
    double zeroFor(const QString &axis) const;

    // Record `rawCount` as this axis' zero, stamped now.
    void setZero(const QString &axis, double rawCount);

    // Forget every zero and pose. Used when the axis ports are reopened, since
    // a reconnect is the one moment a power cycle is likely to have happened.
    void clear();

    // Never fails: a missing or unreadable file yields an empty origin, which
    // means "nothing is homed yet" and is the correct state for a fresh install.
    static AxisOrigin load();

    // Write to the per-user file. False (with the reason on stderr) if it
    // cannot be written.
    bool save() const;

    // Per-user, writable, beside devices.json:
    // %LOCALAPPDATA%/MoilLab/FisheyeCalisys/positions.json
    static QString userPath();
};
