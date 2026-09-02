#include "session_services.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace SessionOps {
namespace {

QJsonObject parseObj(const QString &s, bool *ok) {
    if (s.trimmed().isEmpty()) {
        if (ok) *ok = true;
        return {};
    }
    QJsonParseError e{};
    const QJsonDocument d = QJsonDocument::fromJson(s.toUtf8(), &e);
    const bool good = e.error == QJsonParseError::NoError && d.isObject();
    if (ok) *ok = good;
    return good ? d.object() : QJsonObject();
}

QString dump(const QJsonObject &o) {
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

Reply fail(const QString &msg) {
    Reply r;
    r.success = false;
    r.message = msg;
    return r;
}

void fire(const EventFn &event, const QString &id, quint64 v, const QString &kind,
          const QString &detail = QString()) {
    if (event) event(id, v, kind, detail);
}

// Which capture slots an op consumes. Explicit "slots" in params wins; otherwise
// the conventional default for the op's image count.
//
// This is what replaced uploading the images: the op names slots in the session
// instead of carrying pixels, so the pictures never leave the machine that took
// them until somebody wants to look at one.
// NOTE: the local variable is not called `slots`. Qt defines `slots` as a macro
// (the Q_SLOTS keyword), so a variable of that name expands to nothing and the
// errors that follow point everywhere except at the name.
QStringList slotsFor(const QString &op, const QJsonObject &params, int want) {
    QStringList named;
    for (const QJsonValue &v : params.value("slots").toArray()) named << v.toString();
    if (!named.isEmpty()) return named;

    if (op == ComputeOps::detect::kPatternRingRadii) return {"single"};
    if (want >= 2) return {"positive", "negative"};
    return {"positive"};
}

}  // namespace

// -------------------------------------------------------------------- command

Reply command(SessionRegistry &reg, const QString &cmd, const QString &sessionId,
              const QString &payload, qint64 nowSeconds, const EventFn &event) {
    bool ok = false;
    const QJsonObject p = parseObj(payload, &ok);
    if (!ok) return fail("payload is not a JSON object");

    Reply r;
    QString err;

    if (cmd == "create") {
        CaliSession *s = reg.create(p.value("name").toString(), nowSeconds, &err);
        if (!s) return fail("create failed: " + err);
        r.success = true;
        r.sessionId = s->id();
        r.version = s->version();
        // The initial state rides along: a client that just created a session is
        // certain to want it, and this saves the round trip.
        QJsonObject res;
        res["version"] = static_cast<double>(s->version());
        res["state"] = QJsonDocument::fromJson(s->stateJson().toUtf8()).object();
        r.result = dump(res);
        fire(event, s->id(), s->version(), "created", s->name());
        return r;
    }

    if (cmd == "list") {
        QJsonArray arr;
        for (const SessionRegistry::Summary &s : reg.list()) {
            QJsonObject o;
            o["id"] = s.id;
            o["name"] = s.name;
            o["created_at"] = static_cast<double>(s.createdAt);
            o["version"] = static_cast<double>(s.version);
            QJsonArray caps;
            for (const QString &c : s.captures) caps.append(c);
            o["captures"] = caps;
            arr.append(o);
        }
        QJsonObject res;
        res["sessions"] = arr;
        r.success = true;
        r.result = dump(res);
        return r;
    }

    if (sessionId.isEmpty()) return fail("command \"" + cmd + "\" needs a session_id");

    if (cmd == "open") {
        CaliSession *s = reg.open(sessionId, &err);
        if (!s) return fail("open failed: " + err);
        r.success = true;
        r.sessionId = s->id();
        r.version = s->version();
        QJsonObject res;
        res["version"] = static_cast<double>(s->version());
        res["state"] = QJsonDocument::fromJson(s->stateJson().toUtf8()).object();
        r.result = dump(res);
        fire(event, s->id(), s->version(), "opened");
        return r;
    }

    if (cmd == "close") {
        if (!reg.close(sessionId, &err)) return fail("close failed: " + err);
        r.success = true;
        r.sessionId = sessionId;
        fire(event, sessionId, 0, "closed");
        return r;
    }

    if (cmd == "delete") {
        // Irreversible, and it takes hours of calibration with it. A mistyped
        // `ros2 service call` should not be enough.
        if (!p.value("confirm").toBool(false))
            return fail("delete needs {\"confirm\": true} -- it removes the session and its "
                        "captures for good");
        if (!reg.remove(sessionId, &err)) return fail("delete failed: " + err);
        r.success = true;
        r.sessionId = sessionId;
        fire(event, sessionId, 0, "deleted");
        return r;
    }

    // The rest need the session in memory. Not auto-opened: a typo'd id would then
    // quietly do work against a freshly loaded session instead of failing.
    CaliSession *s = reg.resident(sessionId);
    if (!s) return fail("session not open: " + sessionId + " (send \"open\" first)");

    if (cmd == "rename") {
        const QString name = p.value("name").toString();
        if (name.isEmpty()) return fail("rename needs {\"name\": \"...\"}");
        s->setName(name);
        r.success = true;
        r.sessionId = sessionId;
        r.version = s->version();
        fire(event, sessionId, s->version(), "renamed", name);
        return r;
    }

    if (cmd == "undo" || cmd == "redo") {
        const bool applied = (cmd == "undo") ? s->undo() : s->redo();
        QJsonObject res;
        res["applied"] = applied;
        res["version"] = static_cast<double>(s->version());
        r.success = true;
        r.sessionId = sessionId;
        r.version = s->version();
        r.result = dump(res);
        // Only announce a real change: an undo on an empty stack changes nothing and
        // would make every client refetch for no reason.
        if (applied) fire(event, sessionId, s->version(), cmd == "undo" ? "undone" : "redone");
        return r;
    }

    if (cmd == "save") {
        if (!reg.save(sessionId, &err)) return fail("save failed: " + err);
        QJsonObject res;
        res["version"] = static_cast<double>(s->version());
        res["dir"] = reg.dirFor(sessionId);
        r.success = true;
        r.sessionId = sessionId;
        r.version = s->version();
        r.result = dump(res);
        fire(event, sessionId, s->version(), "saved");
        return r;
    }

    return fail("unknown command: " + cmd);
}

// ----------------------------------------------------------------------- edit

Reply edit(SessionRegistry &reg, const QString &sessionId, const QString &edits,
           const EventFn &event) {
    CaliSession *s = reg.resident(sessionId);
    if (!s) return fail("session not open: " + sessionId);

    bool ok = false;
    const QJsonObject e = parseObj(edits, &ok);
    if (!ok) return fail("edits is not a JSON object");

    std::vector<CaliSession::CellEdit> cells;
    for (const QJsonValue &v : e.value("cells").toArray()) {
        const QJsonObject c = v.toObject();
        CaliSession::CellEdit ce;
        ce.round = c.value("round").toInt(-1);
        ce.row = c.value("row").toInt(-1);
        ce.col = c.value("col").toInt(-1);
        if (ce.round < 0 || ce.round > 10 || ce.row < 0 || ce.col < 0)
            return fail(QString("cell edit out of range: round=%1 row=%2 col=%3")
                            .arg(ce.round).arg(ce.row).arg(ce.col));
        // Numbers accepted as well as strings, so a hand-written service call works,
        // but stored as text -- "" and "0" are different to the formulas.
        const QJsonValue t = c.value("text");
        ce.text = t.isString() ? t.toString()
                               : (t.isDouble() ? QString::number(t.toDouble(), 'g', 15) : QString());
        cells.push_back(std::move(ce));
    }

    std::vector<CaliSession::FieldEdit> fields;
    const QJsonObject f = e.value("fields").toObject();
    for (auto it = f.constBegin(); it != f.constEnd(); ++it) {
        const QJsonValue t = it.value();
        fields.push_back({it.key(), t.isString()
                                        ? t.toString()
                                        : (t.isDouble() ? QString::number(t.toDouble(), 'g', 15)
                                                        : QString())});
    }

    if (cells.empty() && fields.empty()) return fail("edits contained nothing to apply");

    Reply r;
    r.success = true;
    r.sessionId = sessionId;
    r.version = s->applyEdits(cells, fields);
    fire(event, sessionId, r.version, "edited",
         QString("%1 cell(s), %2 field(s)").arg(cells.size()).arg(fields.size()));
    return r;
}

// ---------------------------------------------------------------------- state

Reply state(SessionRegistry &reg, const QString &sessionId, quint64 sinceVersion, bool *changed,
            QString *stateJson) {
    CaliSession *s = reg.resident(sessionId);
    if (!s) return fail("session not open: " + sessionId);

    Reply r;
    r.success = true;
    r.sessionId = sessionId;
    r.version = s->version();

    if (sinceVersion > 0 && s->version() <= sinceVersion) {
        if (changed) *changed = false;
        if (stateJson) stateJson->clear();
        r.message = "mirror is current";
        return r;
    }
    if (changed) *changed = true;
    if (stateJson) *stateJson = s->stateJson();
    return r;
}

// -------------------------------------------------------------------- capture

Reply putCapture(SessionRegistry &reg, const QString &sessionId, const QString &slot,
                 const QByteArray &encoded, int *width, int *height, const EventFn &event) {
    CaliSession *s = reg.resident(sessionId);
    if (!s) return fail("session not open: " + sessionId);
    if (!CaliSession::captureNames().contains(slot))
        return fail("unknown slot \"" + slot + "\"; expected one of " +
                    CaliSession::captureNames().join(", "));
    if (encoded.isEmpty()) return fail("no frame to store");

    s->putCapture(slot, encoded);

    // Decode once here to report the size AND to reject a frame that does not
    // decode. Storing an undecodable capture would turn a camera problem into a
    // detection failure later, somewhere else.
    const cv::Mat m = s->captureMat(slot);
    if (m.empty()) {
        s->putCapture(slot, QByteArray());
        return fail("frame did not decode; slot left empty");
    }
    if (width) *width = m.cols;
    if (height) *height = m.rows;

    Reply r;
    r.success = true;
    r.sessionId = sessionId;
    r.version = s->version();
    r.message = QString("%1 stored, %2x%3").arg(slot).arg(m.cols).arg(m.rows);
    fire(event, sessionId, r.version, "captured", slot);
    return r;
}

// ---------------------------------------------------------------------- image

Reply image(SessionRegistry &reg, const QString &sessionId, const QString &slot, int maxSide,
            QByteArray *out, int *width, int *height) {
    CaliSession *s = reg.resident(sessionId);
    if (!s) return fail("session not open: " + sessionId);
    if (!s->hasCapture(slot)) return fail("slot \"" + slot + "\" is empty");

    const cv::Mat full = s->captureMat(slot);
    if (full.empty()) return fail("stored capture does not decode");
    if (width) *width = full.cols;
    if (height) *height = full.rows;

    Reply r;
    r.success = true;
    r.sessionId = sessionId;
    r.version = s->version();

    const int longest = std::max(full.cols, full.rows);
    if (maxSide <= 0 || longest <= maxSide) {
        // Original bytes, untouched. Re-encoding would be lossless but pointless,
        // and for anything that will be analysed it must be the original.
        if (out) *out = s->capture(slot);
        r.message = QString("%1 %2x%3, original").arg(slot).arg(full.cols).arg(full.rows);
        return r;
    }

    const double k = double(maxSide) / longest;
    cv::Mat small;
    cv::resize(full, small, cv::Size(), k, k, cv::INTER_AREA);
    std::vector<unsigned char> buf;
    if (!cv::imencode(".png", small, buf)) return fail("could not encode the scaled preview");
    if (out) *out = QByteArray(reinterpret_cast<const char *>(buf.data()), int(buf.size()));
    r.message = QString("%1 %2x%3 scaled to %4x%5 for preview")
                    .arg(slot).arg(full.cols).arg(full.rows).arg(small.cols).arg(small.rows);
    return r;
}

// ----------------------------------------------------------------- runCompute

Reply runCompute(SessionRegistry &reg, const QString &sessionId, const QString &op,
                 const QString &params, const ComputeOps::ProgressFn &progress,
                 const EventFn &event) {
    CaliSession *s = reg.resident(sessionId);
    if (!s) return fail("session not open: " + sessionId);

    bool ok = false;
    const QJsonObject p = parseObj(params, &ok);
    if (!ok) return fail("params is not a JSON object");

    QString err, result;
    Reply r;
    r.sessionId = sessionId;

    const int want = ComputeOps::detectImageCount(op);
    if (want > 0) {
        // Image analysis: the pictures come out of the session, not off the wire.
        const QStringList wanted = slotsFor(op, p, want);
        if (wanted.size() < want)
            return fail(QString("op %1 needs %2 capture slot(s), got %3")
                            .arg(op).arg(want).arg(wanted.size()));

        std::vector<cv::Mat> images;
        for (int i = 0; i < want; ++i) {
            if (!s->hasCapture(wanted[i]))
                return fail("capture slot \"" + wanted[i] + "\" is empty -- take that shot first");
            images.push_back(s->captureMat(wanted[i]));
            if (images.back().empty())
                return fail("capture slot \"" + wanted[i] + "\" does not decode");
        }

        result = ComputeOps::runDetectOp(op, images, params, &err);
        if (result.isEmpty()) return fail(err);
        r.success = true;
        r.result = result;
        // Analysis reads; it does not change the session, so the version stands and
        // no client needs to refetch a table that did not move.
        r.version = s->version();
        r.message = op + " on " + wanted.mid(0, want).join("+");
        fire(event, sessionId, r.version, "computed", op);
        return r;
    }

    // Calibration pipeline: reads and writes the session's table in place.
    if (!ComputeOps::runCaliOp(op, s->table(), params, &result, &err, progress)) return fail(err);
    r.success = true;
    r.result = result;
    // The table moved, so this IS a state change: advance the version so every
    // mirror learns to refetch.
    r.version = s->markComputed();
    r.message = op;
    fire(event, sessionId, r.version, "computed", op);
    return r;
}

}  // namespace SessionOps
