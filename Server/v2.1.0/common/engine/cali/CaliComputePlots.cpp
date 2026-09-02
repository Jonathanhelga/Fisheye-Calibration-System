// Extracting series for the client to draw. Nothing here decides anything; it
// reshapes what the other four files computed.
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

// ---- plot extraction ------------------------------------------------------
QVector<CaliCompute::Series> CaliCompute::ictZflSeries() {
    QVector<Series> out;
    for (int r = 0; r <= 10; ++r) {
        if (!isRoundEnabled(r)) continue;  // round 0 always enabled
        QVector<double> xs, ys;
        ictZflXY(r, xs, ys);
        const int n = std::min(xs.size(), ys.size());
        if (n < 1) continue;
        Series s;
        s.round = r;
        s.color = roundColor(r);
        for (int k = 0; k < n; ++k) s.pts.append(QPointF(xs[k], ys[k]));
        out.append(s);
    }
    return out;
}

QVector<QPointF> CaliCompute::ictZflPoints(int round) {
    QVector<double> xs, ys;
    ictZflXY(round, xs, ys);
    const int n = std::min(xs.size(), ys.size());
    QVector<QPointF> pts;
    for (int k = 0; k < n; ++k) pts.append(QPointF(xs[k], ys[k]));
    return pts;
}

void CaliCompute::gatherAlphaIctTraining(std::vector<double> &tx, std::vector<double> &ty) {
    for (int r = 0; r <= 10; ++r) {
        if (!isRoundEnabled(r)) continue;
        QVector<double> xs, ys;
        gatherRoundXY(r, "alpha", "ict", xs, ys);
        const int n = std::min(xs.size(), ys.size());
        for (int k = 0; k < n; ++k) { tx.push_back(xs[k]); ty.push_back(ys[k]); }
    }
}

QVector<CaliCompute::Series> CaliCompute::ihAlphaSeries() {
    QVector<Series> out;
    for (int r = 0; r <= 10; ++r) {
        if (!isRoundEnabled(r)) continue;
        QVector<double> xs, ys;  // alpha(rad), ict
        gatherRoundXY(r, "alpha", "ict", xs, ys);
        const int n = std::min(xs.size(), ys.size());
        if (n <= 1) continue;
        Series s;
        s.round = r;
        s.color = roundColor(r);
        for (int k = 0; k < n; ++k) s.pts.append(QPointF(xs[k] * 180.0 / M_PI, ys[k]));
        out.append(s);
    }
    return out;
}

QVector<QPointF> CaliCompute::ihAlphaRegression(int degree) {
    std::vector<double> tx, ty;  // alpha(rad), ict
    gatherAlphaIctTraining(tx, ty);
    QVector<QPointF> curve;
    if ((int)tx.size() <= 2) return curve;

    const std::vector<double> coef = Regression::polyFit(tx, ty, degree);
    if (coef.empty()) return curve;

    std::vector<double> sx = tx;
    std::sort(sx.begin(), sx.end());
    for (double x : sx) curve.append(QPointF(x * 180.0 / M_PI, Regression::polyEval(coef, x)));
    return curve;
}

QVector<double> CaliCompute::alphaPolynomial(int degree) {
    std::vector<double> tx, ty;
    gatherAlphaIctTraining(tx, ty);
    QVector<double> out;
    if ((int)tx.size() <= 2) return out;
    const std::vector<double> coef = Regression::polyFit(tx, ty, degree);
    for (double c : coef) out.append(c);
    return out;
}

