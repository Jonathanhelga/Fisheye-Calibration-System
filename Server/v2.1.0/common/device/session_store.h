#pragma once

#include <functional>
#include <memory>

#include <QByteArray>
#include <QHash>
#include <QImage>
#include <QObject>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVector>

#include "CaliTableData.h"

class CameraDevice;

// The calibration sessions, on this machine.
//
// This is SessionClient with the ROS action, the seven services and the mirror
// removed. What is left is not a smaller version of it -- it is the thing the
// mirror was a copy OF. mirror() still exists, and still returns a
// CaliTableData, but it now hands back the session's own table rather than a
// snapshot that could be stale, so refresh() has nothing to fetch and always
// succeeds. Callers written against SessionClient keep working; they simply stop
// being able to be out of date.
//
// The three properties the rig-owned session was built for survive the merge,
// because they were properties of the STORE and not of the transport:
//
//   * work outlives the app -- sessions are directories under sessionsDir()
//   * captures are never uploaded -- the camera is right here
//   * progress and Cancel work -- runCompute() takes the same ProgressFn, which
//     is now called on the caller's own thread instead of arriving as action
//     feedback
//
// What is gone is a second client seeing the first one's work. That was the one
// thing only two processes could do, and merging them is what was asked for.
class SessionStore : public QObject {
    Q_OBJECT

public:
    // `camera` is borrowed, not owned, and may be null -- then capture() fails
    // with that as its reason rather than the app refusing to start.
    explicit SessionStore(CameraDevice *camera, QString sessionsDir = defaultSessionsDir(),
                          QObject *parent = nullptr);
    ~SessionStore() override;

    SessionStore(const SessionStore &) = delete;
    SessionStore &operator=(const SessionStore &) = delete;

    // ~/moilcali_sessions, the same path the rig's node defaulted to, so an
    // existing store is picked up without being told where it is.
    static QString defaultSessionsDir();
    QString sessionsDir() const;

    // The store is part of this process; there is nothing to be unavailable.
    // Kept because callers asked before every action.
    bool available() const { return true; }

    // ---- lifecycle ---------------------------------------------------------
    struct Summary {
        QString id;
        QString name;
        qint64 createdAt = 0;
        quint64 version = 0;
        QStringList captures;
    };

    // Sessions on disk, newest first -- including ones made before this launch.
    QVector<Summary> list(QString *err);

    bool create(const QString &name, QString *err);
    bool open(const QString &id, QString *err);
    bool close(QString *err);
    bool rename(const QString &name, QString *err);
    bool save(QString *err);
    // Irreversible. The caller must already have asked the operator.
    bool remove(const QString &id, QString *err);

    bool undo(QString *err);
    bool redo(QString *err);

    QString sessionId() const;
    bool hasSession() const { return !sessionId().isEmpty(); }

    // ---- edits -------------------------------------------------------------
    struct CellEdit {
        int round = 0;
        int row = 0;
        int col = 0;
        QString text;
    };

    // One call is ONE undo step. Send a paste as one call, or the operator needs
    // forty Ctrl+Z to take it back.
    bool applyEdits(const QVector<CellEdit> &cells, const QHash<QString, QString> &fields,
                    QString *err);
    bool setField(const QString &name, const QString &text, QString *err) {
        return applyEdits({}, {{name, text}}, err);
    }

    // ---- the table ---------------------------------------------------------
    // The session's own table. Empty when nothing is open.
    const CaliTableData &mirror() const;
    quint64 mirrorVersion() const;

    // Nothing to fetch: the table above IS the state. Kept so callers that
    // refresh after an event still compile, and still correct.
    bool refresh(QString *) { return true; }

    // ---- captures ----------------------------------------------------------
    // Grabs a frame from the camera into `slot`. No upload: the camera is on this
    // machine, which is what the rig-side capture was for in the first place.
    bool capture(const QString &slot, double timeoutSec, QString *err);

    // Fetch a capture to display. maxSide shrinks it first; pass 0 for the
    // original.
    //
    // Decodes. Use captureBytes() for anything that is going straight back out
    // over a wire -- this one costs a decode the caller then pays for again on
    // the way out, and at 3040x3040 that re-encode is where the original stopped
    // being deliverable.
    QImage captureImage(const QString &slot, int maxSide, QString *err);

    // The capture as ENCODED BYTES. With maxSide 0 -- or any maxSide at or above
    // the frame -- these are the stored bytes exactly as the shutter produced
    // them: no decode, no re-encode, no size cap. Only a request that genuinely
    // shrinks the frame pays for an encode, and then it is the small one.
    //
    // This is what a client asking for "the original" has to receive. Handing it
    // a QImage instead meant the caller re-encoded, and a 3040x3040 re-encode to
    // PNG turned a ~1.5 MB stored JPEG into ~16 MB, which is more than the
    // service transport carries -- the request hung with both ends silent, and
    // the client fell back to a 2048 px copy without anyone being told.
    //
    // `originalSize` is the size of the STORED frame, which is not the size of
    // the returned bytes when maxSide shrank them.
    QByteArray captureBytes(const QString &slot, int maxSide, QSize *originalSize, QString *err);

    // ---- compute -----------------------------------------------------------
    // Blocking, with progress and a working Cancel: return false from `progress`
    // to cancel. Runs on the calling thread, so call it from a worker if the
    // caller is the GUI.
    using ProgressFn = std::function<bool(int done, int total, const QString &stage)>;
    bool runCompute(const QString &op, const QString &params, QString *result, QString *err,
                    const ProgressFn &progress = nullptr);

signals:
    // A session changed. Emitted for every mutation, as SessionEvent was, so
    // views that connected to it keep updating.
    void sessionChanged(const QString &sessionId, quint64 version, const QString &kind,
                        const QString &detail);

private:
    struct Impl;
    std::unique_ptr<Impl> d_;
};
