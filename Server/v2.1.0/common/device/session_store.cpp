#include "session_store.h"
#include <memory>

#include <utility>

#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QThread>

#include <opencv2/imgcodecs.hpp>

#include "CaliSession.h"
#include "camera_device.h"
#include "session_registry.h"
#include "session_services.h"

namespace {

QString dump(const QJsonObject &o) {
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

// An empty table to hand back from mirror() when nothing is open. Static so the
// reference stays valid; const so nobody can quietly calibrate into it.
const CaliTableData &emptyTable() {
    static const CaliTableData empty;
    return empty;
}

}  // namespace

struct SessionStore::Impl {
    CameraDevice *camera = nullptr;
    QString root;
    std::unique_ptr<SessionRegistry> registry;
    QString currentId;

    CaliSession *current() {
        if (currentId.isEmpty()) return nullptr;
        return registry->resident(currentId);
    }
    const CaliSession *current() const {
        if (currentId.isEmpty()) return nullptr;
        return const_cast<SessionRegistry *>(registry.get())->resident(currentId);
    }

    static qint64 nowSeconds() { return QDateTime::currentSecsSinceEpoch(); }

    // Turn a handler Reply into the bool/err pair every method here returns.
    static bool finish(const SessionOps::Reply &reply, QString *err) {
        if (reply.success) return true;
        if (err) *err = reply.message;
        return false;
    }
};

// ---------------------------------------------------------------------------

QString SessionStore::defaultSessionsDir() {
    return QDir::homePath() + QStringLiteral("/moilcali_sessions");
}

SessionStore::SessionStore(CameraDevice *camera, QString sessionsDir, QObject *parent)
    : QObject(parent), d_(new Impl) {
    d_->camera = camera;
    d_->root = std::move(sessionsDir);
    QDir().mkpath(d_->root);
    d_->registry = std::make_unique<SessionRegistry>(d_->root);
}

SessionStore::~SessionStore() = default;

QString SessionStore::sessionsDir() const { return d_->root; }

QString SessionStore::sessionId() const { return d_->currentId; }

// ------------------------------------------------------------------ lifecycle

QVector<SessionStore::Summary> SessionStore::list(QString *err) {
    QVector<Summary> out;
    const SessionOps::Reply reply = SessionOps::command(
        *d_->registry, QStringLiteral("list"), QString(), QString(), Impl::nowSeconds(),
        nullptr);
    if (!Impl::finish(reply, err)) return out;

    const QJsonObject result =
        QJsonDocument::fromJson(reply.result.toUtf8()).object();
    for (const QJsonValue &v : result.value(QStringLiteral("sessions")).toArray()) {
        const QJsonObject o = v.toObject();
        Summary s;
        s.id = o.value(QStringLiteral("id")).toString();
        s.name = o.value(QStringLiteral("name")).toString();
        s.createdAt = static_cast<qint64>(o.value(QStringLiteral("created_at")).toDouble());
        s.version = static_cast<quint64>(o.value(QStringLiteral("version")).toDouble());
        for (const QJsonValue &c : o.value(QStringLiteral("captures")).toArray())
            s.captures << c.toString();
        out.append(s);
    }
    return out;
}

bool SessionStore::create(const QString &name, QString *err) {
    QJsonObject payload;
    payload[QStringLiteral("name")] = name;

    const SessionOps::Reply reply = SessionOps::command(
        *d_->registry, QStringLiteral("create"), QString(), dump(payload), Impl::nowSeconds(),
        [this](const QString &id, quint64 version, const QString &kind, const QString &detail) {
            emit sessionChanged(id, version, kind, detail);
        });
    if (!Impl::finish(reply, err)) return false;

    d_->currentId = reply.sessionId;
    return true;
}

bool SessionStore::open(const QString &id, QString *err) {
    const SessionOps::Reply reply = SessionOps::command(
        *d_->registry, QStringLiteral("open"), id, QString(), Impl::nowSeconds(),
        [this](const QString &sid, quint64 version, const QString &kind, const QString &detail) {
            emit sessionChanged(sid, version, kind, detail);
        });
    if (!Impl::finish(reply, err)) return false;

    d_->currentId = reply.sessionId;
    return true;
}

bool SessionStore::close(QString *err) {
    if (d_->currentId.isEmpty()) return true;

    const QString id = d_->currentId;
    const SessionOps::Reply reply = SessionOps::command(
        *d_->registry, QStringLiteral("close"), id, QString(), Impl::nowSeconds(),
        [this](const QString &sid, quint64 version, const QString &kind, const QString &detail) {
            emit sessionChanged(sid, version, kind, detail);
        });
    // Forget it either way. A close that failed to SAVE still leaves nothing this
    // object should keep pointing at, and holding a stale id is how the next
    // command ends up working against a session that is half gone.
    d_->currentId.clear();
    return Impl::finish(reply, err);
}

bool SessionStore::rename(const QString &name, QString *err) {
    QJsonObject payload;
    payload[QStringLiteral("name")] = name;
    return Impl::finish(
        SessionOps::command(*d_->registry, QStringLiteral("rename"), d_->currentId,
                            dump(payload), Impl::nowSeconds(),
                            [this](const QString &sid, quint64 v, const QString &kind,
                                   const QString &detail) {
                                emit sessionChanged(sid, v, kind, detail);
                            }),
        err);
}

bool SessionStore::save(QString *err) {
    return Impl::finish(
        SessionOps::command(*d_->registry, QStringLiteral("save"), d_->currentId, QString(),
                            Impl::nowSeconds(),
                            [this](const QString &sid, quint64 v, const QString &kind,
                                   const QString &detail) {
                                emit sessionChanged(sid, v, kind, detail);
                            }),
        err);
}

bool SessionStore::remove(const QString &id, QString *err) {
    // The handler refuses without this, on purpose: it takes hours of calibration
    // and its captures with it. Setting it here is this method's whole contract --
    // its own documentation says the caller has already asked the operator.
    QJsonObject payload;
    payload[QStringLiteral("confirm")] = true;

    const SessionOps::Reply reply = SessionOps::command(
        *d_->registry, QStringLiteral("delete"), id, dump(payload), Impl::nowSeconds(),
        [this](const QString &sid, quint64 v, const QString &kind, const QString &detail) {
            emit sessionChanged(sid, v, kind, detail);
        });
    if (!Impl::finish(reply, err)) return false;

    if (d_->currentId == id) d_->currentId.clear();
    return true;
}

bool SessionStore::undo(QString *err) {
    return Impl::finish(
        SessionOps::command(*d_->registry, QStringLiteral("undo"), d_->currentId, QString(),
                            Impl::nowSeconds(),
                            [this](const QString &sid, quint64 v, const QString &kind,
                                   const QString &detail) {
                                emit sessionChanged(sid, v, kind, detail);
                            }),
        err);
}

bool SessionStore::redo(QString *err) {
    return Impl::finish(
        SessionOps::command(*d_->registry, QStringLiteral("redo"), d_->currentId, QString(),
                            Impl::nowSeconds(),
                            [this](const QString &sid, quint64 v, const QString &kind,
                                   const QString &detail) {
                                emit sessionChanged(sid, v, kind, detail);
                            }),
        err);
}

// ---------------------------------------------------------------------- edits

bool SessionStore::applyEdits(const QVector<CellEdit> &cells,
                              const QHash<QString, QString> &fields, QString *err) {
    QJsonArray cellArray;
    for (const CellEdit &c : cells) {
        QJsonObject o;
        o[QStringLiteral("round")] = c.round;
        o[QStringLiteral("row")] = c.row;
        o[QStringLiteral("col")] = c.col;
        o[QStringLiteral("text")] = c.text;
        cellArray.append(o);
    }

    QJsonObject fieldObject;
    for (auto it = fields.constBegin(); it != fields.constEnd(); ++it)
        fieldObject[it.key()] = it.value();

    QJsonObject edits;
    edits[QStringLiteral("cells")] = cellArray;
    edits[QStringLiteral("fields")] = fieldObject;

    return Impl::finish(
        SessionOps::edit(*d_->registry, d_->currentId, dump(edits),
                         [this](const QString &sid, quint64 v, const QString &kind,
                                const QString &detail) {
                             emit sessionChanged(sid, v, kind, detail);
                         }),
        err);
}

// ---------------------------------------------------------------------- table

const CaliTableData &SessionStore::mirror() const {
    const CaliSession *session = d_->current();
    return session ? session->table() : emptyTable();
}

quint64 SessionStore::mirrorVersion() const {
    const CaliSession *session = d_->current();
    return session ? session->version() : 0;
}

// ------------------------------------------------------------------- captures

bool SessionStore::capture(const QString &slot, double timeoutSec, QString *err) {
    if (!d_->camera) {
        if (err) *err = QStringLiteral("no camera is attached to this session store");
        return false;
    }
    if (d_->currentId.isEmpty()) {
        if (err) *err = QStringLiteral("no session is open");
        return false;
    }

    // CameraDevice::single_image() already waits for a frame that arrives after
    // the call and gives up after a second. Retry until the caller's deadline so a
    // camera that is mid-reopen -- which takes longer than one frame interval --
    // does not fail a capture the operator will just retry by hand.
    QElapsedTimer clock;
    clock.start();
    const qint64 deadlineMs = static_cast<qint64>(timeoutSec * 1000.0);

    QByteArray encoded;
    for (;;) {
        encoded = d_->camera->single_image();
        if (!encoded.isEmpty()) break;
        if (clock.elapsed() >= deadlineMs) {
            if (err)
                *err = QStringLiteral("no frame from %1 within %2 s")
                           .arg(d_->camera->url(), QString::number(timeoutSec, 'f', 1));
            return false;
        }
        QThread::msleep(50);
    }

    int width = 0, height = 0;
    return Impl::finish(
        SessionOps::putCapture(*d_->registry, d_->currentId, slot, encoded, &width, &height,
                               [this](const QString &sid, quint64 v, const QString &kind,
                                      const QString &detail) {
                                   emit sessionChanged(sid, v, kind, detail);
                               }),
        err);
}

QByteArray SessionStore::captureBytes(const QString &slot, int maxSide, QSize *originalSize,
                                      QString *err) {
    QByteArray bytes;
    int width = 0, height = 0;

    const SessionOps::Reply reply =
        SessionOps::image(*d_->registry, d_->currentId, slot, maxSide, &bytes, &width, &height);
    if (!Impl::finish(reply, err)) return {};

    if (originalSize) *originalSize = QSize(width, height);
    return bytes;
}

QImage SessionStore::captureImage(const QString &slot, int maxSide, QString *err) {
    const QByteArray bytes = captureBytes(slot, maxSide, nullptr, err);
    if (bytes.isEmpty()) return {};

    QImage image;
    if (!image.loadFromData(bytes)) {
        if (err) *err = QStringLiteral("capture \"%1\" could not be decoded").arg(slot);
        return {};
    }
    return image;
}

// -------------------------------------------------------------------- compute

bool SessionStore::runCompute(const QString &op, const QString &params, QString *result,
                              QString *err, const ProgressFn &progress) {
    // The op runs here, on the caller's thread. Under ROS this was an action so
    // the work would outlive a client that walked away; there is no separate
    // client now, so what the action bought is gone and what it cost -- feedback
    // messages for a progress bar in another process -- is gone with it. Cancel
    // still works the same way: return false from `progress`.
    ComputeOps::ProgressFn forwarded;
    if (progress)
        forwarded = [&progress](int done, int total, const QString &stage) {
            return progress(done, total, stage);
        };

    const SessionOps::Reply reply = SessionOps::runCompute(
        *d_->registry, d_->currentId, op, params, forwarded,
        [this](const QString &sid, quint64 v, const QString &kind, const QString &detail) {
            emit sessionChanged(sid, v, kind, detail);
        });

    if (result) *result = reply.result;
    return Impl::finish(reply, err);
}
