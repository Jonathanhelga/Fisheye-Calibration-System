// The yinda rig: five independent MCode controllers, one per axis, one port
// each. A port of axis_module_yinda.py; see the note at the top of
// axis_yuanman.cpp about staying byte-identical on the wire.

#include "axis_backend.h"

#include <algorithm>
#include <cmath>

#include <QRegularExpression>
#include <QSerialPort>
#include <QThread>

using namespace axis_detail;

namespace {

// Five independent controllers, one per axis, each on its own port speaking
// MCode. Every motion is three commands: set the velocity, save it, then move.
class YindaBackend : public AxisBackend {
public:
    explicit YindaBackend(const DeviceConfig &cfg) : cfg_(cfg) {}

    bool open(QString *err) override {
        struct Entry {
            QSerialPort *port;
            const QString &name;
            const char *label;
        };
        const Entry entries[] = {
            {&x_, cfg_.xPort, "X"},         {&y_, cfg_.yPort, "Y"},
            {&z_, cfg_.zPort, "Z"},         {&yaw_, cfg_.yawPort, "Yaw"},
            {&pitch_, cfg_.pitchPort, "Pitch"},
        };

        for (const Entry &e : entries) {
            QString portErr;
            if (openPort(e.port, e.name, cfg_.yindaBaud, &portErr)) continue;
            // Same reasoning as yuanman: do not leave the rig half-open.
            close();
            *err = QStringLiteral("%1 serial connect error: %2")
                       .arg(QLatin1String(e.label), portErr);
            return false;
        }
        QThread::msleep(910);
        return true;
    }

    void close() override {
        for (QSerialPort *p : {&x_, &y_, &z_, &yaw_, &pitch_}) p->close();
    }

    bool isOpen() const override {
        return x_.isOpen() && y_.isOpen() && z_.isOpen() && yaw_.isOpen() && pitch_.isOpen();
    }

    QString describePorts() const override {
        return QStringLiteral("yinda: x=%1 y=%2 z=%3 yaw=%4 pitch=%5")
            .arg(cfg_.xPort, cfg_.yPort, cfg_.zPort, cfg_.yawPort, cfg_.pitchPort);
    }

    QString home(const QString &axis) override {
        const QString a = axis.toLower();
        if (!portFor(a)) return unknownAxis(a);
        transact(QStringLiteral("VH=300"), a);
        transact(QStringLiteral("SAVE C"), a);
        return transact(QStringLiteral("H"), a);
    }

    QString move(const QString &direction, double distance, const QString &speed) override {
        // "MR <steps>" is a relative move; the sign is the direction.
        struct Spec {
            const char *direction;
            const char *axis;
            int sign;
        };
        static constexpr Spec kSpecs[] = {
            {"x_left", "x", -1},      {"x_right", "x", +1},
            {"y_up", "y", +1},        {"y_down", "y", -1},
            {"z_forward", "z", +1},   {"z_back", "z", -1},
            {"yaw_left", "yaw", +1},  {"yaw_right", "yaw", -1},
            {"pitch_down", "pitch", +1}, {"pitch_up", "pitch", -1},
        };

        for (const Spec &s : kSpecs) {
            if (direction != QLatin1String(s.direction)) continue;
            const QString axis = QLatin1String(s.axis);

            transact(QStringLiteral("VM=%1").arg(velocity(axis, speed)), axis);
            transact(QStringLiteral("SAVE C"), axis);
            const QString amount = QString::number(s.sign * distance, 'g', 10);
            return transact(QStringLiteral("MR %1").arg(amount), axis);
        }
        return QStringLiteral("unknown direction \"%1\"").arg(direction);
    }

    QString stop(const QString &axis) override {
        const QString a = axis.toLower();
        if (!portFor(a)) return unknownAxis(a);
        return transact(QStringLiteral("STOP"), a);
    }

    QString readPosition(const QString &axis) override {
        const QString a = axis.toLower();
        if (!portFor(a)) return unknownAxis(a);

        QString err;
        const QByteArray reply = send(QStringLiteral("PR P"), a, &err);
        if (!err.isEmpty()) return err;

        // The "PR P" echo carries no digits, so the last number in the reply is
        // the position counter.
        static const QRegularExpression number(QStringLiteral("-?\\d+"));
        QString last;
        auto it = number.globalMatch(QString::fromLatin1(reply));
        while (it.hasNext()) last = it.next().captured();
        return last;
    }

    QString writePosition(const QString &axis, double value) override {
        const QString a = axis.toLower();
        if (!portFor(a)) return unknownAxis(a);
        return transact(QStringLiteral("P=%1").arg(static_cast<long long>(value)), a);
    }

    QString sensor(const QString &name) override {
        // uiName -> (axis, query, the character that means "on"). ?IN1/2/3 are the
        // three opto inputs and ?ST is the status word whose 'D' means "moving".
        struct Spec {
            const char *ui;
            const char *axis;
            const char *query;
            char on;
        };
        static constexpr Spec kSpecs[] = {
            {"x_left", "x", "?IN3", '1'},       {"x_org", "x", "?IN1", '1'},
            {"x_right", "x", "?IN2", '1'},      {"x_move", "x", "?ST", 'D'},
            {"y_down", "y", "?IN3", '1'},       {"y_org", "y", "?IN1", '1'},
            {"y_up", "y", "?IN2", '1'},         {"y_move", "y", "?ST", 'D'},
            {"z_back", "z", "?IN1", '1'},       {"z_forward", "z", "?IN2", '1'},
            {"z_move", "z", "?ST", 'D'},        {"yaw_left", "yaw", "?IN3", '1'},
            {"yaw_org", "yaw", "?IN1", '1'},    {"yaw_right", "yaw", "?IN2", '1'},
            {"yaw_move", "yaw", "?ST", 'D'},    {"pitch_down", "pitch", "?IN2", '1'},
            {"pitch_org", "pitch", "?IN1", '1'},{"pitch_up", "pitch", "?IN3", '1'},
            {"pitch_move", "pitch", "?ST", 'D'},
        };

        // This rig has no Z origin switch wired, and the Python returned a plain
        // False rather than reading anything. Answering "false" (not "") keeps the
        // lamp dark instead of grey: there is no fault, there is no switch.
        if (name == QLatin1String("z_org")) return QStringLiteral("false");

        for (const Spec &s : kSpecs) {
            if (name != QLatin1String(s.ui)) continue;

            QString err;
            const QByteArray reply =
                send(QLatin1String(s.query), QLatin1String(s.axis), &err);
            if (!err.isEmpty()) return QString();

            // The flag is the third character from the end, before the CRLF.
            const QString text = QString::fromLatin1(reply);
            if (text.size() < 3) return QString();
            return text.at(text.size() - 3) == QLatin1Char(s.on) ? QStringLiteral("true")
                                                                 : QStringLiteral("false");
        }
        return QString();
    }

private:
    static QString unknownAxis(const QString &axis) {
        return QStringLiteral("unknown axis \"%1\"").arg(axis);
    }

    // Z is geared differently from the other four and gets its own velocities.
    // The Python looked these up with capitalised keys ('Low'/'Mid'/'High') while
    // the GUI sends lowercase, so every yinda move raised KeyError before it
    // reached the controller. Matched case-insensitively here.
    static int velocity(const QString &axis, const QString &speed) {
        const QString s = speed.toLower();
        if (axis == QLatin1String("z")) {
            if (s == QLatin1String("high")) return 2500;
            if (s == QLatin1String("mid")) return 1500;
            return 500;
        }
        if (s == QLatin1String("high")) return 500;
        if (s == QLatin1String("mid")) return 300;
        return 100;
    }

    QSerialPort *portFor(const QString &axis) {
        if (axis == QLatin1String("x")) return &x_;
        if (axis == QLatin1String("y")) return &y_;
        if (axis == QLatin1String("z")) return &z_;
        if (axis == QLatin1String("yaw")) return &yaw_;
        if (axis == QLatin1String("pitch")) return &pitch_;
        return nullptr;
    }

    QString transact(const QString &command, const QString &axis) {
        QString err;
        const QByteArray reply = send(command, axis, &err);
        if (!err.isEmpty()) return err;
        return QString::fromLatin1(reply).trimmed();
    }

    QByteArray send(const QString &command, const QString &axis, QString *err) {
        if (!isOpen()) {
            *err = QStringLiteral("serial port is not open");
            return {};
        }
        QSerialPort *port = portFor(axis);
        if (!port) {
            *err = unknownAxis(axis);
            return {};
        }

        QThread::msleep(100);

        const QByteArray frame = command.toUtf8() + "\r\n";
        port->clear(QSerialPort::Input);
        if (port->write(frame) != frame.size() || !port->waitForBytesWritten(1000)) {
            *err = QStringLiteral("write to %1 failed: %2").arg(port->portName(),
                                                                port->errorString());
            return {};
        }

        // Same split as the yuanman path: a timeout that lets the controller
        // answer, and a fixed 100 ms gap before the next command. This rig is
        // yuanman so the number here has never been measured against real
        // hardware -- which is the reason to give it room rather than the reason
        // to leave it tight.
        const QByteArray reply = readReply(port, std::max(100, kMinReplyTimeoutMs));
        QThread::msleep(100);
        return reply;
    }

    DeviceConfig cfg_;
    QSerialPort x_, y_, z_, yaw_, pitch_;
};

}  // namespace

std::unique_ptr<AxisBackend> makeYindaBackend(const DeviceConfig &cfg) {
    return std::make_unique<YindaBackend>(cfg);
}
