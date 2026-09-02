// The dense bands a monitor gap leaves in the ring crossings, and removing the
// nodes that fall inside them.
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

namespace {
// Tuning for band detection. A real ring is spaced ~trend apart; the monitor gap
// produces a run of much tighter ("tiny") crossings. These separate the two.
constexpr double kTinyFrac = 0.55;  // gap < kTinyFrac*trend  -> inside a dense band
constexpr double kGridLo = 0.78;    // a "grid ring" has a neighbour gap in
constexpr double kGridHi = 1.5;     //   [kGridLo, kGridHi]*trend (near real spacing)

double medianOfVec(std::vector<double> v) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    const size_t m = v.size() / 2;
    return (v.size() % 2) ? v[m] : 0.5 * (v[m - 1] + v[m]);
}

// A node is a real "grid ring" if at least one side is spaced near the trend.
bool isGridNode(const std::vector<double> &nd, int j, double T) {
    const int n = static_cast<int>(nd.size());
    const double gl = (j > 0) ? nd[j] - nd[j - 1] : 1e18;
    const double gr = (j < n - 1) ? nd[j + 1] - nd[j] : 1e18;
    const double lo = kGridLo * T, hi = kGridHi * T;
    return (gl >= lo && gl <= hi) || (gr >= lo && gr <= hi);
}

// Detect noise bands in ONE (sorted) direction: runs of tiny gaps, with real
// grid rings trimmed off the ends, holes bridged, and the terminal edge (a run
// with no grid ring after it) ignored. Returns (anchor_lo, anchor_hi) pairs.
std::vector<std::pair<double, double>> detectDirBands(const std::vector<double> &nd, double T) {
    std::vector<std::pair<double, double>> out;
    const int n = static_cast<int>(nd.size());
    if (n < 5) return out;
    const double tiny = kTinyFrac * T;

    std::vector<char> grid(n);
    for (int j = 0; j < n; ++j) grid[j] = isGridNode(nd, j, T);

    // Maximal runs of consecutive tiny gaps -> node index ranges [s, e].
    std::vector<std::pair<int, int>> runs;
    for (int j = 0; j < n - 1;) {
        if (nd[j + 1] - nd[j] < tiny) {
            int k = j;
            while (k + 1 < n - 1 && nd[k + 2] - nd[k + 1] < tiny) ++k;
            runs.push_back({j, k + 1});
            j = k + 1;
        } else {
            ++j;
        }
    }
    // Trim grid rings (real anchors) off both ends of each run.
    std::vector<std::pair<int, int>> trimmed;
    for (auto pr : runs) {
        int s = pr.first, e = pr.second;
        while (s < e && grid[s]) ++s;
        while (e > s && grid[e]) --e;
        if (!(s == e && grid[s])) trimmed.push_back({s, e});
    }
    // Bridge runs separated only by node-less holes (no grid ring in between):
    // the monitor gap can occlude whole rings, leaving a hole inside the band.
    std::vector<std::pair<int, int>> bands;
    for (auto pr : trimmed) {
        if (!bands.empty()) {
            bool gridBetween = false;
            for (int k = bands.back().second + 1; k < pr.first; ++k)
                if (grid[k]) { gridBetween = true; break; }
            if (!gridBetween) { bands.back().second = pr.second; continue; }
        }
        bands.push_back(pr);
    }
    // Anchor each band to the nearest grid ring on both sides; drop terminal
    // (edge) runs that have no grid ring after them.
    for (auto pr : bands) {
        double lo = 0, hi = 0;
        bool haveLo = false, haveHi = false;
        for (int k = pr.first - 1; k >= 0; --k)
            if (grid[k]) { lo = nd[k]; haveLo = true; break; }
        for (int k = pr.second + 1; k < n; ++k)
            if (grid[k]) { hi = nd[k]; haveHi = true; break; }
        if (haveLo && haveHi) out.push_back({lo, hi});
    }
    return out;
}
}  // namespace

QVector<CaliCompute::NoiseBand> CaliCompute::autoDetectNoiseBands(int i) const {
    struct Group { const char *name; QStringList dirs; };
    static const QVector<Group> kGroups = {
        {"N & S", {"n", "s"}},
        {"W & E", {"w", "e"}},
        {"NW / SE / SW / NE", {"nw", "se", "sw", "ne"}},
    };

    QVector<NoiseBand> result;
    const bool present = hasTable(i);
    const int maxLayer = this->maxLayer(i, 0);

    for (const Group &g : kGroups) {
        NoiseBand nb;
        nb.group = g.name;
        nb.dirs = g.dirs;
        if (!present) { result.append(nb); continue; }

        // Collect each direction's sorted node list + a combined gap list (for
        // the shared trend, robust because real gaps dominate the band's tiny ones).
        std::vector<std::vector<double>> cols;
        std::vector<double> allGaps;
        for (const QString &d : g.dirs) {
            const int col = colIndex("ict_" + d);
            std::vector<double> nd;
            for (int layer = 0; layer < maxLayer; ++layer) {
                double v = 0;
                if (isFloat(cell(i, rowByLayer(layer), col), &v)) nd.push_back(v);
            }
            std::sort(nd.begin(), nd.end());
            for (size_t k = 1; k < nd.size(); ++k) allGaps.push_back(nd[k] - nd[k - 1]);
            cols.push_back(std::move(nd));
        }
        if (allGaps.size() < 4) { result.append(nb); continue; }
        const double T = medianOfVec(allGaps);

        std::vector<std::pair<double, double>> bands;
        for (const auto &nd : cols) {
            const auto b = detectDirBands(nd, T);
            bands.insert(bands.end(), b.begin(), b.end());
        }
        if (bands.empty()) { result.append(nb); continue; }

        // One shared range per group. Use the SAFE intersection of the per-
        // direction bands (max of lower anchors, min of upper anchors) so no real
        // anchor ring is ever swept in; fall back to the union if they don't overlap.
        double lo = -1e18, hi = 1e18;
        for (const auto &b : bands) { lo = std::max(lo, b.first); hi = std::min(hi, b.second); }
        if (hi <= lo) {
            lo = 1e18;
            hi = -1e18;
            for (const auto &b : bands) { lo = std::min(lo, b.first); hi = std::max(hi, b.second); }
        }
        nb.found = true;
        nb.lo = lo;
        nb.hi = hi;
        result.append(nb);
    }
    return result;
}

int CaliCompute::removeNodesInBand(int i, const QStringList &dirs, double lo, double hi) {
    if (hi <= lo) return 0;
    if (!hasTable(i)) return 0;
    const int maxLayer = this->maxLayer(i, 0);
    int removed = 0;
    for (const QString &d : dirs) {
        const int col = colIndex("ict_" + d);
        for (int layer = 0; layer < maxLayer; ++layer) {
            const int row = rowByLayer(layer);
            double v = 0;
            // Strictly between lo and hi -> noise inside the band; blank in place
            // (no shifting) so the anchor rings and every survivor keep their row.
            if (isFloat(cell(i, row, col), &v) && v > lo && v < hi) {
                clearItem(i, row, col);
                ++removed;
            }
        }
    }
    if (removed > 0) updateIctAvg(i);
    return removed;
}
