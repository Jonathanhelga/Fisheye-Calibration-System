#pragma once

// Small JSON helpers shared by the op dispatchers. Internal to src/core/compute.

#include <string>
#include <vector>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

namespace ComputeOps {
namespace json {

inline QJsonObject parseObject(const QString &text, bool *ok) {
    // An empty params string means "no parameters", not a parse error: several ops
    // take none and it is friendlier than forcing callers to send "{}".
    if (text.trimmed().isEmpty()) {
        if (ok) *ok = true;
        return {};
    }
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        if (ok) *ok = false;
        return {};
    }
    if (ok) *ok = true;
    return doc.object();
}

inline QString dump(const QJsonObject &o) {
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

// toInt() returns its default for a non-integral JSON number, which turns 45.5
// into the default instead of 45 -- go through double so a float on the wire
// still lands somewhere sensible.
inline int getInt(const QJsonObject &o, const QString &key, int def) {
    const QJsonValue v = o.value(key);
    if (v.isDouble()) return static_cast<int>(std::lround(v.toDouble()));
    if (v.isString()) {
        bool ok = false;
        const double d = v.toString().trimmed().toDouble(&ok);
        if (ok) return static_cast<int>(std::lround(d));
    }
    return def;
}

inline double getDouble(const QJsonObject &o, const QString &key, double def) {
    const QJsonValue v = o.value(key);
    if (v.isDouble()) return v.toDouble();
    if (v.isString()) {
        bool ok = false;
        const double d = v.toString().trimmed().toDouble(&ok);
        if (ok) return d;
    }
    return def;
}

inline bool getBool(const QJsonObject &o, const QString &key, bool def) {
    const QJsonValue v = o.value(key);
    return v.isBool() ? v.toBool() : def;
}

inline std::vector<double> toDoubleVec(const QJsonValue &v) {
    std::vector<double> out;
    for (const QJsonValue &e : v.toArray()) out.push_back(e.toDouble());
    return out;
}

inline std::vector<int> toIntVec(const QJsonValue &v) {
    std::vector<int> out;
    for (const QJsonValue &e : v.toArray()) out.push_back(static_cast<int>(std::lround(e.toDouble())));
    return out;
}

inline QJsonArray fromDoubleVec(const std::vector<double> &v) {
    QJsonArray a;
    for (double d : v) a.append(d);
    return a;
}

inline QJsonArray fromIntVec(const std::vector<int> &v) {
    QJsonArray a;
    for (int d : v) a.append(d);
    return a;
}

}  // namespace json
}  // namespace ComputeOps
