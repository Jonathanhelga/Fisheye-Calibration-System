// A capture becoming a filled round, and the aggregation that scores one.
//
// updateTableFromCapture is the long one and it is long because it is the point
// of contact between what the operator measured and what the maths expects: every
// branch in it is a shape the capture can arrive in.
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

// ---- aggregation ----------------------------------------------------------
void CaliCompute::gatherRoundXY(int r, const QString &xBase, const QString &yBase,
                                QVector<double> &xs, QVector<double> &ys) {
    if (!hasTable(r)) return;
    updateSideLayer(r);
    const int side = sideLayer(r);
    const int maxLayer = this->maxLayer(r, 0);
    const auto add = [&](QVector<double> &list, int row, const QString &col) {
        double v = 0;
        if (isFloat(cell(r, row, colIndex(col)), &v)) list.append(roundTo2(v));
    };
    for (int layer = 0; layer < maxLayer; ++layer) {
        const int row = rowByLayer(layer);
        if (layer < side) {
            add(xs, row, xBase + "_avg");
            add(ys, row, yBase + "_avg");
        } else {
            for (const QString &d : dirsXY()) {
                add(xs, row, xBase + "_" + d);
                add(ys, row, yBase + "_" + d);
            }
        }
    }
}

void CaliCompute::ictZflXY(int i, QVector<double> &xs, QVector<double> &ys) {
    gatherRoundXY(i, "ict", "zfl", xs, ys);
}

double CaliCompute::aggregationTotal(QVector<double> xs, QVector<double> ys) {
    const int n = std::min(xs.size(), ys.size());
    QVector<QPointF> pts;
    pts.reserve(n);
    for (int k = 0; k < n; ++k) pts.append(QPointF(xs[k], ys[k]));
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

void CaliCompute::updateAggregationSingleRound(int i) {
    QVector<double> xs, ys;
    ictZflXY(i, xs, ys);
    const QString name = QString("lineedit_aggregation_round_%1").arg(i);
    if (!src_ || !src_->hasField(name)) return;  // no such read-out on this form
    src_->setFieldText(name, (xs.isEmpty() || ys.isEmpty())
                                 ? QString()
                                 : QString::number(aggregationTotal(xs, ys), 'f', 3));
}

// ---- pipeline -------------------------------------------------------------
void CaliCompute::calculateResult(int i) {
    updateSideLayer(i);
    updateRoundNum(i);
    remove45SideData(i);

    clearColumn(i, "ict_avg");
    updateIctAvg(i);
    clearColumn(i, "pct_cal");
    updatePctCal(i);

    // distance: in single-round mode (cb_distance) always fill from the round's
    // own line-edit; otherwise (re)fill from the global range_0 base only when
    // the column is empty. Ports update_distance_auto.
    if (useSingleRoundDistance_) {
        clearColumn(i, "distance");
        const double d = lineEdit(QString("lineedit_distance_round_%1").arg(i), 200.0);
        const int col = colIndex("distance");
        const int rowsOfLayers = maxLayer(i, 40);
        for (int layer = 0; layer < rowsOfLayers; ++layer) setCell(i, rowByLayer(layer), col, d);
    } else if (distanceColumnHasEmpty(i)) {
        clearColumn(i, "distance");
        updateDistance(i, lineEdit("lineedit_distance_range_0", 250.0));
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

void CaliCompute::computeAll() {
    for (int i = 1; i <= 10; ++i)
        if (isRoundEnabled(i)) calculateResult(i);
}

bool CaliCompute::hasAnyRawIct() const {
    static const QStringList dirs = {"n", "s", "w", "e", "nw", "se", "sw", "ne"};
    for (int i = 1; i <= 10; ++i) {
        if (!hasTable(i)) continue;
        const int maxLayer = this->maxLayer(i, 0);
        for (const QString &d : dirs) {
            const int col = colIndex("ict_" + d);
            for (int layer = 0; layer < maxLayer; ++layer)
                if (isFloat(cell(i, rowByLayer(layer), col))) return true;
        }
    }
    return false;
}

namespace {

// A detected crossing and which direction saw it.
//
// The clustering below needs the direction because of the one thing a ring
// cannot be: two crossings of the SAME direction. That is a contradiction, not a
// close call, and until now nothing noticed -- both values were written to the
// same cell and setCell kept whichever came last. On the 2026-08-24 capture pair
// that silently discarded 24 of 166 nodes.
struct RingNode {
    double v;
    int dir;
};

double ringMean(const std::vector<RingNode> &g, int lo, int hi) {
    double s = 0;
    for (int k = lo; k < hi; ++k) s += g[k].v;
    return s / (hi - lo);
}

bool holdsDirTwice(const std::vector<RingNode> &g, int lo, int hi) {
    for (int a = lo; a < hi; ++a)
        for (int b = a + 1; b < hi; ++b)
            if (g[a].dir == g[b].dir) return true;
    return false;
}

// One ring per run -- unless the run holds a direction twice, in which case it is
// two rings the merge window chained together, and it is split at its widest
// internal gap and re-examined.
//
// Why runs chain at all: the window compares each value to the RUNNING MEAN of
// the run being built, so the mean drifts upward and keeps absorbing. On the
// 2026-08-24 pair the first run spanned 18.64 -> 43.73 and held se three times;
// splitting it gives ~19.6 (se+sw), a lone 31.18 (se, which no other direction
// sees), and ~40.6 (all four) -- which is what the image shows.
//
// The window cannot simply be made smaller: 15 px is there to absorb the ~14 px
// disagreement an off-centre capture puts between directions, and the inner rings
// on that capture are 16-18 px apart. There is no width that separates the two
// cases. The direction is what separates them.
//
// Terminates: a run of one cannot hold a direction twice, and every split makes
// both halves strictly shorter.
void emitRings(const std::vector<RingNode> &g, int lo, int hi, std::vector<double> &out) {
    if (hi <= lo) return;
    if (hi - lo == 1 || !holdsDirTwice(g, lo, hi)) {
        out.push_back(ringMean(g, lo, hi));
        return;
    }
    int cut = lo + 1;
    double widest = -1;
    for (int k = lo + 1; k < hi; ++k)
        if (g[k].v - g[k - 1].v > widest) {
            widest = g[k].v - g[k - 1].v;
            cut = k;
        }
    emitRings(g, lo, cut, out);   // ascending, so `out` stays sorted
    emitRings(g, cut, hi, out);
}

}  // namespace

void CaliCompute::updateTableFromCapture(int i, const QVector<QString> &pctList,
                                         const std::map<std::string, std::vector<double>> &ict8dir) {
    if (!hasTable(i)) return;
    const int maxLayer = this->maxLayer(i, 0);

    // Clear all data columns of the round, then set the round number.
    for (auto it = columnMap().constBegin(); it != columnMap().constEnd(); ++it)
        clearColumn(i, it.key());
    updateRoundNum(i);

    // The clear above wipes the Side column too, so (re)write the layer index
    // 0..N down that column — ports init_column_side. Without this the Side
    // column shows blank after Update Table.
    const int colSide = colIndex("side");
    for (int layer = 0; layer < maxLayer; ++layer)
        setCell(i, rowByLayer(layer), colSide, QString::number(layer));

    // PCT is filled AFTER the layout below, because from the "*" (side-layer)
    // row down it must switch to the SIDE pattern. sideStartLayer is set by the
    // cleaning layout; -1 means no split (plain top-packing).
    int sideStartLayer = -1;

    // ict_* 8-direction columns, laid out BY RING:
    //   * diagonals (NW/SE/SW/NE) run continuously, rows 0..D (D = last diagonal);
    //   * straight (N/S/W/E) rings up to the diagonal reach align on the same rows;
    //   * the straight segment AFTER the middle bezel sits on its own, packed from
    //     row D+2 down, with a "*" placed in the Round column on the gap row D+1.
    //
    // This is unconditional. It used to happen only with cleaning on, and the
    // alternative was top-packing each column independently from row 0 -- which
    // means row k in the N column and row k in the E column are different rings
    // whenever the two directions found different numbers of nodes. The table
    // then shows holes wherever the counts disagree, and every row-wise figure
    // computed from it is comparing unrelated rings. Layout is not a cleaning
    // choice; only the two steps that actually discard data are.
    if (!MoilCali::noise_cleaning_enabled()) {
        // CLEAN NOISE OFF -> THE TABLE IS THE RAW DETECTION.
        //
        // Every crossing the detector found appears, in the order it was found,
        // in its own direction's column. Nothing is clustered, nothing is
        // aligned away, and above all nothing is dropped.
        //
        // The ring layout below is the right thing when cleaning is on, but it
        // discards in three places, all of them unconditional until now: a
        // diagonal that matches no ring, a straight that matches neither the
        // diagonal rings nor the after-bezel rings, and the first after-bezel
        // ring (r == 0), which was thrown away as a suspected bezel residual.
        // The note at step 5 already says the rule -- "anything that removes data
        // belongs behind this button" -- and those three were in front of it.
        //
        // KNOW WHAT THIS COSTS. Row k in the N column and row k in the E column
        // are no longer the same ring once two directions find different numbers
        // of nodes, so ict_avg, alpha_* and zfl_* are averaging across a row that
        // no longer means one radius. With cleaning ON the ring layout below
        // still applies and those figures remain comparable. This mode is for
        // seeing what the detector actually produced.
        static const QStringList kAll8 = {"n", "s", "w", "e", "nw", "se", "sw", "ne"};
        for (const QString &d : kAll8) {
            const auto it = ict8dir.find(d.toStdString());
            if (it == ict8dir.end()) continue;
            const int col = colIndex("ict_" + d);
            int layer = 0;
            for (double v : it->second) {
                if (layer >= maxLayer) break;  // the table is only 40 layers tall
                setCell(i, rowByLayer(layer), col, v);
                ++layer;
            }
        }
        // sideStartLayer stays -1, so the PCT fill below top-packs to match.
    } else {
        // Same ring seen from different straight directions can differ by ~14 px
        // (off-centre capture), so merge values within this window into one ring.
        constexpr double kRingMerge = 15.0;
        auto clusterRings = [](std::vector<RingNode> vals) {
            std::sort(vals.begin(), vals.end(),
                      [](const RingNode &a, const RingNode &b) { return a.v < b.v; });
            std::vector<double> rings;
            int start = 0;
            double sum = 0;
            int cnt = 0;
            // Measured from the run's FIRST value, not its running mean. The mean
            // drifts as the run grows, so the window walks outward with it and a
            // run can span far more than kRingMerge -- 18.64 to 43.73 on the
            // 2026-08-24 pair, 25 px through a 15 px window. Anchoring caps a run
            // at the width the comment above claims, which is also what makes the
            // boundary land between rings instead of through one: the ~127 px ring
            // (ne 121.57, sw 125.87, nw 129.99, se 132.23) was being cut in half
            // because the drifting mean reached its limit in the middle of it.
            for (int k = 0; k < static_cast<int>(vals.size()); ++k) {
                if (cnt == 0 || vals[k].v - vals[start].v > kRingMerge) {
                    if (cnt > 0) emitRings(vals, start, k, rings);
                    start = k;
                    sum = vals[k].v;
                    cnt = 1;
                } else {
                    sum += vals[k].v;
                    ++cnt;
                }
            }
            if (cnt > 0) emitRings(vals, start, static_cast<int>(vals.size()), rings);
            return rings;
        };
        // Nearest ring, or -1 when the value does not actually belong to any.
        //
        // The distance limit is the point. Without it this returns the closest
        // ring however far away it is, so every straight node past the diagonals'
        // reach piled onto the last diagonal row -- on the current captures that
        // put values around 1080 on a ring at 843, a 236 px error, and the row
        // read as four directions agreeing on a ring none of them saw.
        //
        // Half the local ring spacing is the natural cut: a node closer than that
        // is this ring, further and it is the next one (or none).
        auto nearestRing = [](const std::vector<double> &rings, double v) -> int {
            const auto lb = std::lower_bound(rings.begin(), rings.end(), v);
            int r = -1;
            double best = 1e18;
            for (int c : {int(lb - rings.begin()) - 1, int(lb - rings.begin())})
                if (c >= 0 && c < static_cast<int>(rings.size()) && std::abs(rings[c] - v) < best) {
                    best = std::abs(rings[c] - v);
                    r = c;
                }
            if (r < 0) return -1;
            double spacing = 2 * kRingMerge;  // lone ring: fall back to the merge window
            if (rings.size() > 1) spacing = (r > 0) ? rings[r] - rings[r - 1] : rings[1] - rings[0];
            return (best <= 0.5 * spacing) ? r : -1;
        };
        auto nodesOf = [&](const QString &d) -> const std::vector<double> * {
            auto it = ict8dir.find(d.toStdString());
            return it == ict8dir.end() ? nullptr : &it->second;
        };
        static const QStringList kDiag = {"nw", "se", "sw", "ne"};
        static const QStringList kStraight = {"n", "s", "w", "e"};

        // --- 1. Diagonals: continuous grid, rows 0..D ---
        std::vector<RingNode> diagAll;
        for (int di = 0; di < kDiag.size(); ++di)
            if (auto *L = nodesOf(kDiag[di]))
                for (double v : *L) diagAll.push_back({v, di});
        const std::vector<double> diagRing = clusterRings(diagAll);
        const int D = static_cast<int>(diagRing.size()) - 1;

        // How far out the top panel is still being measured. Beyond the last
        // diagonal ring plus half its spacing -- the same cut nearestRing applies
        // to that ring -- there are no more top-panel crossings to match.
        double diagReach = 0.0;
        if (!diagRing.empty()) {
            double spacing = 2 * kRingMerge;
            if (diagRing.size() > 1) spacing = diagRing.back() - diagRing[diagRing.size() - 2];
            diagReach = diagRing.back() + 0.5 * spacing;
        }
        sideStartLayer = D + 1;  // side pattern (and "*") begins on this row
        for (const QString &d : kDiag) {
            auto *L = nodesOf(d);
            if (!L) continue;
            const int col = colIndex("ict_" + d);
            for (double v : *L) {
                const int r = nearestRing(diagRing, v);
                if (r >= 0 && r < maxLayer) setCell(i, rowByLayer(r), col, v);
            }
        }

        // --- 2. Straight directions split by whether they land on a diagonal
        //        ring at all. A node that matches one shares its row; a node that
        //        matches none is past the diagonals' reach and belongs to the
        //        after-bezel segment.
        //
        // This used to be decided by a radius threshold picked from the widest gap
        // in the straight nodes. That threshold sat at 1122 px while the diagonals
        // ended at 843, so everything in between was declared "before" and had
        // nowhere correct to go. Asking whether a ring exists is the direct
        // question; the gap heuristic was a proxy for it. ---
        // Only what is PAST the top panel. "Matched no diagonal ring" is not the
        // same question: a straight crossing inside the diagonals' reach that
        // misses every ring is a ring the diagonals did not find, or noise --
        // either way it is a top-panel crossing and it is not a side stripe.
        //
        // Letting those in put a node at 207.0 px, which missed diagonal ring
        // 192.1 by 1 px more than the tolerance allowed, at the head of the side
        // segment. Every side row's ICT then paired with a PCT two stripes further
        // down the pattern, and pct_cal is a RUNNING SUM -- so the first real side
        // ring was credited with three stripes of pattern distance instead of one,
        // and every side alpha and ZFL was wrong. Invisible in the table, because
        // this pattern's 50 side intervals are all 150.
        std::vector<RingNode> afterAll;
        for (int si = 0; si < kStraight.size(); ++si)
            if (auto *L = nodesOf(kStraight[si]))
                for (double v : *L)
                    if (v > diagReach) afterAll.push_back({v, si});
        const std::vector<double> afterRing = clusterRings(afterAll);

        for (const QString &d : kStraight) {
            auto *L = nodesOf(d);
            if (!L) continue;
            const int col = colIndex("ict_" + d);
            for (double v : *L) {
                const int r = nearestRing(diagRing, v);
                if (r >= 0 && r < maxLayer) setCell(i, rowByLayer(r), col, v);
            }
        }

        // --- 3. Star on the gap row D+1 (Round column). setCell() only stores
        //        numeric text, so go straight to the data source. This is also the
        //        side-layer marker read by startInWhichLayer(). ---
        if (D + 1 >= 0 && D + 1 < maxLayer)
            src_->setCell(i, rowByLayer(D + 1), colIndex("round"), QStringLiteral("*"));

        // --- 4. After-bezel (side) straight, packed from row D+2. The FIRST side
        //        ring is dropped (often a bezel-edge residual) and the rest shift
        //        up one row, so ring index r lands on row D+2 + (r-1). ---
        const int afterStart = D + 2;
        for (const QString &d : kStraight) {
            auto *L = nodesOf(d);
            if (!L) continue;
            const int col = colIndex("ict_" + d);
            for (double v : *L) {
                if (v <= diagReach) continue;  // top panel: placed above, or noise
                const int r = nearestRing(afterRing, v);
                if (r >= 1 && afterStart + (r - 1) < maxLayer)  // r==0 -> dropped
                    setCell(i, rowByLayer(afterStart + (r - 1)), col, v);
            }
        }

        // --- 5. Row completeness, and ONLY when Clean Noise is on.
        //
        // Within a group -- N/S/W/E, or NW/SE/SW/NE -- a row survives when at
        // least two of the four are on it. A lone value is a crossing one
        // direction found and the other three did not, which is what an artefact
        // looks like; a row holding it reads as a ring three directions missed.
        //
        // But this DISCARDS nodes the histogram shows, so it is cleaning, and with
        // cleaning off the table has to be the raw detection -- every crossing,
        // where it actually is, however lonely. Anything that removes data belongs
        // behind this button; only the row placement above is unconditional,
        // because that moves nodes rather than dropping them. ---
        // PER GROUP, not across all eight. Do not "tighten" this.
        //
        // Tried on 2026-08-25 and reverted the same day: requiring all eight
        // directions before keeping a row emptied about three quarters of the
        // table on a real capture, because the straights cross the top panel's
        // bezel exactly where the diagonals are still on glass. Rows legitimately
        // carry four diagonals and no straights, or the reverse, and those rows
        // are data -- a ring four directions genuinely measured -- not artefacts.
        // Matches origin/calibration_inROS, where this is the version that works.
        //
        // THIS IS THE ONLY COPY OF THIS RULE. It used to be twinned: the client
        // filled its own table with the same function in cpp/src/core/cali/
        // CaliCompute.cpp, and if the two rules differed the same capture produced
        // two different tables with neither side reporting anything. That client
        // is no longer in this branch, so the drift hazard is gone -- but the
        // reason it was dangerous has not changed. Any future client that fills a
        // round table must call THIS code, not reimplement it.
        // TWO, not four. A row is emptied only when ONE direction is on it.
        //
        // Measured on the 2026-08-24 pair, once the clustering above stopped
        // merging distinct rings: of 166 detected nodes, 157 are placed and the
        // quorum alone decides how many survive --
        //
        //   all four  40    three  88    two  124    off  157
        //
        // "All four" was defensible while a row could hold two rings averaged
        // together; a row that four directions had to agree on was the only cheap
        // guard against that. The rings are now one ring per row, so a row with
        // three values is a ring one direction missed, not an artefact, and
        // deleting the three that WERE measured is throwing away data to hide a
        // gap. One value alone is still what an artefact looks like, and that is
        // what this removes.
        constexpr int kRowQuorum = 2;
        if (MoilCali::noise_cleaning_enabled()) {
            struct Group { QStringList dirs; };
            const QVector<Group> groups = {
                {{"n", "s", "w", "e"}},
                {{"nw", "se", "sw", "ne"}},
            };
            for (const Group &g : groups) {
                QVector<int> cols;
                for (const QString &d : g.dirs) cols.append(colIndex("ict_" + d));
                for (int layer = 0; layer < maxLayer; ++layer) {
                    const int row = rowByLayer(layer);
                    int present = 0;
                    for (int c : cols)
                        if (!cell(i, row, c).trimmed().isEmpty()) ++present;
                    if (present > 0 && present < kRowQuorum)
                        for (int c : cols) clearItem(i, row, c);
                }
            }
        }
    }

    // PCT column. pctList = 25 concentric (TOP) + 50 stripeline (SIDE). Rows above
    // the "*" take the TOP pattern in order; from the "*" (sideStartLayer) down
    // they take the SIDE pattern, so the side pattern's line 1 lands on the "*"
    // row. Without a split (cleaning off) it is the plain top-packing.
    const int colPct = colIndex("pct");
    for (int layer = 0; layer < maxLayer; ++layer) {
        const int src = (sideStartLayer >= 0 && layer >= sideStartLayer)
                            ? kTopLayers + (layer - sideStartLayer)
                            : layer;
        const QString v = (src >= 0 && src < pctList.size() && !pctList[src].trimmed().isEmpty())
                              ? pctList[src].trimmed()
                              : QStringLiteral("0");
        setCell(i, rowByLayer(layer), colPct, v);
    }
}
