#include "CaliSession.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <opencv2/imgcodecs.hpp>

const QStringList &CaliSession::captureNames() {
    static const QStringList names = {"positive", "negative", "single", "chessboard"};
    return names;
}

// ---------------------------------------------------------------------- edits

CaliSession::Step CaliSession::applyStep(const Step &step) {
    Step before;
    before.cells.reserve(step.cells.size());
    for (const CellEdit &e : step.cells) {
        before.cells.push_back({e.round, e.row, e.col, table_.cell(e.round, e.row, e.col)});
        table_.setCell(e.round, e.row, e.col, e.text);
    }
    before.fields.reserve(step.fields.size());
    for (const FieldEdit &f : step.fields) {
        before.fields.push_back({f.name, table_.fieldText(f.name)});
        table_.setField(f.name, f.text);
    }
    bump();
    return before;
}

quint64 CaliSession::applyEdits(const std::vector<CellEdit> &cells,
                                const std::vector<FieldEdit> &fields) {
    if (cells.empty() && fields.empty()) return version_;

    Step before = applyStep({cells, fields});
    undo_.push_back(std::move(before));
    if (undo_.size() > kMaxUndo) undo_.erase(undo_.begin());
    // A new edit invalidates the redo branch: redoing after diverging would apply
    // values that were computed from a table state that no longer exists.
    redo_.clear();
    return version_;
}

bool CaliSession::undo() {
    if (undo_.empty()) return false;
    Step step = undo_.back();
    undo_.pop_back();
    redo_.push_back(applyStep(step));
    return true;
}

bool CaliSession::redo() {
    if (redo_.empty()) return false;
    Step step = redo_.back();
    redo_.pop_back();
    undo_.push_back(applyStep(step));
    return true;
}

quint64 CaliSession::markComputed() {
    redo_.clear();
    bump();
    return version_;
}

// ------------------------------------------------------------------- captures

void CaliSession::putCapture(const QString &name, QByteArray encoded) {
    captures_.insert(name, std::move(encoded));
    matCache_.remove(name);  // or the next op analyses the previous shot
    bump();
}

QStringList CaliSession::presentCaptures() const {
    QStringList out;
    for (const QString &n : captureNames())
        if (captures_.contains(n) && !captures_.value(n).isEmpty()) out << n;
    return out;
}

cv::Mat CaliSession::captureMat(const QString &name) const {
    const auto cached = matCache_.constFind(name);
    if (cached != matCache_.constEnd()) return cached.value();

    const QByteArray raw = captures_.value(name);
    if (raw.isEmpty()) return {};
    const cv::Mat buf(1, raw.size(), CV_8UC1, const_cast<char *>(raw.constData()));
    cv::Mat img = cv::imdecode(buf, cv::IMREAD_COLOR);
    matCache_.insert(name, img);
    return img;
}

// ---------------------------------------------------------------- persistence

bool CaliSession::save(const QString &dir, QString *err) const {
    const auto fail = [&](const QString &m) {
        if (err) *err = m;
        return false;
    };
    if (!QDir().mkpath(dir)) return fail("cannot create " + dir);

    QJsonObject meta;
    meta["id"] = id_;
    meta["name"] = name_;
    meta["created_at"] = static_cast<double>(createdAt_);
    meta["version"] = static_cast<double>(version_);

    QJsonObject root;
    root["meta"] = meta;
    // The table travels as the same JSON the wire uses, so a saved session and a
    // transferred one cannot disagree about the format.
    root["table"] =
        QJsonDocument::fromJson(table_.toJson().toUtf8()).object();

    // Write to a temporary file and rename. A session.json truncated by a power cut
    // mid-write is worse than a slightly stale one: the calibration would look
    // present and be unparseable.
    const QString path = dir + "/session.json";
    const QString tmp = path + ".tmp";
    {
        QFile f(tmp);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
            return fail("cannot write " + tmp);
        f.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
        if (!f.flush()) return fail("cannot flush " + tmp);
    }
    QFile::remove(path);
    if (!QFile::rename(tmp, path)) return fail("cannot rename " + tmp + " -> " + path);

    const QString capDir = dir + "/captures";
    if (!captures_.isEmpty() && !QDir().mkpath(capDir)) return fail("cannot create " + capDir);
    for (const QString &n : captureNames()) {
        const QByteArray raw = captures_.value(n);
        const QString cp = capDir + "/" + n + ".png";
        if (raw.isEmpty()) {
            QFile::remove(cp);  // slot cleared since the last save
            continue;
        }
        QFile f(cp);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
            return fail("cannot write " + cp);
        f.write(raw);
    }
    return true;
}

std::unique_ptr<CaliSession> CaliSession::load(const QString &dir, QString *err) {
    const auto fail = [&](const QString &m) {
        if (err) *err = m;
        return std::unique_ptr<CaliSession>();
    };

    QFile f(dir + "/session.json");
    if (!f.open(QIODevice::ReadOnly)) return fail("cannot read " + dir + "/session.json");
    QJsonParseError pe{};
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject())
        return fail("session.json is not valid JSON: " + pe.errorString());

    const QJsonObject root = doc.object();
    const QJsonObject meta = root.value("meta").toObject();
    const QString id = meta.value("id").toString(QFileInfo(dir).fileName());
    auto s = std::make_unique<CaliSession>(id, meta.value("name").toString(id));
    s->createdAt_ = static_cast<qint64>(meta.value("created_at").toDouble());

    bool ok = false;
    s->table_ = CaliTableData::fromJson(
        QString::fromUtf8(QJsonDocument(root.value("table").toObject()).toJson()), &ok);
    if (!ok) return fail("session table did not parse");
    // A restored session must still have its 11 rounds even if the saved table was
    // sparse enough to omit some of them.
    for (int r = 0; r <= 10; ++r)
        if (!s->table_.hasRound(r)) s->table_.addRound(r);

    for (const QString &n : captureNames()) {
        QFile c(dir + "/captures/" + n + ".png");
        if (c.open(QIODevice::ReadOnly)) s->captures_.insert(n, c.readAll());
    }

    // Restore the version rather than starting from 0: a client that still holds a
    // mirror from before the restart would otherwise think its stale copy was
    // current. Undo history is deliberately NOT persisted -- it is a UI affordance
    // for the current sitting, and restoring it would let an operator undo edits
    // they never saw made.
    s->version_ = static_cast<quint64>(meta.value("version").toDouble());
    return s;
}

QString CaliSession::stateJson() const {
    QJsonObject meta;
    meta["id"] = id_;
    meta["name"] = name_;
    meta["created_at"] = static_cast<double>(createdAt_);
    meta["version"] = static_cast<double>(version_);
    meta["can_undo"] = canUndo();
    meta["can_redo"] = canRedo();

    QJsonArray caps;
    for (const QString &n : presentCaptures()) caps.append(n);
    meta["captures"] = caps;

    QJsonObject root;
    root["meta"] = meta;
    root["table"] = QJsonDocument::fromJson(table_.toJson().toUtf8()).object();
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}
