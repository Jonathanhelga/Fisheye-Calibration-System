#pragma once

// The two rigs, behind one interface, and the serial primitives both use.
//
// This was one 1000-line file: two complete controller drivers, the serial
// helpers they share, and the threading that owns the ports, all in a single
// anonymous namespace. It is four now:
//
//   axis_serial.cpp    zfill, the reply window, opening a port
//   axis_yuanman.cpp   Arduino Mega (X/Y/Z) + KOHZU CRUX (yaw/pitch), 2 ports
//   axis_yinda.cpp     five MCode controllers, one port per axis
//   axis_device.cpp    the serial thread, the shutdown gate, the public API
//
// The two backends never referred to each other and never will: they are
// different firmware on different wire protocols, and only one of them is real
// on any given rig. Keeping them apart means a change to one cannot reach the
// other, which is worth more here than anywhere else in this tree -- these are
// the bytes that move a physical stage, and the rig's own axis_module_*.py had
// exactly this shape for the same reason.
//
// Private to device/. Nothing outside it should include this; axis_device.h is
// the interface.

#include <memory>

#include <QByteArray>
#include <QString>

#include "device_config.h"

class QSerialPort;

namespace axis_detail {

// Python's str.zfill: pad with zeros to `width` TOTAL characters, with the minus
// sign kept in front of the padding ("-1234".zfill(9) == "-00001234"). Writing
// this out matters -- QString::number() with a field width pads on the wrong side
// of the sign, and the controller reads the command positionally.
QString zfill(long long value, int width);

// "high" -> 7, "mid" -> 5, "low" -> 3, anything else -> 3. The final case is not
// a fallback for bad input so much as what the GUI's default speed resolves to.
int yuanmanSpeed(const QString &speed);

// The smallest amount of time a reply is allowed before it is called silence.
//
// Measured on the rig, 8 status reads each: the Arduino's first byte comes back
// in 14.1 ms at best, 20 ms typically, 30 ms at worst; the CRUX answers in 15-18
// ms. Against those numbers arduino_reply_ms is 10, so EVERY X/Y/Z read expired
// before the controller had said anything -- and initSensorStatus() probes
// is_sensor_x_left, an Arduino sensor, so one guaranteed miss produced "The axis
// controller is not answering" and skipped the CRUX sensors that were fine.
//
// The numbers were not wrong where they came from. In axis_module_yuanman.py
// they were pyserial timeouts, which bound a single read inside a loop that kept
// going until it had a line; ported here they became the total budget for the
// whole exchange, and the same 10 stopped meaning "check this often" and started
// meaning "give up this fast".
//
// So the two ideas are separated: this is how long a reply may take, and the
// configured *_reply_ms stays what its comment says it is -- the gap the
// controllers need BETWEEN commands. Raising a timeout costs nothing when the
// device answers, because the read stops at the newline; it costs only when the
// device really is silent, which is exactly when waiting is the right thing.
inline constexpr int kMinReplyTimeoutMs = 300;

// Collect a reply for up to `windowMs`.
//
// The Python slept a fixed window and then read whatever pyserial had buffered.
// This waits the same window but stops as soon as the reply is newline
// terminated. That is not a change to what is parsed -- every reply the firmware
// sends ends in CRLF, and the parsing indexes into the text before it -- it only
// stops the driver from sitting out the rest of a window it has already been
// answered in. Nothing is read that the Python would not have read; some replies
// simply arrive sooner.
QByteArray readReply(QSerialPort *port, int windowMs);

// Open one port, or explain why not. The Python checked the port against the
// enumerated list first and returned a distinct message for "not found" versus
// "could not open"; that distinction is the difference between "the rig is
// unplugged" and "something else is holding it", so it is kept.
bool openPort(QSerialPort *port, const QString &name, int baud, QString *err);

}  // namespace axis_detail

class AxisBackend {
public:
    virtual ~AxisBackend() = default;

    virtual bool open(QString *err) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;
    virtual QString describePorts() const = 0;

    virtual QString home(const QString &axis) = 0;
    virtual QString move(const QString &direction, double distance, const QString &speed) = 0;
    virtual QString stop(const QString &axis) = 0;
    virtual QString readPosition(const QString &axis) = 0;
    virtual QString writePosition(const QString &axis, double value) = 0;
    // "true" / "false" / "" -- "" is the ERROR the UI shows as a grey lamp.
    virtual QString sensor(const QString &name) = 0;
};

// The backends are built through these rather than by name, so the classes
// themselves stay private to their own translation unit.
//
// Whichever one is made, it MUST be made on the serial thread: a QSerialPort's
// overlapped I/O completions are delivered by the event dispatcher of the thread
// the port object lives on. See AxisWorker::init() in axis_device.cpp for what
// happens when it is not.
std::unique_ptr<AxisBackend> makeYuanmanBackend(const DeviceConfig &cfg);
std::unique_ptr<AxisBackend> makeYindaBackend(const DeviceConfig &cfg);
