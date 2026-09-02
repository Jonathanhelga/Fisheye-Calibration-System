#pragma once

#include <QString>

// Where CaliCompute reads and writes.
//
// The compute pipeline used to reach into the Cali Result form itself
// (content_->findChild<QTableWidget>("tablewidget_3")), so the formulas were
// welded to a live widget tree -- and therefore to the client process, which is
// the only place a widget tree exists. Behind this interface the same formulas
// run against plain memory, which is what lets the rig's compute node serve them
// headless.
//
// It also removes the reason the distance searches were slow. Every probe in
// find_min_aggregation_by_lineedit recomputes all 11 rounds, and there are 182
// probes: ~2000 full round recomputes, each of which was writing a
// QTableWidgetItem per cell and emitting a change signal per write. Against a
// plain array those writes are stores.
//
// Rounds are indexed 0..10 (0 is the "current" scratch round). Rows are
// physical: row 1 holds the per-round metadata (side_layer, round number) and
// layer L lives on row L+2 -- CaliCompute owns that mapping, not this interface.
class CaliDataSource {
public:
    virtual ~CaliDataSource() = default;

    // False when the round has no backing table at all. Kept separate from
    // rowCount() because the callers that tolerate a missing round substitute a
    // layer count of their own (40) rather than treating it as zero rows, and
    // collapsing the two would silently change which layers get written.
    virtual bool hasRound(int round) const = 0;

    virtual int rowCount(int round) const = 0;
    virtual int columnCount(int round) const = 0;

    // Empty string for an absent cell or an out-of-range index.
    virtual QString cell(int round, int row, int col) const = 0;

    // Stores `text` verbatim; out-of-range writes are dropped. The
    // "numeric text only" rule lives in CaliCompute::setCell, not here, because
    // the '*' side-layer marker is written deliberately through this path.
    virtual void setCell(int round, int row, int col, const QString &text) = 0;

    // Named scalar inputs (the form's line-edits: pixel sizes, gaps, distances).
    virtual bool hasField(const QString &name) const = 0;
    virtual double field(const QString &name, double def) const = 0;
    virtual void setFieldText(const QString &name, const QString &text) = 0;
};
