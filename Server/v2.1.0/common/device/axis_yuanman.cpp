// The yuanman rig: an Arduino Mega for X/Y/Z and a KOHZU CRUX for yaw/pitch, on
// two ports. A port of axis_module_yuanman.py -- where the Python did something
// surprising the comment says so and says which way this one went; everything
// else is deliberately the same bytes on the wire, because the firmware on the
// other end was not ported and does not know that anything changed.

#include "axis_backend.h"

#include <algorithm>
#include <cmath>
#include <iostream>

#include <QRegularExpression>
#include <QSerialPort>
#include <QThread>

using namespace axis_detail;

namespace {

// Arduino Mega drives X/Y/Z, a KOHZU CRUX drives yaw/pitch, and both speak the
// same framed command set: STX, ascii body, CRLF. Which port a command goes to is
// decided by the axis number that is the FOURTH character of the body -- 1 and 2
// are the CRUX, 3 to 6 the Arduino -- which is why the routing below reads a
// character out of the middle of a string instead of taking the axis as an
// argument. That is the protocol, not a shortcut.
class YuanmanBackend : public AxisBackend {
public:
    explicit YuanmanBackend(const DeviceConfig &cfg) : cfg_(cfg) {}

    bool open(QString *err) override {
        QString e;
        if (!openPort(&crux_, cfg_.cruxPort, cfg_.cruxBaud, &e)) {
            *err = QStringLiteral("CRUX serial connect error: %1").arg(e);
            return false;
        }
        if (!openPort(&arduino_, cfg_.arduinoPort, cfg_.arduinoBaud, &e)) {
            // Release the one that DID open. A half-open rig is the state the
            // Python's _verify_ports_open() existed to refuse: every later
            // command fails with "Serial Port is not open" while the caller
            // believes the hardware is ready.
            crux_.close();
            *err = QStringLiteral("ARDUINO serial connect error: %1").arg(e);
            return false;
        }
        // The Python slept 0.1 s + 0.81 s after opening. The Arduino resets when
        // its port is opened (DTR), and commands sent during the bootloader
        // window are swallowed, so this wait is load-bearing.
        QThread::msleep(910);
        return true;
    }

    void close() override {
        arduino_.close();
        crux_.close();
    }

    bool isOpen() const override { return arduino_.isOpen() && crux_.isOpen(); }

    QString describePorts() const override {
        return QStringLiteral("yuanman: arduino=%1 crux=%2").arg(cfg_.arduinoPort, cfg_.cruxPort);
    }

    QString home(const QString &axis) override {
        const QString a = axis.toLower();
        const QString n = axisNumber(a);
        if (n.isEmpty()) return unknownAxis(a);
        QThread::msleep(50);
        // yaw/pitch home at speed 7, the linear axes at 8. Same two forms the
        // Python emitted; the trailing /0 is the response mode ("completed").
        const QString cmd = isRotary(a) ? QStringLiteral("ORG%1/7/0").arg(n)
                                        : QStringLiteral("ORG%1/8/0").arg(n);
        return transact(cmd);
    }

    QString move(const QString &direction, double distance, const QString &speed) override {
        const int sp = yuanmanSpeed(speed);

        // Steps per millimetre, and which way round the sign goes. These four
        // constants are the rig's gearing; they are the reason a "10 mm" button
        // moves 10 mm. The doubling on the linear axes is in the Python too --
        // the Arduino counts half-steps.
        if (direction == QLatin1String("x_left"))
            return rps(QStringLiteral("x"), sp, linearSteps(distance, 0.002));
        if (direction == QLatin1String("x_right"))
            return rps(QStringLiteral("x"), sp, -linearSteps(distance, 0.002));
        if (direction == QLatin1String("y_up"))
            return rps(QStringLiteral("y"), sp, -linearSteps(distance, 0.002));
        if (direction == QLatin1String("y_down"))
            return rps(QStringLiteral("y"), sp, linearSteps(distance, 0.002));
        if (direction == QLatin1String("z_forward"))
            return rps(QStringLiteral("z"), sp, -linearSteps(distance, 0.005));
        if (direction == QLatin1String("z_back"))
            return rps(QStringLiteral("z"), sp, linearSteps(distance, 0.005));
        if (direction == QLatin1String("yaw_left"))
            return rps(QStringLiteral("yaw"), sp, rotarySteps(distance, 0.00067));
        if (direction == QLatin1String("yaw_right"))
            return rps(QStringLiteral("yaw"), sp, -rotarySteps(distance, 0.00067));
        if (direction == QLatin1String("pitch_up"))
            return rps(QStringLiteral("pitch"), sp, -rotarySteps(distance, 0.00084));
        if (direction == QLatin1String("pitch_down"))
            return rps(QStringLiteral("pitch"), sp, rotarySteps(distance, 0.00084));

        return QStringLiteral("unknown direction \"%1\"").arg(direction);
    }

    QString stop(const QString &axis) override {
        const QString a = axis.toLower();
        const QString n = axisNumber(a);
        if (n.isEmpty()) return unknownAxis(a);
        return transact(QStringLiteral("STP%1/0").arg(n));
    }

    QString readPosition(const QString &axis) override {
        const QString a = axis.toLower();
        const QString n = axisNumber(a);
        if (n.isEmpty()) return unknownAxis(a);
        QThread::msleep(50);

        QString err;
        const QByteArray reply = send(QStringLiteral("RDP%1").arg(n), &err);
        if (!err.isEmpty()) return err;

        // The controller echoes a fixed-width header before the number, and the
        // two controllers use different header lengths.
        const QString text = QString::fromLatin1(reply);
        const int skip = isRotary(a) ? 7 : 6;
        if (text.size() <= skip) return QString();
        return text.mid(skip).trimmed();
    }

    QString writePosition(const QString &axis, double value) override {
        const QString a = axis.toLower();
        const QString n = axisNumber(a);
        if (n.isEmpty()) return unknownAxis(a);
        QThread::msleep(50);

        const long long amount = static_cast<long long>(value);

        // The Python's guards here were both written as `if LOW < amount or
        // amount < HIGH`, which is true for every possible amount, so WRP always
        // returned "amount over limit" and the command was never sent. These are
        // the guards those two lines were reaching for. See
        // DeviceConfig::enableWritePosition for why sending it at all is opt-in.
        if (isRotary(a)) {
            if (amount < -8388607 || amount > 8388607)
                return QStringLiteral("amount over limit");
            return transact(QStringLiteral("WRP%1/%2").arg(n, QString::number(amount)));
        }
        if (amount < -9800000 || amount > 9800000) return QStringLiteral("amount over limit");
        return transact(
            QStringLiteral("WRP%1/%2").arg(n, zfill(amount, amount >= 0 ? 8 : 9)));
    }

    QString sensor(const QString &name) override {
        // uiName -> (status field, the character that means "on"). The polarity
        // differs between the linear axes and the rotary ones: the Arduino
        // reports its limit switches active-low and the CRUX active-high. Getting
        // one of these backwards shows an operator a lamp that is on when the
        // stage is nowhere near the limit, so the table is written out in full
        // rather than derived.
        struct Spec {
            const char *ui;
            const char *field;
            char on;
        };
        static constexpr Spec kSpecs[] = {
            {"x_left", "x_cw", '0'},        {"x_org", "x_org", '0'},
            {"x_right", "x_ccw", '0'},      {"x_move", "x", '1'},
            {"y_down", "y_cw", '0'},        {"y_org", "y_org", '0'},
            {"y_up", "y_ccw", '0'},         {"y_move", "y", '1'},
            {"z_back", "z_cw", '0'},        {"z_org", "z_org", '0'},
            {"z_forward", "z_ccw", '0'},    {"z_move", "z", '1'},
            {"yaw_left", "yaw_cw", '1'},    {"yaw_org", "yaw_org", '1'},
            {"yaw_right", "yaw_ccw", '1'},  {"yaw_move", "yaw", '1'},
            {"pitch_down", "pitch_cw", '1'},{"pitch_org", "pitch_org", '1'},
            {"pitch_up", "pitch_ccw", '1'}, {"pitch_move", "pitch", '1'},
        };

        for (const Spec &s : kSpecs) {
            if (name != QLatin1String(s.ui)) continue;
            QChar value;
            if (!status(QLatin1String(s.field), &value)) return QString();
            return value == QLatin1Char(s.on) ? QStringLiteral("true")
                                              : QStringLiteral("false");
        }
        return QString();
    }

private:
    static bool isRotary(const QString &axis) {
        return axis == QLatin1String("yaw") || axis == QLatin1String("pitch");
    }

    static QString axisNumber(const QString &axis) {
        if (axis == QLatin1String("pitch")) return QStringLiteral("1");
        if (axis == QLatin1String("yaw")) return QStringLiteral("2");
        if (axis == QLatin1String("x")) return QStringLiteral("3");
        if (axis == QLatin1String("y")) return QStringLiteral("4");
        if (axis == QLatin1String("z")) return QStringLiteral("5");
        return QString();
    }

    static QString unknownAxis(const QString &axis) {
        return QStringLiteral("unknown axis \"%1\"").arg(axis);
    }

    // int() in Python truncates toward zero; distance is a positive millimetre
    // figure from the GUI, so this is the same value.
    static long long linearSteps(double distance, double perStep) {
        return static_cast<long long>(distance / perStep) * 2;
    }
    static long long rotarySteps(double distance, double perStep) {
        return static_cast<long long>(distance / perStep);
    }

    // RPS: relative position drive.
    QString rps(const QString &axis, int speed, long long amount) {
        QThread::msleep(50);
        const QString n = axisNumber(axis);

        if (isRotary(axis)) {
            if (amount < -16777215 || amount > 16777215)
                return QStringLiteral("amount over limit");
            return transact(QStringLiteral("RPS%1/%2/%3/0")
                                .arg(n, QString::number(speed), QString::number(amount)));
        }
        if (amount < -195000 || amount > 195000) return QStringLiteral("amount over limit");
        return transact(QStringLiteral("RPS%1/%2/%3/0")
                            .arg(n, QString::number(speed), zfill(amount, amount >= 0 ? 8 : 9)));
    }

    // STR: read status. One command returns every flag for a controller, so the
    // field wanted is picked out by position.
    bool status(const QString &field, QChar *out) {
        QThread::msleep(50);

        QString command;
        int index = -1;

        if (field.startsWith(QLatin1String("pitch"))) {
            command = QStringLiteral("STR1");
            if (field == QLatin1String("pitch")) index = 7;
            else if (field == QLatin1String("pitch_org")) index = 11;
            else if (field == QLatin1String("pitch_ccw")) index = 13;
            else if (field == QLatin1String("pitch_cw")) index = 15;
        } else if (field.startsWith(QLatin1String("yaw"))) {
            command = QStringLiteral("STR2");
            if (field == QLatin1String("yaw")) index = 7;
            else if (field == QLatin1String("yaw_org")) index = 11;
            else if (field == QLatin1String("yaw_ccw")) index = 13;
            else if (field == QLatin1String("yaw_cw")) index = 15;
        } else {
            command = QStringLiteral("STR6");
            if (field == QLatin1String("x")) index = 7;
            else if (field == QLatin1String("y")) index = 8;
            else if (field == QLatin1String("z")) index = 9;
            else if (field == QLatin1String("x_cw")) index = 10;
            else if (field == QLatin1String("x_org")) index = 11;
            else if (field == QLatin1String("x_ccw")) index = 12;
            else if (field == QLatin1String("y_cw")) index = 13;
            else if (field == QLatin1String("y_org")) index = 14;
            else if (field == QLatin1String("y_ccw")) index = 15;
            else if (field == QLatin1String("z_cw")) index = 16;
            else if (field == QLatin1String("z_org")) index = 17;
            else if (field == QLatin1String("z_ccw")) index = 18;
        }
        if (index < 0) {
            std::cerr << "[axis] status: field \"" << qUtf8Printable(field)
                      << "\" has no index\n";
            return false;
        }

        QString err;
        const QByteArray reply = send(command, &err);
        if (!err.isEmpty()) {
            std::cerr << "[axis] " << qUtf8Printable(command) << " failed: "
                      << qUtf8Printable(err) << "\n";
            return false;
        }

        const QString text = QString::fromLatin1(reply);
        // A short reply means the window closed before the controller finished
        // answering. Reporting it as a sensor value would invent a reading, so it
        // is an error -- the UI shows the lamp grey, which is the truth.
        //
        // Said out loud, because the caller turns this into "the axis controller
        // is not answering" and that sentence sent an operator hunting for a dead
        // stage when the stage was answering perfectly and the reply was simply
        // one character shorter than the index being read out of it.
        if (text.size() <= index) {
            QString shown = text;
            shown.replace(QLatin1Char('\t'), QLatin1String("<TAB>"));
            shown.replace(QLatin1Char('\r'), QLatin1String("<CR>"));
            shown.replace(QLatin1Char('\n'), QLatin1String("<LF>"));
            std::cerr << "[axis] " << qUtf8Printable(command) << " -> " << text.size()
                      << " chars, need index " << index << ": \"" << qUtf8Printable(shown)
                      << "\"\n";
            return false;
        }

        *out = text.at(index);
        return true;
    }

    // Send and return the reply as the message the caller shows, mirroring what
    // the /axis service put in `message`.
    QString transact(const QString &command) {
        QString err;
        const QByteArray reply = send(command, &err);
        if (!err.isEmpty()) return err;
        return QString::fromLatin1(reply).trimmed();
    }

    // The wire format: STX, the ascii body, CRLF.
    QByteArray send(const QString &command, QString *err) {
        if (!isOpen()) {
            *err = QStringLiteral("serial port is not open");
            return {};
        }

        // Routing is by the axis number inside the body, which is its fourth
        // character ("RPS5/..." -> '5').
        if (command.size() < 4) {
            *err = QStringLiteral("malformed command \"%1\"").arg(command);
            return {};
        }
        const QChar axisNum = command.at(3);
        QSerialPort *port = nullptr;
        int window = 0;
        if (axisNum == QLatin1Char('1') || axisNum == QLatin1Char('2')) {
            port = &crux_;
            window = cfg_.cruxReplyMs;
        } else if (axisNum >= QLatin1Char('3') && axisNum <= QLatin1Char('6')) {
            port = &arduino_;
            window = cfg_.arduinoReplyMs;
        } else {
            *err = QStringLiteral("axis number \"%1\" is not in 1..6").arg(axisNum);
            return {};
        }

        QByteArray frame;
        frame += '\x02';
        frame += command.toUtf8();
        frame += "\r\n";

        port->clear(QSerialPort::Input);
        if (port->write(frame) != frame.size() || !port->waitForBytesWritten(1000)) {
            *err = QStringLiteral("write to %1 failed: %2").arg(port->portName(),
                                                                port->errorString());
            return {};
        }

        // Waiting and pacing are different things -- see kMinReplyTimeoutMs. The
        // read gets a timeout long enough for the controller to actually answer;
        // the sleep afterwards keeps the configured gap between commands.
        const QByteArray reply = readReply(port, std::max(window, kMinReplyTimeoutMs));
        // The Python slept the same window again after reading. It is the gap the
        // controllers need between commands, not an artefact of the read.
        QThread::msleep(static_cast<unsigned long>(window));
        return reply;
    }

    DeviceConfig cfg_;
    QSerialPort arduino_;
    QSerialPort crux_;
};

}  // namespace

std::unique_ptr<AxisBackend> makeYuanmanBackend(const DeviceConfig &cfg) {
    return std::make_unique<YuanmanBackend>(cfg);
}
