#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <vector>

#include <QString>

#include "CaliSession.h"

// The rig's set of calibration sessions: which exist, which are loaded, and where
// they live on disk.
//
// Separate from CaliSession because it is about the store, not the calibration:
// directory layout, id generation, and the lock that keeps two concurrent service
// callbacks from mutating one session at once.
class SessionRegistry {
public:
    // `root` is the directory that holds one subdirectory per session.
    explicit SessionRegistry(QString root) : root_(std::move(root)) {}

    struct Summary {
        QString id;
        QString name;
        qint64 createdAt = 0;
        quint64 version = 0;
        QStringList captures;
    };

    // Every session on disk, newest first. Reads each session.json -- there are
    // tens of these, not thousands, and a stale in-memory index would be worse than
    // the directory scan: a session another process wrote would be invisible.
    std::vector<Summary> list() const;

    // Creates a session and makes it resident. `nowSeconds` is passed in rather than
    // read from the clock so the caller owns time (and a test can fix it).
    CaliSession *create(const QString &name, qint64 nowSeconds, QString *err);

    // Loads from disk if not already resident. Returns null with *err set.
    CaliSession *open(const QString &id, QString *err);

    // Resident session, or null. Does NOT load from disk: a caller that means "open
    // it" should say so, or a typo'd id would silently create work against a
    // freshly loaded session instead of failing.
    CaliSession *resident(const QString &id);

    // Saves and drops from memory. Not an error if it was never resident.
    bool close(const QString &id, QString *err);

    bool save(const QString &id, QString *err);

    // Removes the directory. Irreversible -- an hours-long calibration and its
    // captures. The caller is responsible for having been asked to.
    bool remove(const QString &id, QString *err);

    QString dirFor(const QString &id) const { return root_ + "/" + id; }
    const QString &root() const { return root_; }

    // Held across a whole request, so an edit and the state read that follows it
    // cannot interleave with a compute op writing derived columns. Compute ops are
    // already serialised by ComputeOps::CleaningScope; this covers the rest.
    std::mutex &mutex() { return mutex_; }

private:
    // Sortable and human-readable: "20260807-103045-a1b2". Sortable matters because
    // list() orders by it, and readable matters because it is also the directory
    // name an operator will be looking at over SSH.
    QString makeId(qint64 nowSeconds) const;

    QString root_;
    std::map<QString, std::unique_ptr<CaliSession>> loaded_;
    std::mutex mutex_;
};
