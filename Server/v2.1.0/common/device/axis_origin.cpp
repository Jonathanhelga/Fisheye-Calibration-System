#include "axis_origin.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QStandardPaths>

#include <iostream>

namespace {

// The five the rig has. Reading against a fixed list rather than whatever keys
// the file happens to carry keeps a hand-edited "pich" from becoming a sixth
// axis nobody ever sees.
const char *const kAxes[] = {"x", "y", "z", "yaw", "pitch"};

}  // namespace

bool AxisOrigin::hasZero(const QString &axis) const {
    return zeroCount.contains(axis.toLower());
}

double AxisOrigin::zeroFor(const QString &axis) const {
    return zeroCount.value(axis.toLower(), 0.0);
}

void AxisOrigin::setZero(const QString &axis, double rawCount) {
    const QString a = axis.toLower();
    zeroCount[a] = rawCount;
    zeroTakenAt[a] = QDateTime::currentDateTime().toString(Qt::ISODate);
    restored = false;  // this one was taken here, not read off disk
}

void AxisOrigin::clear() {
    zeroCount.clear();
    zeroTakenAt.clear();
    lastPos.clear();
    restored = false;
}

QString AxisOrigin::userPath() {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (dir.isEmpty())  // no such location: beside the binary, as devices.json does
        return QDir(QCoreApplication::applicationDirPath())
            .filePath(QStringLiteral("positions.json"));
    return QDir(dir).filePath(QStringLiteral("positions.json"));
}

AxisOrigin AxisOrigin::load() {
    AxisOrigin origin;

    QFile f(userPath());
    if (!f.exists()) return origin;  // nothing homed yet: not an error
    if (!f.open(QIODevice::ReadOnly)) {
        std::cerr << "[positions] cannot read " << qUtf8Printable(userPath()) << ": "
                  << qUtf8Printable(f.errorString()) << " -- starting unhomed\n";
        return origin;
    }

    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        std::cerr << "[positions] " << qUtf8Printable(userPath()) << " is not valid JSON ("
                  << qUtf8Printable(err.errorString()) << ") -- starting unhomed\n";
        return origin;
    }

    const QJsonObject root = doc.object();
    const QJsonObject axes = root.value(QStringLiteral("axes")).toObject();
    for (const char *name : kAxes) {
        const QJsonObject a = axes.value(QLatin1String(name)).toObject();
        if (a.isEmpty()) continue;
        const QJsonValue zero = a.value(QStringLiteral("zero_count"));
        if (zero.isDouble()) {
            origin.zeroCount[QLatin1String(name)] = zero.toDouble();
            origin.zeroTakenAt[QLatin1String(name)] =
                a.value(QStringLiteral("zero_taken_at")).toString();
        }
        const QJsonValue pos = a.value(QStringLiteral("last_position"));
        if (pos.isDouble()) origin.lastPos[QLatin1String(name)] = pos.toDouble();
    }

    // Anything that came off disk carries the power-cycle caveat in the header:
    // it describes a counter this run has not yet seen move.
    origin.restored = !origin.zeroCount.isEmpty();
    return origin;
}

bool AxisOrigin::save() const {
    QJsonObject axes;
    for (const char *name : kAxes) {
        const QString a = QLatin1String(name);
        if (!zeroCount.contains(a) && !lastPos.contains(a)) continue;
        QJsonObject o;
        if (zeroCount.contains(a)) {
            o[QStringLiteral("zero_count")] = zeroCount.value(a);
            o[QStringLiteral("zero_taken_at")] = zeroTakenAt.value(a);
        }
        if (lastPos.contains(a)) o[QStringLiteral("last_position")] = lastPos.value(a);
        axes[a] = o;
    }

    QJsonObject root;
    // Read by nothing -- it is here so the file explains itself to whoever opens
    // it looking for why a coordinate reads the way it does.
    root[QStringLiteral("_note")] = QStringLiteral(
        "Software zero per axis, in raw controller counts, recorded at HOME. "
        "Positions shown in the app are (counter - zero_count) * "
        "axis.units_per_count from devices.json. Valid only while the "
        "controller keeps power: after a power cycle the counter restarts and "
        "these must be re-taken by homing.");
    root[QStringLiteral("axes")] = axes;

    const QString p = userPath();
    QDir().mkpath(QFileInfo(p).absolutePath());

    QFile f(p);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        std::cerr << "[positions] cannot write " << qUtf8Printable(p) << ": "
                  << qUtf8Printable(f.errorString()) << "\n";
        return false;
    }
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}
