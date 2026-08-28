#pragma once

#include <vector>

#include <opencv2/core.hpp>

// C++ port of libcbdetect -- the checkerboard detector of
//
//   A. Geiger, F. Moosmann, O. Car, B. Schuster,
//   "Automatic Camera and Range Sensor Calibration using a single Shot",
//   ICRA 2012.   https://www.cvlibs.net/software/libcbdetect/
//
// Original MATLAB implementation Copyright 2012 Andreas Geiger, Institute of
// Measurement and Control Systems, Karlsruhe Institute of Technology, released
// under the GNU General Public License v3. This port is a derived work and
// carries the same licence.
//
// WHY IT IS HERE. Everything else in PanelCornerRecovery detects a board by
// assuming the grid is (or can be made) globally straight: findChessboardCorners*
// wants straight rows, and the two rectifying paths straighten the panel first.
// This method never makes that assumption. It finds corners individually by
// correlating a checkerboard prototype at several scales and orientations,
// measures each corner's two edge directions, and then GROWS the board one
// row/column at a time, predicting the next corner from the local trend of the
// last three (extrapolating both the angle and the spacing). A fisheye grid is
// exactly that: locally regular, globally curved. It also needs no grid size,
// and returns every board it can find rather than one requested shape.
//
// The cost is speed -- the corner likelihood is a set of large convolutions --
// and a tendency to return partial boards when contrast fails locally.
namespace CbDetect {

// One detected corner: sub-pixel position plus the two edge directions that
// meet there (unit vectors, forming a right-handed pair).
struct Corner {
    cv::Point2f p;
    cv::Point2f v1, v2;
    double score = 0;
};

struct Params {
    // Radii of the correlation prototypes, in pixels. These set the range of
    // square sizes the detector responds to; the defaults are the paper's.
    std::vector<int> radii{4, 8, 12};
    double tauNms = 0.025;  // corner-likelihood floor during non-maxima suppression
    double tauScore = 0.01;  // final corner-quality floor
    int nmsWindow = 3;       // NMS neighbourhood (n)
    int nmsMargin = 5;       // border kept clear of corners
    bool refine = true;      // sub-pixel + orientation refinement

    // --- structure recovery ---
    // The paper's two hard-coded gates, exposed because both of them assume a
    // board that is only mildly distorted, and a fisheye panel is not.
    //
    // seedHomogeneity: a 3x3 seed is rejected when the relative spread of its
    // corner spacings exceeds this. On a strongly foreshortened panel neighbour
    // spacings genuinely differ by more than the paper's 0.3, so every seed on
    // the off-axis panels is thrown away before growth even starts.
    //
    // energyAccept: a grown board is kept when its energy drops below this.
    // Energy is -N + N * (largest relative second difference anywhere in the
    // board), so it charges the whole board for its single most curved triple --
    // which on a coarse board seen through a fisheye is a large number.
    double seedHomogeneity = 0.3;
    double energyAccept = -10.0;

    // How many times to re-run structure recovery on the corners no board has
    // claimed yet. The paper does one pass, which on a calibration box leaves
    // whole panels unclaimed (see the comment in boardsFromCorners). 1 restores
    // the original behaviour.
    int maxPasses = 6;

    // Bound on the structure-recovery stage: it is O(seeds * corners), so a
    // texture-rich image can produce far more seeds than are useful.
    int maxCorners = 1200;
    long budgetMs = 4000;
};

// Corner detection only (stages 1-3 of the paper).
std::vector<Corner> findCorners(const cv::Mat &image, const Params &params = Params());

// A recovered board. `idx` is row-major, rows*cols entries, each an index into
// the corner list the board was built from. Boards are always complete
// rectangles; `energy` is the paper's score (lower is better, < -10 accepted).
struct Board {
    int rows = 0, cols = 0;
    std::vector<int> idx;
    double energy = 0;
    int at(int r, int c) const { return idx[static_cast<size_t>(r) * cols + c]; }
};

// Where the seeds died, for tuning. Every corner is tried as a seed, so these
// count attempts, not corners.
struct Stats {
    int seeds = 0;           // seeds attempted
    int seedNoInit = 0;      // no valid 3x3 neighbourhood (spacing too uneven)
    int seedBadEnergy = 0;   // 3x3 formed but already too distorted to grow
    int grown = 0;           // reached the growth stage
    int rejectedEnergy = 0;  // grown but never good enough to keep
    int rejectedOverlap = 0; // good, but a better board already owned its corners
    int passes = 0;          // structure-recovery passes that produced a board
};

// Structure recovery (stage 4): grow boards out of the detected corners. The
// returned boards do not overlap; they are sorted by energy, best first.
std::vector<Board> boardsFromCorners(const std::vector<Corner> &corners,
                                     const Params &params = Params(), Stats *stats = nullptr);

}  // namespace CbDetect
