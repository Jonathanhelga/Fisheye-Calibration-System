#pragma once

#include <vector>

// Pure calibration math extracted from controller_cali_result.py
// (calculate_pct_cal / calculate_distance / calculate_alpha /
// average_without_zero). No Qt / no table model — the table-model glue that
// feeds these values is ported separately.
namespace CaliMath {

// Mean of the numeric values (matches average_without_zero, which averages all
// float-parseable entries). Returns 0 for an empty list.
double averageWithoutZero(const std::vector<double> &values);

// Cumulative PCT * pixel size up to `targetLayer`. Below `sideLayer` the sum
// runs from layer 0 with the top pixel size; at/after it the sum starts at
// `sideLayer` with the side pixel size. (calculate_pct_cal)
double pctCal(const std::vector<double> &pctColumn, int targetLayer, int sideLayer,
              double pixelSizeTop, double pixelSizeSide);

// base + disPerRound * (round - firstValidRound)  (calculate_distance)
double distance(double base, double disPerRound, double round, double firstValidRound);

// Top screen: atan(pctCal / distance).
double alphaTop(double pctCal, double distance);

// Side screen: pi/2 - atan((distance - pctCal - vGap) / hGap).
double alphaSide(double distance, double pctCal, double vGap, double hGap);

}  // namespace CaliMath
