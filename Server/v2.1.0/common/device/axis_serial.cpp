// The serial primitives both controller backends use. What each one is for, and
// why it is shaped the way it is, is in axis_backend.h beside its declaration.

#include "axis_backend.h"

#include <QElapsedTimer>
#include <QSerialPort>
#include <QSerialPortInfo>

namespace axis_detail {

QString zfill(long long value, int width) {
    const bool negative = value < 0;
    QString digits = QString::number(negative ? -value : value);
    const int pad = width - digits.size() - (negative ? 1 : 0);
    if (pad > 0) digits.prepend(QString(pad, QLatin1Char('0')));
    return negative ? QLatin1Char('-') + digits : digits;
}

int yuanmanSpeed(const QString &speed) {
    const QString s = speed.toLower();
    if (s == QLatin1String("high")) return 7;
    if (s == QLatin1String("mid")) return 5;
    return 3;
}

QByteArray readReply(QSerialPort *port, int windowMs) {
    QByteArray reply;
    QElapsedTimer timer;
    timer.start();
    for (;;) {
        const int left = windowMs - static_cast<int>(timer.elapsed());
        if (left <= 0) break;
        if (port->waitForReadyRead(left)) reply += port->readAll();
        if (reply.endsWith('\n')) break;
    }
    return reply;
}

bool openPort(QSerialPort *port, const QString &name, int baud, QString *err) {
    bool present = false;
    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts())
        if (info.portName().compare(name, Qt::CaseInsensitive) == 0) present = true;

    if (!present) {
        *err = QStringLiteral("%1 is not found").arg(name);
        return false;
    }

    port->setPortName(name);
    port->setBaudRate(baud);
    port->setDataBits(QSerialPort::Data8);
    port->setParity(QSerialPort::NoParity);
    port->setStopBits(QSerialPort::OneStop);
    port->setFlowControl(QSerialPort::NoFlowControl);

    if (!port->open(QIODevice::ReadWrite)) {
        // On Windows this is "Access is denied" when something else already holds
        // the port -- most often a second copy of this app, or the old
        // moil_servers.exe / run_axis.bat still running from the ROS setup.
        *err = QStringLiteral("%1 could not be opened: %2").arg(name, port->errorString());
        return false;
    }
    return true;
}

}  // namespace axis_detail
