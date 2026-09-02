#pragma once

#include <functional>
#include <map>
#include <string>
#include <vector>

#include <QColor>
#include <QPointF>
#include <QString>
#include <QVector>

#include "CaliDataSource.h"

// Port of the calibration-result compute pipeline in controller_cali_result.py.
// Operates on the 11 round tables and the form's scalar inputs through
// CaliDataSource, so the same formulas run against the client's QTableWidgets
// (CaliWidgetSource) and against plain memory on the rig's compute node
// (CaliTableData). Row for layer L is L+2; row 1 (layer -1) holds the per-round
// side_layer and round number metadata.
class CaliCompute {
public:
    // `src` must outlive the CaliCompute; it is the caller's data model, not a
    // copy of it -- the pipeline writes derived columns straight back into it.
    explicit CaliCompute(CaliDataSource *src) : src_(src) {}

    // Per-round enable status (rounds 1..10; round 0 "current" is always on).
    // Ports _round_enabled_status: disabled rounds are skipped by computeAll,
    // the plots and the cross-round aggregation helpers.
    void setRoundEnabled(int round, bool enabled) {
        if (round >= 1 && round <= 10) roundEnabled_[round] = enabled;
    }
    bool isRoundEnabled(int round) const {
        return (round < 1 || round > 10) ? true : roundEnabled_[round];
    }

    // Progress callback for long searches: called as (done, total); return
    // false to cancel. Set to nullptr to disable. Used by the distance searches.
    void setProgress(std::function<bool(int, int)> cb) { progress_ = std::move(cb); }

    // cb_distance mode: when true, computeAll fills each round's distance from
    // its per-round line-edit (lineedit_distance_round_i) instead of the global
    // base + dis_per_round formula. Ports _is_use_single_round_distance.
    void setUseSingleRoundDistance(bool on) { useSingleRoundDistance_ = on; }

    // calculate_result(table_index): side_layer, round_num, remove 45deg side
    // data, ict_avg, pct_cal, distance (fill only if empty), alpha/zfl 8-dir,
    // alpha_avg/zfl_avg, aggregation line-edit.
    void calculateResult(int tableIndex);

    // onclick_btn_update_all_cali_result: compute rounds 1..10.
    void computeAll();

    // True if any round table holds at least one numeric raw ict_* value, i.e.
    // there is calibration data worth (re)computing. Cheap; reads raw columns
    // only (no derived zfl), so it is valid *before* computeAll() runs.
    bool hasAnyRawIct() const;

    // Populate a round from a fresh capture: clear it, set round number, write
    // the PCT column from the 75 pattern values, and the ict_* 8-direction
    // columns from a MoilCali detection dict. Ports update_column_pct +
    // update_ict_8direction. (Detection itself is run by the caller.)
    void updateTableFromCapture(int tableIndex, const QVector<QString> &pctList,
                                const std::map<std::string, std::vector<double>> &ict8dir);

    // ---- Per-round monitor-gap noise cleaning (band based) -----------------
    // The monitor's dark panel gap occupies the SAME radial band in every
    // direction, so noise is found as a "hole" in the shared ring backbone
    // rather than per point. Directions are handled in groups that share a band:
    // N&S, W&E, and the four diagonals together (chosen by the user).
    struct NoiseBand {
        QString group;      // human label, e.g. "N & S"
        QStringList dirs;   // direction keys, e.g. {"n","s"}
        double lo = 0.0;    // keep nodes <= lo (top of the lower anchor ring)
        double hi = 0.0;    // keep nodes >= hi (bottom of the upper anchor ring)
        bool found = false; // whether a band was auto-detected for this group
    };

    // Auto-detect one noise band per direction-group for a round: a real ring is
    // a radial position where a quorum of the group's directions agree (noise is
    // random per direction); the band is the largest hole between consecutive
    // real rings. Returns pre-filled (lo, hi) the caller can show and let the
    // user adjust before removal.
    QVector<NoiseBand> autoDetectNoiseBands(int tableIndex) const;

    // Blank (in place, no shifting) every node strictly between lo and hi in the
    // given direction columns of a round, then recompute ict_avg. Returns count.
    int removeNodesInBand(int tableIndex, const QStringList &dirs, double lo, double hi);

    // Recompute one round using an explicit per-round distance (fills the whole
    // distance column with `distance`), then alpha/zfl/avg + aggregation.
    // Ports calculate_result_single_round in single-round-distance mode.
    void calculateResultSingleRound(int tableIndex, double distance);

    // Aggregation of a round if its distance were `distance` (INF if no data).
    double aggregationByDistance(int tableIndex, double distance);

    struct MinAggr {
        double bestDistance = 0;
        double bestAggr = 1e300;
        QVector<QPointF> samples;  // (distance, aggregation) probed during search
    };
    // Ternary search over [max(250,distMin), distMax] minimising aggregation.
    // Ports find_min_aggr_single_round.
    MinAggr findMinAggrSingleRound(int tableIndex, double distMin, double distMax,
                                   int maxIter = 30, double tol = 1);

    // Recompute a round like calculateResult but always fill the distance column
    // from an explicit base (distance = base + dis_per_round*(round-firstValid)).
    void calculateResultWithBaseDistance(int tableIndex, double baseDistance);

    // Aggregation of ALL rounds' combined (ict, zfl) points when the base
    // distance is `baseDistance`. If useRange, only points with ict in
    // [xLo, xHi] contribute. Ports calculate_aggregation_by_distance.
    double aggregationAllRoundsByDistance(double baseDistance, bool useRange = false,
                                          double xLo = 0, double xHi = 0);

    // Largest ict value across all rounds' point lists (ih_max_px). 0 if none.
    double maxIctAllRounds();

    // (ict, alpha_degrees) points across enabled rounds — for the Range
    // subsystem's data-total count + alpha min/max per IH window.
    QVector<QPointF> globalIctAlpha();

    // Search distance in [1,500] minimising the cross-round aggregation
    // (optionally within the ict window [xLo,xHi]). Ports
    // find_min_aggregation_by_lineedit. Coarse sweep + local refine.
    MinAggr findMinAggregationInWindow(bool useWindow, double xLo = 0, double xHi = 0);

    // Search distance in [1,500] whose cross-round aggregation is CLOSEST to
    // `target` (not the minimum): 201 coarse probes then 81 in a +/-5 window
    // around the best. bestAggr is the aggregation reached, not the error.
    //
    // Lives here rather than in the controller that used to hold the loop, because
    // each probe recomputes all 11 rounds -- ~282 of them. Driving that from the
    // controller means 282 separate calls, which over a service is 282 round trips
    // and ~4 MB of table JSON each way. As one op it is one call.
    MinAggr findDistanceForTargetAggregation(double target, bool useRange = false, double xLo = 0,
                                             double xHi = 0);

    // ---- Plot data extraction (degrees for X where noted) ----
    struct Series {
        int round = 0;
        QVector<QPointF> pts;
        QColor color;
    };
    // IH-ZFL / Overlap: (x=ict, y=zfl) per round.
    QVector<Series> ictZflSeries();
    // (x=ict, y=zfl) for a single round, ignoring the enabled flag (used by the
    // per-round popup graphs).
    QVector<QPointF> ictZflPoints(int round);
    // IH-Alpha: (x=alpha_degrees, y=ict) per round.
    QVector<Series> ihAlphaSeries();
    // Degree-4 polynomial fit over ALL rounds' (alpha_rad, ict); returned curve
    // is (x=alpha_degrees, y=predicted ict), sorted by x. Empty if <3 points.
    QVector<QPointF> ihAlphaRegression(int degree = 4);
    // The 6 polynomial coefficients (constant..x^5 slots) for the parameter box,
    // filled from the degree-4 fit (higher slots are 0). Empty if no fit.
    QVector<double> alphaPolynomial(int degree = 4);

    static QColor roundColor(int round);

    // Round-table geometry. Public because callers outside the compute layer read
    // the same tables directly — e.g. the client's PCT Recommendation window needs
    // the PCT and ICT-avg columns — and should not have to repeat the numbers.
    static int colIndex(const QString &title);
    static int rowByLayer(int layer) { return layer + 2; }

private:
    CaliDataSource *src_;
    bool roundEnabled_[11] = {true, true, true, true, true, true,
                              true, true, true, true, true};
    bool useSingleRoundDistance_ = false;
    std::function<bool(int, int)> progress_;

    bool hasTable(int i) const { return src_ && src_->hasRound(i); }
    // Layer count of a round: the 2 header rows do not hold layer data. Callers
    // that must cope with a missing round pass their own fallback rather than
    // getting a negative count out of rowCount() == 0.
    int maxLayer(int i, int ifAbsent) const {
        return hasTable(i) ? src_->rowCount(i) - 2 : ifAbsent;
    }
    static bool isFloat(const QString &s, double *out = nullptr);

    QString cell(int i, int row, int col) const;
    void setCell(int i, int row, int col, const QString &val);
    void setCell(int i, int row, int col, double val);
    void clearColumn(int i, const QString &title);
    void clearItem(int i, int row, int col);
    double lineEdit(const QString &name, double def) const;

    // side / metadata
    int startInWhichLayer(int i) const;      // layer with '*' in round col, else -1
    void updateSideLayer(int i);             // write side_layer to row 1
    void updateRoundNum(int i);              // write i to row 1 round col
    int sideLayer(int i) const;              // read side_layer from row 1

    // derived columns
    void remove45SideData(int i);
    void updateIctAvg(int i);
    void updatePctCal(int i);
    void updateDistance(int i, double baseDistance);
    bool distanceColumnHasEmpty(int i) const;
    int firstRoundHasIct() const;
    void updateAlpha8(int i);
    void updateZfl8(int i);
    void updateAlphaAvg(int i);
    void updateZflAvg(int i);
    QString calcAlpha(int i, const QString &dir, int layer) const;
    QString calcZfl(int i, const QString &dir, int layer) const;

    // aggregation
    void updateAggregationSingleRound(int i);
    void ictZflXY(int i, QVector<double> &xs, QVector<double> &ys);

    // Collect one round's (xBase, yBase) column values: the *_avg columns for
    // top layers, per-direction *_<dir> for side layers (rounded to 2). Shared
    // by ictZflXY / the IH-Alpha series / regression / globalIctAlpha.
    void gatherRoundXY(int round, const QString &xBase, const QString &yBase,
                       QVector<double> &xs, QVector<double> &ys);
    // All enabled rounds' (alpha_rad, ict) training pairs for the regression.
    void gatherAlphaIctTraining(std::vector<double> &tx, std::vector<double> &ty);
    static double aggregationTotal(QVector<double> xs, QVector<double> ys);

    static QString avgWithoutZero(const QVector<QString> &vals);
};
