// Solving for a distance: the three searches the CaliJob action drives, and the
// per-round and all-round scoring they minimise.
//
// See CaliCompute_p.h for what is in the other four files.

#include "CaliCompute_p.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <utility>
#include <vector>

#include "moilcali_algorithm.h"
#include "Regression.h"

using namespace cali_detail;

void CaliCompute::calculateResultSingleRound(int i, double distance) {
    updateSideLayer(i);
    updateRoundNum(i);
    remove45SideData(i);

    clearColumn(i, "ict_avg");
    updateIctAvg(i);
    clearColumn(i, "pct_cal");
    updatePctCal(i);

    // Single-round distance: fill the whole distance column with `distance`.
    clearColumn(i, "distance");
    {
        const int col = colIndex("distance");
        const int rowsOfLayers = maxLayer(i, 40);
        for (int layer = 0; layer < rowsOfLayers; ++layer)
            setCell(i, rowByLayer(layer), col, distance);
    }

    for (const QString &d : dirs8()) {
        clearColumn(i, "alpha_" + d);
        clearColumn(i, "zfl_" + d);
    }
    updateAlpha8(i);
    updateZfl8(i);

    clearColumn(i, "alpha_avg");
    updateAlphaAvg(i);
    clearColumn(i, "zfl_avg");
    updateZflAvg(i);

    updateAggregationSingleRound(i);
}

double CaliCompute::aggregationByDistance(int i, double distance) {
    calculateResultSingleRound(i, distance);
    QVector<double> xs, ys;
    ictZflXY(i, xs, ys);
    if (xs.isEmpty() || ys.isEmpty()) return 1e300;
    return aggregationTotal(xs, ys);
}

void CaliCompute::calculateResultWithBaseDistance(int i, double baseDistance) {
    updateSideLayer(i);
    updateRoundNum(i);
    remove45SideData(i);
    clearColumn(i, "ict_avg");
    updateIctAvg(i);
    clearColumn(i, "pct_cal");
    updatePctCal(i);
    clearColumn(i, "distance");
    updateDistance(i, baseDistance);  // base + dis_per_round*(round - firstValid)
    for (const QString &d : dirs8()) { clearColumn(i, "alpha_" + d); clearColumn(i, "zfl_" + d); }
    updateAlpha8(i);
    updateZfl8(i);
    clearColumn(i, "alpha_avg");
    updateAlphaAvg(i);
    clearColumn(i, "zfl_avg");
    updateZflAvg(i);
    updateAggregationSingleRound(i);
}

double CaliCompute::maxIctAllRounds() {
    double m = 0;
    bool any = false;
    for (int r = 0; r <= 10; ++r) {
        if (!isRoundEnabled(r)) continue;  // _collect_enabled_ih_max_px
        QVector<double> xs, ys;
        ictZflXY(r, xs, ys);
        for (double x : xs) { if (!any || x > m) m = x; any = true; }
    }
    return any ? m : 0;
}

QVector<QPointF> CaliCompute::globalIctAlpha() {
    QVector<QPointF> out;
    for (int r = 0; r <= 10; ++r) {
        if (!isRoundEnabled(r)) continue;
        if (!hasTable(r)) continue;
        updateSideLayer(r);
        const int side = sideLayer(r);
        const int maxLayer = this->maxLayer(r, 0);
        for (int layer = 0; layer < maxLayer; ++layer) {
            const int row = rowByLayer(layer);
            const auto add = [&](const QString &ictCol, const QString &alphaCol) {
                double ict = 0, alpha = 0;
                if (isFloat(cell(r, row, colIndex(ictCol)), &ict) &&
                    isFloat(cell(r, row, colIndex(alphaCol)), &alpha))
                    out.append(QPointF(roundTo2(ict), alpha * 180.0 / M_PI));  // alpha -> deg
            };
            if (layer < side) {
                add("ict_avg", "alpha_avg");
            } else {
                for (const QString &d : dirsXY()) add("ict_" + d, "alpha_" + d);
            }
        }
    }
    return out;
}

CaliCompute::MinAggr CaliCompute::findMinAggregationInWindow(bool useWindow, double xLo, double xHi) {
    MinAggr r;
    const double DLO = 1, DHI = 500;
    const auto probe = [&](double d) { return aggregationAllRoundsByDistance(d, useWindow, xLo, xHi); };
    bool canceled = false;
    for (int i = 0; i <= 120; ++i) {
        if (progress_ && !progress_(i, 182)) { canceled = true; break; }
        const double d = DLO + (DHI - DLO) * (i / 120.0);
        const double a = probe(d);
        if (!std::isfinite(a)) continue;
        r.samples.append(QPointF(d, a));
        if (a < r.bestAggr) { r.bestAggr = a; r.bestDistance = d; }
    }
    if (!canceled && r.bestAggr < 1e299) {  // local refine around the coarse minimum
        const double half = std::max((DHI - DLO) * 0.02, 5.0);
        const double lo = std::max(DLO, r.bestDistance - half);
        const double hi = std::min(DHI, r.bestDistance + half);
        for (int i = 0; i <= 60; ++i) {
            if (progress_ && !progress_(121 + i, 182)) break;
            const double d = lo + (hi - lo) * (i / 60.0);
            const double a = probe(d);
            if (!std::isfinite(a)) continue;
            r.samples.append(QPointF(d, a));
            if (a < r.bestAggr) { r.bestAggr = a; r.bestDistance = d; }
        }
    }
    return r;
}

CaliCompute::MinAggr CaliCompute::findDistanceForTargetAggregation(double target, bool useRange,
                                                                   double xLo, double xHi) {
    MinAggr r;
    const double DLO = 1.0, DHI = 500.0;
    const auto probe = [&](double d) { return aggregationAllRoundsByDistance(d, useRange, xLo, xHi); };

    // bestErr is tracked separately from r.bestAggr: the search minimises the
    // DISTANCE FROM the target, but what the caller displays is the aggregation
    // actually reached. Folding them into one field would report the error in the
    // aggregation box.
    double bestErr = 1e300, bestD = 0, bestA = 0;
    bool any = false, canceled = false;
    for (int i = 0; i <= 200; ++i) {
        if (progress_ && !progress_(i, 282)) { canceled = true; break; }
        const double d = DLO + (DHI - DLO) * (i / 200.0);
        const double a = probe(d);
        if (!std::isfinite(a)) continue;
        r.samples.append(QPointF(d, a));
        const double err = std::abs(a - target);
        if (err < bestErr) { bestErr = err; bestD = d; bestA = a; any = true; }
    }
    if (any && !canceled) {
        const double half = std::max((DHI - DLO) * 0.02, 5.0);
        const double lo = std::max(DLO, bestD - half), hi = std::min(DHI, bestD + half);
        for (int i = 0; i <= 80; ++i) {
            if (progress_ && !progress_(201 + i, 282)) break;
            const double d = lo + (hi - lo) * (i / 80.0);
            const double a = probe(d);
            if (!std::isfinite(a)) continue;
            r.samples.append(QPointF(d, a));
            const double err = std::abs(a - target);
            if (err < bestErr) { bestErr = err; bestD = d; bestA = a; }
        }
    }
    if (any) {
        r.bestDistance = bestD;
        r.bestAggr = bestA;
    }
    return r;
}

double CaliCompute::aggregationAllRoundsByDistance(double base, bool useRange, double xLo,
                                                   double xHi) {
    QVector<double> tx, ty;
    for (int page = 0; page <= 10; ++page) {
        calculateResultWithBaseDistance(page, base);
        QVector<double> xs, ys;
        ictZflXY(page, xs, ys);
        const int n = std::min(xs.size(), ys.size());
        for (int k = 0; k < n; ++k) { tx.append(xs[k]); ty.append(ys[k]); }
    }
    if (tx.isEmpty()) return 1e300;
    if (!useRange) return aggregationTotal(tx, ty);
    // separate-range: keep pairs with x in [xLo, xHi], sorted, sum gaps.
    QVector<QPointF> pts;
    for (int k = 0; k < tx.size(); ++k)
        if (tx[k] >= xLo && tx[k] <= xHi) pts.append(QPointF(tx[k], ty[k]));
    std::sort(pts.begin(), pts.end(), [](const QPointF &a, const QPointF &b) {
        return a.x() != b.x() ? a.x() < b.x() : a.y() < b.y();
    });
    double agg = 0;
    for (int k = 0; k + 1 < pts.size(); ++k) {
        const double dx = pts[k].x() - pts[k + 1].x();
        const double dy = pts[k].y() - pts[k + 1].y();
        agg += std::sqrt(dx * dx + dy * dy);
    }
    return agg;
}

CaliCompute::MinAggr CaliCompute::findMinAggrSingleRound(int i, double distMin, double distMax,
                                                        int maxIter, double tol) {
    MinAggr r;
    long a = std::max<long>(250, long(distMin));
    long b = long(distMax);
    for (int it = 0; it < maxIter; ++it) {
        if (progress_ && !progress_(it, maxIter)) break;
        if (b - a <= tol) break;
        const long m1 = a + (b - a) / 3;
        const long m2 = b - (b - a) / 3;
        const double f1 = aggregationByDistance(i, double(m1));
        const double f2 = aggregationByDistance(i, double(m2));
        r.samples.append(QPointF(double(m1), f1));
        r.samples.append(QPointF(double(m2), f2));
        if (f1 < r.bestAggr) { r.bestAggr = f1; r.bestDistance = double(m1); }
        if (f2 < r.bestAggr) { r.bestAggr = f2; r.bestDistance = double(m2); }
        if (f1 < f2) b = m2 - 1;
        else a = m1 + 1;
    }
    return r;
}
