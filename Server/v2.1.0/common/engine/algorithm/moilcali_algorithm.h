#pragma once

#include <map>
#include <string>
#include <utility>
#include <vector>

#include <opencv2/core.hpp>

// Port of mvc_model/moilcali_algorithm.py (MoilCali). Pure OpenCV/std, no Qt.
namespace MoilCali {

// Gray-scale intensities sampled from `center` outwards along `direction`
// (one of n/s/w/e/nw/ne/sw/se). Empty if the image can't be read.
std::vector<int> get_pattern_histogram_gray(const std::string &imgPath,
                                         cv::Point center,
                                         const std::string &direction);

// NEW (performance): sample a line from an ALREADY decoded+blurred gray image,
// so callers can decode once and reuse it across all 8 directions (avoids the
// per-direction imread+cvtColor+blur). Use load_blurred_gray() to build the Mat.
std::vector<int> get_pattern_histogram_gray(const cv::Mat &gray, cv::Point center,
                                            const std::string &direction);
cv::Mat load_blurred_gray(const std::string &imgPath);

// Same preparation as load_blurred_gray, but from an already-decoded image
// instead of a file. The compute node receives captures as encoded bytes over a
// ROS service, never as a path -- it has no access to the client's image_cali
// folder -- and it must blur exactly as the client does or every node position
// shifts.
cv::Mat blur_gray(const cv::Mat &image);

// Colour-channel variant. In Python this is a `pass` stub; kept 1:1 as an
// empty result until the calibration flow needs it.
std::vector<int> get_pattern_histogram_color(const std::string &imgPath,
                                             cv::Point center,
                                             const std::string &direction,
                                             const std::string &color);

cv::Mat draw_edge_circle_on_imgpath(const std::string &path, int cpx, int cpy,
                                int radius = 0,
                                cv::Scalar color = cv::Scalar(0, 255, 255),
                                int thickness = 3);

// Draws the centering ROI (box + diagonals + circle) in place; returns img.
cv::Mat &draw_center_roi_on_cv2obj(cv::Mat &img, int cpx, int cpy, int radius = 100);

// ---- Optional node-noise cleaning -----------------------------------------
// The monitor's black gaps between panels appear in the fisheye image as flat
// dark bands. Along a scan line they make the pos/neg curves cross repeatedly at
// tiny, irregular spacing — false "nodes" that corrupt calibration. When the
// user enables cleaning (a UI toggle), get_list_intersecting_nodes() drops those:
//   1. Intensity/contrast mask: reject a crossing whose local signal is flat &
//      dark (a monitor gap has no real ring there).
//   2. Spacing filter: drop nodes far closer than the *local* trend, which is
//      safe for the genuine monotonic spacing compression toward the edge.
// Default OFF -> behaviour is byte-for-byte identical to before (no cleaning).
void set_noise_cleaning(bool enabled);
bool noise_cleaning_enabled();

// RAW nodes: no preprocessing of any kind before the crossings are found.
//
// set_noise_cleaning already gates the cleaning STEPS -- the black mask, the
// grayscale test at the crossing, the cross-direction reconcile. Two things ran
// regardless of it, and they are preprocessing by any reasonable reading:
//
//   * a 3x3 box blur over both images (blur_gray), which moves a crossing by a
//     fraction of a pixel and can merge two close rings into one;
//   * the `pos[i] > 5` guard, which drops a crossing whose positive sample is
//     almost black.
//
// With raw on, neither happens: the images are converted to grayscale (a single
// channel is needed to sample an intensity at all) and used exactly as captured.
//
// DEFAULT OFF, and it must stay off. Existing callers -- including the ordinary
// client -- keep the behaviour they have always had; only a caller that asks for
// raw gets it.
//
// Be aware of what the `pos[i] > 5` guard was doing before turning this on: in
// the black surround outside the fisheye circle both curves sit at ~0, and the
// equality cases below in get_list_intersecting_nodes then treat ordinary sensor
// noise as a crossing. Raw output there is genuinely raw, and that includes
// nodes the pattern never produced.
void set_raw_nodes(bool enabled);
bool raw_nodes_enabled();

// Spacing-based cleaner, exposed for testing. Drops both endpoints of any gap
// that is < factor * local-median spacing, removing monitor-gap node clusters
// while preserving the genuine fisheye compression toward the edge. The 0.62
// default separates real edge spacing (~0.65 of local) from gap noise (~0.55).
std::vector<double> clean_intersecting_nodes(const std::vector<double> &nodes,
                                             double factor = 0.62);

// x-positions where the pos/neg intensity curves cross. If noise cleaning is
// enabled (see set_noise_cleaning), flat dark monitor-gap crossings and residual
// close-spaced outliers are filtered out.
std::vector<double> get_list_intersecting_nodes(const std::vector<int> &pos,
                                             const std::vector<int> &neg);

// (center, radius) of the fisheye circle via binary threshold edge scan.
std::pair<cv::Point, int> detect_fisheye_edge(const cv::Mat &image, int threshold);

cv::Point detect_roi(const cv::Mat &image, int orgX, int orgY, int thr);

// ---- Centre from the concentric pattern's known ring structure --------------
// The PCT generator draws `expectedRings` boundaries at known radii. Whatever the
// fisheye does to them, the correct centre is the one that puts each boundary at
// the SAME image radius in every direction, so the cost is the mean normalised
// angular spread of the per-ring radius. Knowing the ring count is what makes it
// work: rays that find a different number of edges (bezel, monitor gap, glare)
// are dropped instead of being silently mis-indexed by one ring.
//
// The cost surface has a sharp ~15 px basin surrounded by local minima, so the
// search is a coarse grid first and a descent only afterwards -- a plain descent
// from a poor seed lands in the wrong basin.
struct RingCenter {
    cv::Point2d center{-1, -1};
    double cost = -1;            // mean of (angular std / mean) over the rings
    int ringsUsed = 0;
    int raysUsed = 0;            // rays that saw the full ring count (of raysTotal)
    int raysTotal = 0;
    std::vector<double> ringRadius;  // mean image radius of each ring, px
    std::vector<double> ringSpread;  // angular std of each ring, px

    // ---- per-ring angular harmonics of radius(theta), px ---------------------
    //
    // Least-squares fit of  r(theta) = a0 + a1 cos t + b1 sin t + a2 cos 2t + b2 sin 2t
    // about the candidate centre, one entry per KEPT ring, index-aligned with
    // ringRadius/ringSpread. Amplitudes are px; a phase is atan2(b, a) radians.
    //
    // Why these exist, given `cost` already reduces the same data to one number.
    //
    // ringSpread is the angular STD, which throws away the shape of the
    // variation and keeps only its size. That is the right summary for centring
    // and the wrong one for everything else, because two quite different rig
    // errors produce the same std:
    //
    //   * the camera is off-centre in X/Y -- the rings stay circular and shift
    //     bodily, which is a 1-lobed (cos/sin theta) variation of CONSTANT
    //     amplitude across rings;
    //   * the camera is tilted in pitch/yaw -- the rings become ellipses, which
    //     is a 2-lobed (cos/sin 2theta) variation, and the perspective keystone
    //     adds a 1-lobed part that grows roughly as R^2.
    //
    // From a single image the X/Y and pitch/yaw corrections are very nearly
    // degenerate: a small tilt and a small translation move the pattern almost
    // identically near the centre, and a centre-finder cannot tell them apart at
    // all -- it is built to be insensitive to exactly that. The ELLIPTICITY is
    // the tie-breaker, because translation cannot produce it.
    //
    // So the 1st and 2nd harmonics are exposed RAW and per ring, and no attempt
    // is made here to separate the centre-offset part of a1/b1 from the keystone
    // part. That decomposition depends on focal length, working distance and the
    // fisheye model, and getting it subtly wrong would bias every correction in
    // the same direction forever. A positioning loop identifies the Jacobian
    // empirically (Broyden) from measured responses, which absorbs the coupling
    // without anyone having to write it down.
    //
    // Cheap because it is conditional: computed only when a caller asks for the
    // detail block, i.e. in evaluate_ring_center and in the single final scoring
    // pass of find_center_from_rings -- never in the ~10^5 cost evaluations of
    // the search itself.
    std::vector<double> ringA1, ringB1;  // 1-lobed: centre offset + tilt keystone
    std::vector<double> ringA2, ringB2;  // 2-lobed: ellipticity, i.e. pitch/yaw

    bool ok() const { return center.x >= 0 && ringsUsed > 0; }
};

// gray: blurred single-channel image (see load_blurred_gray).
// expectedRings: boundary count from the generator; <= 0 means "use the modal
//   count found on the rays", which is less reliable — prefer passing it.
// seed: starting point, e.g. the gradient fit or the image centre.
// searchRadius: half-width of the coarse grid around the seed, px.
RingCenter find_center_from_rings(const cv::Mat &gray, int expectedRings,
                                  cv::Point2d seed, double searchRadius = 150.0);

// The cost above, at ONE point. The search, stopped before it searches.
//
// This exists to VALIDATE a centre that came from somewhere else. The gradient
// fit (the server's pattern_center op) answers in milliseconds and is right most
// of the time; what it cannot do is say how sure it is. It returns a point, and
// a point looks the same whether it is correct or three pixels out -- which is
// the failure that matters here, because the centre is the origin every ICT node
// is measured from, so a quiet three-pixel error corrupts a whole run while every
// number downstream still looks plausible.
//
// Evaluating the ring cost AT that point, and at a few points a short walk away,
// is what turns "here is a point" into "here is a point, and it beats its
// neighbourhood by twelve to one". The cost surface has a sharp ~15 px basin (see
// the note above find_center_from_rings), so a true centre wins its neighbourhood
// decisively and a plausible-looking miss does not.
//
// nRays is the only knob. 120 is enough to rank a neighbourhood; find_center_from_rings
// uses 720 for the number it finally reports. Returns a not-ok() RingCenter when
// the point has no usable rings at all -- that is an answer, not an error.
RingCenter evaluate_ring_center(const cv::Mat &gray, int expectedRings,
                                cv::Point2d at, int nRays = 120);

// The centre offset in PIXELS implied by a fit's per-ring angular spread.
//
// `cost` is dimensionless -- a mean of (angular std / mean radius) -- which makes
// it impossible to put a defensible threshold on. Nobody can say whether 0.0007 is
// good. But the geometry inverts. A centre displaced by d makes ring k's measured
// radius vary as R_k + d*cos(theta - phi), so the angular std of that ring is
// d/sqrt(2), independently of R_k. Therefore
//
//     d ~= sqrt(2) * ringSpread[k]
//
// and the median over rings is a robust estimate of how far off the centre is, in
// pixels -- a number an operator can be told and a threshold can be argued about
// ("within two pixels"), which the raw cost is not. The median rather than the
// mean because one bezel- or glare-contaminated ring should not move the answer.
//
// It reads HIGH, never low: crossing-detection jitter and any genuine lens or
// panel asymmetry land in ringSpread as well, so even an exact centre reports a
// few tenths of a pixel. That is the safe direction to be wrong in -- it escalates
// to the slower search rather than accepting a centre it should not. If a noise
// floor is ever measured (the min spread at a converged optimum on clean
// captures), subtract it in quadrature here rather than loosening the threshold.
//
// A spread that is roughly CONSTANT across rings is a genuine centre offset. One
// that grows with ringRadius is lens distortion or panel tilt, which moving the
// centre cannot fix -- worth reading the two arrays before tightening anything.
//
// Returns -1 for a fit with no rings.
double ring_center_offset_px(const RingCenter &r);

// Boundary radii of a generated concentric pattern, measured from its own PNG by
// walking out from the centre it was drawn about (w/2, h/2). Useful both to get
// the ring count without the generator's table and to report monitor-vs-image
// radius side by side.
std::vector<double> measure_pattern_ring_radii(const cv::Mat &patternGray);

std::map<std::string, std::vector<double>>
get_dict_8direction_intersecting_nodes_by_pos_neg_img_path(const std::string &posPath,
                                   const std::string &negPath,
                                   cv::Point posCenter, cv::Point negCenter);

// The same detection from already-blurred gray images (see blur_gray). The path
// version is a thin wrapper around this one, so the file-fed client path and the
// bytes-fed node path run identical code rather than two copies of it.
std::map<std::string, std::vector<double>>
get_dict_8direction_intersecting_nodes(const cv::Mat &posGray, const cv::Mat &negGray,
                                       cv::Point posCenter, cv::Point negCenter);

}  // namespace MoilCali
