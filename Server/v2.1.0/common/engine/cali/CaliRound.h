#pragma once

#include <array>
#include <vector>

// Per-round calibration compute engine (decoupled from the Qt table).
// Composes the verified primitives (CaliMath) into the column pipeline of
// controller_cali_result: pct (pattern dims) -> pct_cal -> distance -> alpha.
// The Qt table view later just mirrors these columns.
class CaliRound {
public:
    static constexpr int kLayers = 75;

    // pct = pattern physical dimensions per layer (25 concentric radii + 50
    // stripeline intervals in the Python model). Truncated/padded to kLayers.
    void setPct(const std::vector<double> &pct);

    // Same distance for every layer of the round.
    void setDistance(double base, double disPerRound, double round, double firstValidRound);

    // pct_cal[layer] via CaliMath::pctCal (cumulative pct * pixel size).
    void computePctCal(int sideLayer, double pixelSizeTop, double pixelSizeSide);

    // alpha for one direction: atan form below sideLayer, side form at/after it.
    std::vector<double> computeAlpha(int sideLayer, double vGap, double hGap) const;

    const std::array<double, kLayers> &pct() const { return pct_; }
    const std::array<double, kLayers> &pctCal() const { return pctCal_; }
    double distance() const { return distance_; }

private:
    std::array<double, kLayers> pct_{};
    std::array<double, kLayers> pctCal_{};
    double distance_ = 0.0;
};
