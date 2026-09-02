#include "CaliMath.h"

#include <algorithm>
#include <cmath>

namespace CaliMath {

double averageWithoutZero(const std::vector<double> &values) {
    if (values.empty()) return 0.0;
    double sum = 0.0;
    for (double v : values) sum += v;
    return sum / static_cast<double>(values.size());
}

double pctCal(const std::vector<double> &pctColumn, int targetLayer, int sideLayer,
              double pixelSizeTop, double pixelSizeSide) {
    int start, end;
    double pixelSize;
    if (targetLayer < sideLayer) {
        start = 0;
        end = targetLayer;
        pixelSize = pixelSizeTop;
    } else {
        start = sideLayer;
        end = targetLayer;
        pixelSize = pixelSizeSide;
    }
    start = std::max(0, start);
    end = std::min(end, static_cast<int>(pctColumn.size()) - 1);

    double sum = 0.0;
    for (int i = start; i <= end; ++i) sum += pctColumn[i];
    return sum * pixelSize;
}

double distance(double base, double disPerRound, double round, double firstValidRound) {
    return base + disPerRound * (round - firstValidRound);
}

double alphaTop(double pctCal, double distance) {
    return std::atan(pctCal / distance);
}

double alphaSide(double distance, double pctCal, double vGap, double hGap) {
    return M_PI / 2.0 - std::atan((distance - pctCal - vGap) / hGap);
}

}  // namespace CaliMath
