#pragma once

#include <memory>
#include <vector>

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QStringList>

#include <opencv2/core.hpp>

#include "CaliTableData.h"

// One calibration session, owned by the rig.
//
// This is the piece that makes the client a control surface rather than a
// participant. Before it, the client held the data model -- the QTableWidget cells
// WERE the model -- and shipped the whole thing to the rig on every call. The rig
// computed and forgot. So the rig could not answer "what has been calibrated", the
// work was lost if the client crashed, and two clients could not look at the same
// calibration.
//
// Now the rig holds the table, the parameters and the captures, and the client
// mirrors them for display. Every mutation bumps version(): that is how a client
// knows its mirror is stale without diffing 16k cells, and how a client that was
// not running when the work happened can pick it up afterwards.
//
// Deliberately free of ROS and of Qt Widgets. It is the session, not the service.
class CaliSession {
public:
    // Capture slots, named as the client's files were, so an operator reading a log
    // recognises them. These are the images the analysis ops consume.
    static const QStringList &captureNames();

    CaliSession(QString id, QString name) : id_(std::move(id)), name_(std::move(name)) {
        for (int r = 0; r <= 10; ++r) table_.addRound(r);
    }

    const QString &id() const { return id_; }
    const QString &name() const { return name_; }
    void setName(QString n) { name_ = std::move(n); bump(); }

    // Monotonic, bumped by every mutation. Never reset, including by undo -- undo
    // is a change like any other, and a client that saw version 7, undid, and saw 7
    // again would conclude nothing had happened.
    quint64 version() const { return version_; }

    // Seconds since the epoch, supplied by the caller rather than read from the
    // clock here so a restored session keeps its original timestamp.
    void setCreatedAt(qint64 t) { createdAt_ = t; }
    qint64 createdAt() const { return createdAt_; }

    CaliTableData &table() { return table_; }
    const CaliTableData &table() const { return table_; }

    // ---- edits (the client's only way to change anything) -------------------
    struct CellEdit {
        int round = 0;
        int row = 0;
        int col = 0;
        QString text;
    };
    struct FieldEdit {
        QString name;
        QString text;
    };

    // Applies both lists as ONE undo step. A paste of 40 cells has to undo as one
    // action, not forty, and typing one cell is the same code path with one entry.
    // Returns the new version.
    quint64 applyEdits(const std::vector<CellEdit> &cells, const std::vector<FieldEdit> &fields);

    bool canUndo() const { return !undo_.empty(); }
    bool canRedo() const { return !redo_.empty(); }
    bool undo();
    bool redo();

    // A compute op has rewritten the table in place (through table()). Bumps the
    // version so every mirror learns to refetch.
    //
    // No undo step is recorded, deliberately. Undo is for operator INPUT; the
    // columns a compute op writes are derived, and pressing Update again
    // regenerates them. Recording them would also mean a single Ctrl+Z had to carry
    // thousands of cells, and would let an operator "undo" into a table whose
    // derived columns no longer match its inputs.
    //
    // The redo branch IS dropped: those entries were computed against a table state
    // that no longer exists.
    quint64 markComputed();

    // ---- captures ----------------------------------------------------------
    // Encoded bytes exactly as the camera published them. Stored encoded, not as a
    // Mat: it is what gets written to disk and handed back to the client to display,
    // and decoding is only needed by the analysis ops.
    void putCapture(const QString &name, QByteArray encoded);
    QByteArray capture(const QString &name) const { return captures_.value(name); }
    bool hasCapture(const QString &name) const { return captures_.contains(name); }
    QStringList presentCaptures() const;

    // Decoded and cached. Returns an empty Mat if the slot is empty or undecodable.
    // The cache is what stops a 3040x3040 PNG being decoded again for every op in a
    // sequence -- and the decode is a measurable part of a 294 ms detection.
    cv::Mat captureMat(const QString &name) const;

    // ---- persistence -------------------------------------------------------
    // Layout under `dir`:
    //   <dir>/session.json          the table, the fields, the metadata
    //   <dir>/captures/<name>.png   one file per occupied capture slot
    //
    // Captures live beside the JSON rather than base64 inside it: they are ~7 MB
    // each, and a session.json a human cannot open is a session.json nobody checks.
    bool save(const QString &dir, QString *err) const;
    static std::unique_ptr<CaliSession> load(const QString &dir, QString *err);

    // What the client mirrors. Contains the table, the fields, the metadata and
    // WHICH capture slots are filled -- never the image bytes, which are fetched
    // separately and only when something is actually going to show them.
    QString stateJson() const;

private:
    void bump() { ++version_; }

    // Undo/redo entry: the values as they were BEFORE the edit, so applying it is
    // the same operation as making an edit.
    struct Step {
        std::vector<CellEdit> cells;
        std::vector<FieldEdit> fields;
    };
    // Swaps `step` with the current values, so the caller can push the result onto
    // the opposite stack. This is what makes redo fall out of undo for free.
    Step applyStep(const Step &step);

    QString id_;
    QString name_;
    qint64 createdAt_ = 0;
    quint64 version_ = 0;

    CaliTableData table_;
    QHash<QString, QByteArray> captures_;
    mutable QHash<QString, cv::Mat> matCache_;

    std::vector<Step> undo_;
    std::vector<Step> redo_;

    // Bounded, because the stacks hold table cells and a long session would grow
    // without limit. 200 steps is far past what an operator undoes in practice.
    static constexpr size_t kMaxUndo = 200;
};
