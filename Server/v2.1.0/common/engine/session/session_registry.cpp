#include "session_registry.h"

#include <algorithm>

#include <QDateTime>
#include <QDir>
#include <QTimeZone>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>

QString SessionRegistry::makeId(qint64 nowSeconds) const {
    // .toUTC() rather than a QTimeZone overload: QTimeZone::UTC only exists from
    // Qt 6.5, and the rig may be on an older 6.x.
    const QString stamp =
        QDateTime::fromSecsSinceEpoch(nowSeconds).toUTC().toString("yyyyMMdd-HHmmss");
    // Four hex digits of tail: two sessions created in the same second must not
    // collide, and a counter would restart at 0 when the node restarts.
    for (int attempt = 0; attempt < 100; ++attempt) {
        const QString id =
            QString("%1-%2").arg(stamp, QString::number(QRandomGenerator::global()->bounded(0x10000),
                                                        16).rightJustified(4, '0'));
        if (!QDir(dirFor(id)).exists()) return id;
    }
    return stamp + "-xxxx";
}

std::vector<SessionRegistry::Summary> SessionRegistry::list() const {
    std::vector<Summary> out;
    const QDir root(root_);
    if (!root.exists()) return out;

    for (const QString &entry : root.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QFile f(root_ + "/" + entry + "/session.json");
        if (!f.open(QIODevice::ReadOnly)) continue;  // not a session directory
        const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        if (!doc.isObject()) continue;
        const QJsonObject meta = doc.object().value("meta").toObject();

        Summary s;
        s.id = meta.value("id").toString(entry);
        s.name = meta.value("name").toString(s.id);
        s.createdAt = static_cast<qint64>(meta.value("created_at").toDouble());
        s.version = static_cast<quint64>(meta.value("version").toDouble());
        // Read from the directory, not from session.json: the files are the truth
        // about which captures survived, and a hand-copied session may not agree
        // with its own metadata.
        for (const QString &n : CaliSession::captureNames())
            if (QFile::exists(root_ + "/" + entry + "/captures/" + n + ".png")) s.captures << n;
        out.push_back(std::move(s));
    }

    std::sort(out.begin(), out.end(), [](const Summary &a, const Summary &b) {
        return a.createdAt != b.createdAt ? a.createdAt > b.createdAt : a.id > b.id;
    });
    return out;
}

CaliSession *SessionRegistry::create(const QString &name, qint64 nowSeconds, QString *err) {
    const QString id = makeId(nowSeconds);
    auto s = std::make_unique<CaliSession>(id, name.isEmpty() ? id : name);
    s->setCreatedAt(nowSeconds);

    // Write it immediately. A session that exists only in memory is one a client
    // cannot find again after a node restart, and "create" should mean it exists.
    if (!s->save(dirFor(id), err)) return nullptr;

    CaliSession *raw = s.get();
    loaded_[id] = std::move(s);
    return raw;
}

CaliSession *SessionRegistry::open(const QString &id, QString *err) {
    const auto it = loaded_.find(id);
    if (it != loaded_.end()) return it->second.get();

    auto s = CaliSession::load(dirFor(id), err);
    if (!s) return nullptr;
    CaliSession *raw = s.get();
    loaded_[id] = std::move(s);
    return raw;
}

CaliSession *SessionRegistry::resident(const QString &id) {
    const auto it = loaded_.find(id);
    return it == loaded_.end() ? nullptr : it->second.get();
}

bool SessionRegistry::save(const QString &id, QString *err) {
    CaliSession *s = resident(id);
    if (!s) {
        if (err) *err = "session not open: " + id;
        return false;
    }
    return s->save(dirFor(id), err);
}

bool SessionRegistry::close(const QString &id, QString *err) {
    const auto it = loaded_.find(id);
    if (it == loaded_.end()) return true;  // already not resident
    // Save on the way out. Closing is not discarding, and an operator who closes a
    // window does not expect the last few edits to go with it.
    const bool ok = it->second->save(dirFor(id), err);
    loaded_.erase(it);
    return ok;
}

bool SessionRegistry::remove(const QString &id, QString *err) {
    loaded_.erase(id);
    QDir d(dirFor(id));
    if (!d.exists()) {
        if (err) *err = "no such session: " + id;
        return false;
    }
    if (!d.removeRecursively()) {
        if (err) *err = "could not remove " + dirFor(id);
        return false;
    }
    return true;
}
