#include "CaliRound.h"

#include <algorithm>

#include "CaliMath.h"

void CaliRound::setPct(const std::vector<double> &pct) {
    pct_.fill(0.0);
    const int n = std::min(static_cast<int>(pct.size()), kLayers);
    for (int i = 0; i < n; ++i) pct_[i] = pct[i];
}

void CaliRound::setDistance(double base, double disPerRound, double round,
                            double firstValidRound) {
    distance_ = CaliMath::distance(base, disPerRound, round, firstValidRound);
}

void CaliRound::computePctCal(int sideLayer, double pixelSizeTop, double pixelSizeSide) {
    const std::vector<double> col(pct_.begin(), pct_.end());
    for (int layer = 0; layer < kLayers; ++layer)
        pctCal_[layer] = CaliMath::pctCal(col, layer, sideLayer, pixelSizeTop, pixelSizeSide);
}

std::vector<double> CaliRound::computeAlpha(int sideLayer, double vGap, double hGap) const {
    std::vector<double> alpha(kLayers, 0.0);
    for (int layer = 0; layer < kLayers; ++layer) {
        if (layer < sideLayer)
            alpha[layer] = CaliMath::alphaTop(pctCal_[layer], distance_);
        else
            alpha[layer] = CaliMath::alphaSide(distance_, pctCal_[layer], vGap, hGap);
    }
    return alpha;
}
