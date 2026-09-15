// The calibration table: its cells, the side/layer metadata that says which rows
// of a round mean anything, and the columns derived straight from a capture.
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

namespace cali_detail {

const QStringList &dirs8() {
    static const QStringList v = {"n", "w", "s", "e", "nw", "se", "sw", "ne"};
    return v;
}

const QStringList &dirsXY() {
    static const QStringList v = {"n", "s", "w", "e", "nw", "se", "sw", "ne"};
    return v;
}

const QHash<QString, int> &columnMap() {
    static const QHash<QString, int> m = {
        {"round", 0}, {"side", 1}, {"pct", 2},
        {"ict_n", 3}, {"ict_s", 4}, {"ict_w", 5}, {"ict_e", 6},
        {"ict_nw", 7}, {"ict_se", 8}, {"ict_sw", 9}, {"ict_ne", 10},
        {"empty11", 11}, {"ict_avg", 12}, {"pct_cal", 13}, {"distance", 14},
        {"empty15", 15},
        {"alpha_n", 16}, {"zfl_n", 17}, {"alpha_s", 18}, {"zfl_s", 19},
        {"alpha_w", 20}, {"zfl_w", 21}, {"alpha_e", 22}, {"zfl_e", 23},
        {"alpha_nw", 24}, {"zfl_nw", 25}, {"alpha_se", 26}, {"zfl_se", 27},
        {"alpha_sw", 28}, {"zfl_sw", 29}, {"alpha_ne", 30}, {"zfl_ne", 31},
        {"empty32", 32}, {"alpha_avg", 33}, {"zfl_avg", 34},
    };
    return m;
}

QString numText(double v) { return QString::number(v, 'g', 15); }

double roundTo2(double v) { return std::round(v * 100.0) / 100.0; }

}  // namespace cali_detail

int CaliCompute::colIndex(const QString &title) { return columnMap().value(title, -1); }

QColor CaliCompute::roundColor(int round) {
    // controller.curve_color (round 1..10); round 0 -> white.
    static const QColor c[10] = {
        QColor(255, 0, 0),   QColor(255, 255, 0), QColor(50, 255, 255),
        QColor(0, 0, 255),   QColor(255, 127, 127), QColor(127, 0, 127),
        QColor(127, 0, 0),   QColor(127, 127, 0), QColor(127, 170, 0),
        QColor(127, 170, 127)};
    if (round <= 0) return QColor(255, 255, 255);
    return c[std::clamp(round - 1, 0, 9)];
}

bool CaliCompute::isFloat(const QString &s, double *out) {
    bool ok = false;
    const double v = s.trimmed().toDouble(&ok);
    if (ok && out) *out = v;
    return ok;
}

QString CaliCompute::cell(int i, int row, int col) const {
    return src_ ? src_->cell(i, row, col) : QString();
}

void CaliCompute::setCell(int i, int row, int col, const QString &val) {
    // set_item_data: store text only if float-able, else empty.
    if (src_) src_->setCell(i, row, col, isFloat(val) ? val : QString());
}

void CaliCompute::setCell(int i, int row, int col, double val) { setCell(i, row, col, numText(val)); }

void CaliCompute::clearItem(int i, int row, int col) {
    if (src_) src_->setCell(i, row, col, QString());
}

void CaliCompute::clearColumn(int i, const QString &title) {
    if (!hasTable(i)) return;
    const int col = colIndex(title);
    const int rows = src_->rowCount(i);
    for (int row = 2; row < rows; ++row) clearItem(i, row, col);
}

double CaliCompute::lineEdit(const QString &name, double def) const {
    return src_ ? src_->field(name, def) : def;
}

QString CaliCompute::avgWithoutZero(const QVector<QString> &vals) {
    int count = 0;
    double sum = 0;
    for (const QString &s : vals) {
        double v = 0;
        if (!isFloat(s, &v)) continue;
        ++count;
        sum += v;
    }
    if (count == 0) return {};
    return numText(roundTo2(sum / count));
}

// ---- side / metadata ------------------------------------------------------
int CaliCompute::startInWhichLayer(int i) const {
    if (!hasTable(i)) return -1;
    const int col = colIndex("round");
    const int maxLayer = this->maxLayer(i, 0);
    for (int layer = 0; layer < maxLayer; ++layer)
        if (cell(i, rowByLayer(layer), col) == "*") return layer;
    return -1;
}

void CaliCompute::updateSideLayer(int i) {
    int side = startInWhichLayer(i);
    if (side == -1) side = kTopLayers;
    setCell(i, rowByLayer(-1), colIndex("side"), numText(side));
}

void CaliCompute::updateRoundNum(int i) {
    setCell(i, rowByLayer(-1), colIndex("round"), QString::number(i));
}

int CaliCompute::sideLayer(int i) const {
    double v = kTopLayers;
    isFloat(cell(i, rowByLayer(-1), colIndex("side")), &v);
    return int(v);
}

// ---- derived columns ------------------------------------------------------
void CaliCompute::remove45SideData(int i) {
    // Clearing the four diagonals from the side layer down REMOVES DATA, so it
    // belongs behind Clean Noise like everything else that does. With cleaning
    // off the diagonals stay exactly as detected -- which is the point of the
    // raw table, and this used to wipe them regardless.
    if (!MoilCali::noise_cleaning_enabled()) return;
    const int side = sideLayer(i);
    const int maxLayer = this->maxLayer(i, 40);
    for (int layer = side; layer < std::min(40, maxLayer); ++layer)
        for (const QString &d : {QStringLiteral("nw"), QStringLiteral("se"),
                                 QStringLiteral("sw"), QStringLiteral("ne")})
            clearItem(i, rowByLayer(layer), colIndex("ict_" + d));
}

void CaliCompute::updateIctAvg(int i) {
    if (!hasTable(i)) return;
    const int col = colIndex("ict_avg");
    const int maxLayer = this->maxLayer(i, 0);
    for (int layer = 0; layer < maxLayer; ++layer) {
        const int row = rowByLayer(layer);
        QVector<QString> vals;
        for (const QString &d : dirs8()) vals.append(cell(i, row, colIndex("ict_" + d)));
        setCell(i, row, col, avgWithoutZero(vals));
    }
}

void CaliCompute::updatePctCal(int i) {
    if (!hasTable(i)) return;
    const double pxTop = lineEdit("lineedit_pixel_size_top", 0.2478);
    const double pxSide = lineEdit("lineedit_pixel_size_side", 0.155);
    const int side = sideLayer(i);
    const int col = colIndex("pct_cal");
    const int colPct = colIndex("pct");
    const int maxLayer = this->maxLayer(i, 0);
    for (int layer = 0; layer < maxLayer; ++layer) {
        const int startLayer = (layer < side) ? 0 : side;
        const double pixel = (layer < side) ? pxTop : pxSide;
        double sumPct = 0;
        for (int lp = startLayer; lp <= layer; ++lp) {
            double p = 0;
            if (!isFloat(cell(i, rowByLayer(lp), colPct), &p)) p = 0;
            sumPct += p;
        }
        setCell(i, rowByLayer(layer), col, sumPct * pixel);
    }
}

bool CaliCompute::ownRoundDistance(int i, double *out) const {
    const QString name = roundDistanceField(i);
    if (!useRoundDistances_ || !src_ || !src_->hasField(name)) return false;
    const double v = src_->field(name, std::nan(""));
    if (!std::isfinite(v)) return false;
    *out = v;
    return true;
}

bool CaliCompute::roundHasRawIct(int i) const {
    if (!hasTable(i)) return false;
    const int maxLayer = this->maxLayer(i, 0);
    for (const QString &d : dirs8()) {
        const int col = colIndex("ict_" + d);
        for (int layer = 0; layer < maxLayer; ++layer)
            if (isFloat(cell(i, rowByLayer(layer), col))) return true;
    }
    return false;
}

// Raw ICT, not ict_avg, so the anchor does not depend on which round was
// calculated first.
int CaliCompute::firstRoundWithRawIct() const {
    for (int r = 0; r <= 10; ++r)
        if (roundHasRawIct(r)) return r;
    return -1;
}

void CaliCompute::updateDistance(int i, double baseDistance) {
    if (!hasTable(i)) return;
    double distance = 0;
    if (!ownRoundDistance(i, &distance)) {
        const int firstValid = firstRoundWithRawIct();
        if (firstValid < 0 || i < firstValid) return;
        double rv = 0;
        if (!isFloat(cell(i, rowByLayer(-1), colIndex("round")), &rv)) return;
        const double disPerRound = lineEdit(kDistanceStepField, 10.0);
        distance = baseDistance + disPerRound * (rv - firstValid);
    }
    const int col = colIndex("distance");
    const int maxLayer = this->maxLayer(i, 0);
    for (int layer = 0; layer < maxLayer; ++layer) setCell(i, rowByLayer(layer), col, distance);
}

QString CaliCompute::calcAlpha(int i, const QString &dir, int layer) const {
    const int row = rowByLayer(layer);
    double ict = 0;
    if (!isFloat(cell(i, row, colIndex("ict_" + dir)), &ict)) return {};

    const int side = sideLayer(i);
    const bool oblique = (dir == "nw" || dir == "se" || dir == "sw" || dir == "ne");
    if (oblique && layer >= side) return {};

    double pctCal = 0;
    if (!isFloat(cell(i, row, colIndex("pct_cal")), &pctCal)) return {};
    double distance = 0;
    if (!isFloat(cell(i, row, colIndex("distance")), &distance)) return {};

    if (layer < side) {
        return numText(std::atan(pctCal / distance));  // top screen
    }
    // side screen (cardinal directions only)
    const double hGap = lineEdit("lineedit_h_gap_" + dir, 250.0);
    const double vGap = lineEdit("lineedit_v_gap_" + dir, 42.0);
    return numText(M_PI / 2 - std::atan((distance - pctCal - vGap) / hGap));
}

void CaliCompute::updateAlpha8(int i) {
    if (!hasTable(i)) return;
    const int maxLayer = this->maxLayer(i, 0);
    for (const QString &d : dirs8()) {
        const int col = colIndex("alpha_" + d);
        for (int layer = 0; layer < maxLayer; ++layer)
            setCell(i, rowByLayer(layer), col, calcAlpha(i, d, layer));
    }
}

QString CaliCompute::calcZfl(int i, const QString &dir, int layer) const {
    const int row = rowByLayer(layer);
    double alpha = 0, ict = 0;
    if (!isFloat(cell(i, row, colIndex("alpha_" + dir)), &alpha)) return {};
    if (!isFloat(cell(i, row, colIndex("ict_" + dir)), &ict)) return {};
    return numText(1.0 / std::tan(alpha) * ict);
}

void CaliCompute::updateZfl8(int i) {
    if (!hasTable(i)) return;
    const int maxLayer = this->maxLayer(i, 0);
    for (const QString &d : dirs8()) {
        const int col = colIndex("zfl_" + d);
        for (int layer = 0; layer < maxLayer; ++layer)
            setCell(i, rowByLayer(layer), col, calcZfl(i, d, layer));
    }
}

void CaliCompute::updateAlphaAvg(int i) {
    const int side = sideLayer(i);
    const int col = colIndex("alpha_avg");
    for (int layer = 0; layer < side; ++layer) {
        const int row = rowByLayer(layer);
        QVector<QString> vals;
        for (const QString &d : dirs8()) vals.append(cell(i, row, colIndex("alpha_" + d)));
        setCell(i, row, col, avgWithoutZero(vals));
    }
}

void CaliCompute::updateZflAvg(int i) {
    const int side = sideLayer(i);
    const int col = colIndex("zfl_avg");
    for (int layer = 0; layer < side; ++layer) {
        const int row = rowByLayer(layer);
        QVector<QString> vals;
        for (const QString &d : dirs8()) vals.append(cell(i, row, colIndex("zfl_" + d)));
        setCell(i, row, col, avgWithoutZero(vals));
    }
}

