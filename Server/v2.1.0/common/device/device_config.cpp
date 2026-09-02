#include "device_config.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QStandardPaths>

#include <iostream>

namespace {

// Read one key, leaving the target untouched when it is absent or of the wrong
// type. "Wrong type" is deliberately treated like "absent": a hand-edited file
// with `"camera_id": "0"` should start the rig on camera 0 with a note, not
// refuse to start.
void readString(const QJsonObject &o, const char *key, QString *out) {
    const QJsonValue v = o.value(QLatin1String(key));
    if (v.isString()) *out = v.toString();
}

void readInt(const QJsonObject &o, const char *key, int *out) {
    const QJsonValue v = o.value(QLatin1String(key));
    if (v.isDouble()) *out = v.toInt();
}

void readBool(const QJsonObject &o, const char *key, bool *out) {
    const QJsonValue v = o.value(QLatin1String(key));
    if (v.isBool()) *out = v.toBool();
}

void readDouble(const QJsonObject &o, const char *key, double *out) {
    const QJsonValue v = o.value(QLatin1String(key));
    if (v.isDouble()) *out = v.toDouble();
}

// The struct's defaults are the yuanman gearing, because that is the rig this
// app was written against. A yinda commands in user units instead, so its
// counts are units -- see DeviceConfig::unitsPerCountX. Applied after "system"
// is read and before the explicit keys are, so a hand-written units_per_count
// still wins over both.
void applyUnitDefaultsForSystem(const QString &system, DeviceConfig *cfg) {
    if (system != QLatin1String("yinda")) return;
    cfg->unitsPerCountX = 1.0;
    cfg->unitsPerCountY = 1.0;
    cfg->unitsPerCountZ = 1.0;
    cfg->unitsPerCountYaw = -1.0;
    cfg->unitsPerCountPitch = -1.0;
}

}  // namespace

double DeviceConfig::unitsPerCount(const QString &axis) const {
    const QString a = axis.toLower();
    if (a == QLatin1String("x")) return unitsPerCountX;
    if (a == QLatin1String("y")) return unitsPerCountY;
    if (a == QLatin1String("z")) return unitsPerCountZ;
    if (a == QLatin1String("yaw")) return unitsPerCountYaw;
    if (a == QLatin1String("pitch")) return unitsPerCountPitch;
    return 0.0;
}

double DeviceConfig::travelToLimit(const QString &axis) const {
    const QString a = axis.toLower();
    if (a == QLatin1String("x")) return travelToLimitX;
    if (a == QLatin1String("y")) return travelToLimitY;
    if (a == QLatin1String("z")) return travelToLimitZ;
    if (a == QLatin1String("yaw")) return travelToLimitYaw;
    if (a == QLatin1String("pitch")) return travelToLimitPitch;
    return 0.0;
}

QString DeviceConfig::bundledPath() {
    return QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("config/devices.json"));
}

QString DeviceConfig::userPath() {
    // AppConfigLocation resolves to %LOCALAPPDATA%/<org>/<app> on Windows and
    // ~/.config/<org>/<app> elsewhere. Both are writable by the user running the
    // app, which the install directory is not.
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (dir.isEmpty())  // no such location: fall back to beside the binary
        return bundledPath();
    return QDir(dir).filePath(QStringLiteral("devices.json"));
}

QString DeviceConfig::path() {
    // The one load() would read: the per-user file once it exists, the shipped
    // template until then.
    return QFile::exists(userPath()) ? userPath() : bundledPath();
}

DeviceConfig DeviceConfig::load() {
    DeviceConfig cfg;

    // Said once per run, not once per device. The axis, the camera and the
    // monitor each load their own copy -- they are independent and none of them
    // should have to be constructed before another -- but three identical lines
    // about a missing file read like three separate problems.
    static bool announced = false;

    QFile f(path());
    if (!f.exists()) {
        // Not an error. A fresh install has no file, and every default below is
        // the value the rig was running with.
        if (!announced) {
            std::cerr << "[devices] no " << qUtf8Printable(path()) << ", using defaults\n";
            announced = true;
        }
        return cfg;
    }
    if (!f.open(QIODevice::ReadOnly)) {
        std::cerr << "[devices] cannot read " << qUtf8Printable(path()) << ": "
                  << qUtf8Printable(f.errorString()) << " -- using defaults\n";
        return cfg;
    }

    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        std::cerr << "[devices] " << qUtf8Printable(path()) << " is not valid JSON ("
                  << qUtf8Printable(err.errorString()) << ") -- using defaults\n";
        return cfg;
    }

    const QJsonObject root = doc.object();

    const QJsonObject axis = root.value(QStringLiteral("axis")).toObject();
    readString(axis, "system", &cfg.axisSystem);
    readString(axis, "arduino_port", &cfg.arduinoPort);
    readInt(axis, "arduino_baud", &cfg.arduinoBaud);
    readString(axis, "crux_port", &cfg.cruxPort);
    readInt(axis, "crux_baud", &cfg.cruxBaud);
    readString(axis, "yaw_port", &cfg.yawPort);
    readString(axis, "pitch_port", &cfg.pitchPort);
    readString(axis, "x_port", &cfg.xPort);
    readString(axis, "y_port", &cfg.yPort);
    readString(axis, "z_port", &cfg.zPort);
    readInt(axis, "yinda_baud", &cfg.yindaBaud);
    readInt(axis, "crux_reply_ms", &cfg.cruxReplyMs);
    readInt(axis, "arduino_reply_ms", &cfg.arduinoReplyMs);
    readBool(axis, "enable_write_position", &cfg.enableWritePosition);

    applyUnitDefaultsForSystem(cfg.axisSystem, &cfg);
    const QJsonObject units = axis.value(QStringLiteral("units_per_count")).toObject();
    readDouble(units, "x", &cfg.unitsPerCountX);
    readDouble(units, "y", &cfg.unitsPerCountY);
    readDouble(units, "z", &cfg.unitsPerCountZ);
    readDouble(units, "yaw", &cfg.unitsPerCountYaw);
    readDouble(units, "pitch", &cfg.unitsPerCountPitch);

    const QJsonObject travel = axis.value(QStringLiteral("travel_to_limit")).toObject();
    readDouble(travel, "x", &cfg.travelToLimitX);
    readDouble(travel, "y", &cfg.travelToLimitY);
    readDouble(travel, "z", &cfg.travelToLimitZ);
    readDouble(travel, "yaw", &cfg.travelToLimitYaw);
    readDouble(travel, "pitch", &cfg.travelToLimitPitch);

    const QJsonObject cam = root.value(QStringLiteral("camera")).toObject();
    readInt(cam, "camera_id", &cfg.cameraId);
    readString(cam, "backend", &cfg.cameraBackend);
    readInt(cam, "image_width", &cfg.imageWidth);
    readInt(cam, "image_height", &cfg.imageHeight);
    readString(cam, "capture_format", &cfg.captureFormat);
    readInt(cam, "jpeg_quality", &cfg.jpegQuality);

    const QJsonObject mon = root.value(QStringLiteral("monitor")).toObject();
    readString(mon, "top", &cfg.displayTop);
    readString(mon, "n", &cfg.displayN);
    readString(mon, "w", &cfg.displayW);
    readString(mon, "s", &cfg.displayS);
    readString(mon, "e", &cfg.displayE);

    return cfg;
}

bool DeviceConfig::save() const {
    QJsonObject axis;
    axis[QStringLiteral("system")] = axisSystem;
    axis[QStringLiteral("arduino_port")] = arduinoPort;
    axis[QStringLiteral("arduino_baud")] = arduinoBaud;
    axis[QStringLiteral("crux_port")] = cruxPort;
    axis[QStringLiteral("crux_baud")] = cruxBaud;
    axis[QStringLiteral("yaw_port")] = yawPort;
    axis[QStringLiteral("pitch_port")] = pitchPort;
    axis[QStringLiteral("x_port")] = xPort;
    axis[QStringLiteral("y_port")] = yPort;
    axis[QStringLiteral("z_port")] = zPort;
    axis[QStringLiteral("yinda_baud")] = yindaBaud;
    axis[QStringLiteral("crux_reply_ms")] = cruxReplyMs;
    axis[QStringLiteral("arduino_reply_ms")] = arduinoReplyMs;
    axis[QStringLiteral("enable_write_position")] = enableWritePosition;

    QJsonObject units;
    units[QStringLiteral("x")] = unitsPerCountX;
    units[QStringLiteral("y")] = unitsPerCountY;
    units[QStringLiteral("z")] = unitsPerCountZ;
    units[QStringLiteral("yaw")] = unitsPerCountYaw;
    units[QStringLiteral("pitch")] = unitsPerCountPitch;
    axis[QStringLiteral("units_per_count")] = units;

    QJsonObject travel;
    travel[QStringLiteral("x")] = travelToLimitX;
    travel[QStringLiteral("y")] = travelToLimitY;
    travel[QStringLiteral("z")] = travelToLimitZ;
    travel[QStringLiteral("yaw")] = travelToLimitYaw;
    travel[QStringLiteral("pitch")] = travelToLimitPitch;
    axis[QStringLiteral("travel_to_limit")] = travel;

    QJsonObject cam;
    cam[QStringLiteral("camera_id")] = cameraId;
    cam[QStringLiteral("backend")] = cameraBackend;
    cam[QStringLiteral("image_width")] = imageWidth;
    cam[QStringLiteral("image_height")] = imageHeight;
    cam[QStringLiteral("capture_format")] = captureFormat;
    cam[QStringLiteral("jpeg_quality")] = jpegQuality;

    QJsonObject mon;
    mon[QStringLiteral("top")] = displayTop;
    mon[QStringLiteral("n")] = displayN;
    mon[QStringLiteral("w")] = displayW;
    mon[QStringLiteral("s")] = displayS;
    mon[QStringLiteral("e")] = displayE;

    QJsonObject root;
    root[QStringLiteral("axis")] = axis;
    root[QStringLiteral("camera")] = cam;
    root[QStringLiteral("monitor")] = mon;

    // Always the per-user file, never the template beside the binary: on a real
    // Windows install that one lives under Program Files and the write would fail
    // for every operator who is not an administrator.
    const QString p = userPath();
    QDir().mkpath(QFileInfo(p).absolutePath());

    QFile f(p);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        std::cerr << "[devices] cannot write " << qUtf8Printable(p) << ": "
                  << qUtf8Printable(f.errorString()) << "\n";
        return false;
    }
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    std::cerr << "[devices] saved " << qUtf8Printable(p) << "\n";
    return true;
}
