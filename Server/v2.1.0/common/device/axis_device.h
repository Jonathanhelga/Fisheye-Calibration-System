#pragma once

#include <memory>

#include <QString>

#include "device_config.h"

// The motion stage, driven over its serial ports from THIS process.
//
// This is what used to be three hops: the GUI called AxisRosClient, which called
// /axis/* on ROS_DOMAIN_ID 42, which reached axis_node.py, which called
// axis_module_yuanman.py, which wrote to the COM ports. The middle three are
// gone. The public API is deliberately the one AxisRosClient had, so the
// controllers did not have to be rewritten to lose them -- every method still
// returns the same kind of string the service reply carried, and the sensors
// still answer "true" / "false" / "" for ERROR.
//
// Two rigs are supported, chosen by DeviceConfig::axisSystem:
//
//   yuanman  Arduino Mega (X/Y/Z) + KOHZU CRUX (yaw/pitch). Two ports. Framed
//            commands: STX + ascii + CRLF, routed to one port or the other by
//            the axis number inside the command.
//   yinda    Five ports, one per axis, speaking MCode ("MR", "VM=", "?IN1").
//
// Threading: the controllers call this from QtConcurrent workers AND from the
// GUI thread. QSerialPort belongs to whichever thread created it, so all serial
// I/O happens on one worker thread this class owns, and every public method is a
// blocking round trip to it. That also serialises the port the way the old
// mutex-per-call did -- two motion commands interleaved on one Arduino is not a
// thing the firmware survives.
class AxisDevice {
public:
    explicit AxisDevice(DeviceConfig cfg = DeviceConfig::load());
    ~AxisDevice();

    AxisDevice(const AxisDevice &) = delete;
    AxisDevice &operator=(const AxisDevice &) = delete;

    // Whether every serial port opened. False here is the honest version of what
    // the ROS client could only report as "service not available".
    bool isOpen() const;

    // Why isOpen() is false, ready to show an operator. Empty when it is true.
    QString lastError() const;

    // Close and reopen the ports. This is what /axis/command "reconnect" did, and
    // it is the only recovery from a rig that was powered on after the app.
    QString reconnect();

    // ---- motion (snake_case, 1:1 with the old client) ----
    QString home(const QString &axis);
    QString x_left(double distance, const QString &speed);
    QString x_right(double distance, const QString &speed);
    QString y_up(double distance, const QString &speed);
    QString y_down(double distance, const QString &speed);
    QString z_forward(double distance, const QString &speed);
    QString z_back(double distance, const QString &speed);
    QString yaw_left(double distance, const QString &speed);
    QString yaw_right(double distance, const QString &speed);
    QString pitch_up(double distance, const QString &speed);
    QString pitch_down(double distance, const QString &speed);
    QString stop(const QString &axis);

    // Reads the position counter off the controller. Under ROS this returned ""
    // because no such service existed, and the UI kept showing its last value;
    // in-process the command is right there, so the read-out is live again.
    QString read_position(const QString &axis);

    // Zeroes the position counter. Sends nothing unless
    // DeviceConfig::enableWritePosition is set -- see the note there: no rig in
    // this system's history has actually received this command, so turning it on
    // is a change to hardware behaviour, not a restoration of it.
    QString write_position(const QString &axis, double position);

    // Millimetres (x/y/z) or degrees (yaw/pitch) per one count of what
    // read_position returns, signed so that right/up/forward is positive. The
    // counter is raw: without this, the coordinate display is in whatever the
    // controller counts in. See DeviceConfig::unitsPerCountX, including which
    // half of this is measured gearing and which half is an assumption.
    double unitsPerCount(const QString &axis) const;

    // How far to ask this axis to travel when driving it to one of its ends.
    // Longer than the axis can go, on purpose -- the limit switch is what ends
    // the move. See DeviceConfig::travelToLimitX.
    double travelToLimit(const QString &axis) const;

    // ---- sensors (all 20) ----
    QString is_sensor_x_left();
    QString is_sensor_x_org();
    QString is_sensor_x_right();
    QString is_sensor_x_move();
    QString is_sensor_y_down();
    QString is_sensor_y_org();
    QString is_sensor_y_up();
    QString is_sensor_y_move();
    QString is_sensor_z_back();
    QString is_sensor_z_org();
    QString is_sensor_z_forward();
    QString is_sensor_z_move();
    QString is_sensor_yaw_left();
    QString is_sensor_yaw_org();
    QString is_sensor_yaw_right();
    QString is_sensor_yaw_move();
    QString is_sensor_pitch_down();
    QString is_sensor_pitch_org();
    QString is_sensor_pitch_up();
    QString is_sensor_pitch_move();

    // Generic sensor query used by the status system, e.g. sensor("is_sensor_x_left").
    // Strips the "is_sensor_" prefix. "" means the read failed (triState -1).
    QString sensor(const QString &endpoint);

    // Kept so the existing "axis URL" field in the UI stays harmless. There is no
    // URL any more; url() answers with the ports actually in use, which is what
    // the status line and the error messages want to name.
    void setUrl(const QString &ignored);
    QString url() const;

private:
    QString callMove(const QString &direction, double distance, const QString &speed);
    QString callSensor(const QString &name);

    struct Impl;
    std::unique_ptr<Impl> d_;
};
