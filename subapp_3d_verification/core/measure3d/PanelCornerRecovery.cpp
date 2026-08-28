#include "PanelCornerRecovery.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <utility>

#include <opencv2/calib3d.hpp>
#include <opencv2/imgproc.hpp>

#include "CbDetect.h"
#include "Moildev.h"
#include "moil_3d_algorithm.h"

namespace PanelCorner {
namespace {

cv::Mat ensureGray(const cv::Mat &image) {
    if (image.empty()) return cv::Mat();
    if (image.channels() == 1) return image;
    cv::Mat g;
    cv::cvtColor(image, g, cv::COLOR_BGR2GRAY);
    return g;
}

// _refine_subpix: cornerSubPix with a tight termination. Returns the input
// unchanged on OpenCV failure (matches the Python try/except).
std::vector<cv::Point2f> refineSubpix(const cv::Mat &gray, const std::vector<cv::Point2f> &pts,
                                      int windowHalf) {
    if (pts.empty()) return pts;
    std::vector<cv::Point2f> out = pts;
    const cv::TermCriteria crit(cv::TermCriteria::EPS + cv::TermCriteria::MAX_ITER, 40, 0.001);
    try {
        cv::cornerSubPix(gray, out, cv::Size(windowHalf, windowHalf), cv::Size(-1, -1), crit);
    } catch (const cv::Exception &) {
        return pts;
    }
    return out;
}

double median(std::vector<double> v) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    const size_t n = v.size();
    return (n % 2) ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
}

bool insideMargin(const cv::Point2f &p, int m, int w, int h) {
    return p.x >= m + 1 && p.y >= m + 1 && p.x < w - m - 1 && p.y < h - m - 1;
}

// Largest / smallest inner-corner grid the size scan below considers. Used as a
// bound on the square size when picking sub-pixel windows from the clicks alone.
constexpr int kMaxGrid = 17;
constexpr int kMinGrid = 3;

// cornerSubPix half-window that cannot reach the neighbouring corner: the
// shortest click edge spans at least (kMaxGrid-1) squares, so half a square is
// at worst minEdge / (2*(kMaxGrid-1)).
int clickWindowFor(double minEdge) {
    return static_cast<int>(std::clamp(minEdge / (2.0 * (kMaxGrid - 1)), 3.0, 15.0));
}

// Off-axis panels are darker (vignetting), softer (the lens MTF drops with
// alpha) and lower-contrast than the centre one, which is the second reason --
// after the curvature -- that the detector fails on them. CLAHE + unsharp is
// what makes the saddle points measurable again.
cv::Mat enhancePatch(const cv::Mat &gray) {
    cv::Mat eq;
    cv::createCLAHE(3.0, cv::Size(8, 8))->apply(gray, eq);
    cv::Mat blurred;
    cv::GaussianBlur(eq, blurred, cv::Size(0, 0), 1.2);
    cv::Mat sharp;
    cv::addWeighted(eq, 1.6, blurred, -0.6, 0, sharp);
    return sharp;
}

// Result of a size-free chessboard search: the grid that was found plus its
// corners in the coordinates of the patch that was searched.
struct BoardScan {
    bool ok = false;
    int cols = 0, rows = 0;
    std::vector<cv::Point2f> corners;  // row-major in the detector's own orientation
};

// Chessboard search that does not need the grid size up front.
//
// Every candidate (cols,rows) is ranked by how well (cols-1)/(rows-1) matches
// `aspect` -- reliable only when the patch is undistorted, hence the per-caller
// `aspectGate` -- and each is tried on a set of image variants: raw, contrast
// enhanced, and (for small patches, i.e. distant panels with only a handful of
// pixels per square) the same two upscaled, which is below what the SB detector
// can resolve otherwise. `minSpanFrac` rejects sub-grid matches that do not
// cover the patch.
BoardScan scanBoard(const cv::Mat &patch, double aspect, int hintRows, int hintCols,
                    double aspectGate, double minSpanFrac, int maxCandidates, long budgetMs) {
    BoardScan out;
    if (patch.empty() || patch.cols < 20 || patch.rows < 20) return out;

    struct Variant {
        cv::Mat img;
        double scale;
    };
    std::vector<Variant> variants{{patch, 1.0}, {enhancePatch(patch), 1.0}};
    const int shortSide = std::min(patch.cols, patch.rows);
    if (shortSide < 420) {
        const double s = std::min(3.0, 480.0 / std::max(shortSide, 1));
        cv::Mat up;
        cv::resize(patch, up, cv::Size(), s, s, cv::INTER_CUBIC);
        variants.push_back({up, s});
        variants.push_back({enhancePatch(up), s});
    }

    struct Cand {
        double aerr;
        int negArea, cols, rows;
    };
    std::vector<Cand> cands;
    for (int cols = kMinGrid; cols <= kMaxGrid; ++cols)
        for (int rows = kMinGrid; rows <= kMaxGrid - 3; ++rows) {
            const double ga = static_cast<double>(cols - 1) / std::max(rows - 1, 1);
            cands.push_back({std::abs(ga - aspect), -(cols * rows), cols, rows});
        }
    std::sort(cands.begin(), cands.end(), [](const Cand &a, const Cand &b) {
        return a.aerr != b.aerr ? a.aerr < b.aerr : a.negArea < b.negArea;
    });

    const int sbAccurate =
        cv::CALIB_CB_NORMALIZE_IMAGE | cv::CALIB_CB_EXHAUSTIVE | cv::CALIB_CB_ACCURACY;
    const int sbPlain = cv::CALIB_CB_NORMALIZE_IMAGE | cv::CALIB_CB_EXHAUSTIVE;
    const int classicFlags = cv::CALIB_CB_ADAPTIVE_THRESH | cv::CALIB_CB_NORMALIZE_IMAGE;

    // Accept a candidate only when its corners actually span the patch (a
    // sub-grid of the real board would cover a fraction of it) and write it out
    // in patch coordinates.
    auto accept = [&](const Variant &v, std::vector<cv::Point2f> &c, int cols, int rows) -> bool {
        float xmn = 1e9f, xmx = -1e9f, ymn = 1e9f, ymx = -1e9f;
        for (const auto &p : c) {
            xmn = std::min(xmn, p.x);
            xmx = std::max(xmx, p.x);
            ymn = std::min(ymn, p.y);
            ymx = std::max(ymx, p.y);
        }
        if ((xmx - xmn) / std::max(v.img.cols, 1) < minSpanFrac ||
            (ymx - ymn) / std::max(v.img.rows, 1) < minSpanFrac)
            return false;
        for (auto &p : c) {
            p.x = static_cast<float>(p.x / v.scale);
            p.y = static_cast<float>(p.y / v.scale);
        }
        out.ok = true;
        out.cols = cols;
        out.rows = rows;
        out.corners = c;
        return true;
    };

    // The classic detector finds boards that SB rejects, but it is far slower on
    // a miss, so it only runs for the sizes we actually believe in (the hint and
    // the best-fitting candidates) rather than for the whole sweep.
    auto tryOne = [&](const Variant &v, int cols, int rows, bool allowClassic) -> bool {
        if (cols < kMinGrid || rows < kMinGrid) return false;
        const int expect = cols * rows;
        std::vector<cv::Point2f> c;
        for (int flags : {sbAccurate, sbPlain}) {
            bool ok = false;
            try {
                ok = cv::findChessboardCornersSB(v.img, cv::Size(cols, rows), c, flags);
            } catch (const cv::Exception &) {
                ok = false;
            }
            if (ok && static_cast<int>(c.size()) == expect) return accept(v, c, cols, rows);
        }
        if (!allowClassic) return false;
        bool ok = false;
        try {
            ok = cv::findChessboardCorners(v.img, cv::Size(cols, rows), c, classicFlags);
        } catch (const cv::Exception &) {
            ok = false;
        }
        if (!ok || static_cast<int>(c.size()) != expect) return false;
        c = refineSubpix(v.img, c, 5);  // classic corners are only pixel-accurate
        return accept(v, c, cols, rows);
    };

    // Single-shot size discovery: findChessboardCornersSB with CALIB_CB_LARGER
    // reports the grid it found in `meta` instead of us guessing it, which skips
    // the whole candidate sweep when it works. Validated against the patch
    // aspect and span before it is trusted (OpenCV >= 4.3).
#if CV_VERSION_MAJOR > 4 || (CV_VERSION_MAJOR == 4 && CV_VERSION_MINOR >= 3)
    auto trySelfSizing = [&](const Variant &v) -> bool {
        std::vector<cv::Point2f> c;
        cv::Mat meta;
        bool ok = false;
        try {
            ok = cv::findChessboardCornersSB(v.img, cv::Size(kMinGrid, kMinGrid), c,
                                             cv::CALIB_CB_NORMALIZE_IMAGE | cv::CALIB_CB_EXHAUSTIVE |
                                                 cv::CALIB_CB_ACCURACY | cv::CALIB_CB_LARGER,
                                             meta);
        } catch (const cv::Exception &) {
            ok = false;
        }
        if (!ok || meta.empty()) return false;
        const int cols = meta.cols, rows = meta.rows;
        if (cols < kMinGrid || rows < kMinGrid) return false;
        if (static_cast<int>(c.size()) != cols * rows) return false;
        // The size comes back in the detector's own frame, which is a 90 deg
        // rotation of the panel as often as not, so check both orientations
        // against the patch aspect (the labelling downstream handles either).
        const double ga = static_cast<double>(cols - 1) / std::max(rows - 1, 1);
        if (std::min(std::abs(ga - aspect), std::abs(1.0 / ga - aspect)) > aspectGate) return false;
        return accept(v, c, cols, rows);
    };
#endif

    // This runs on the UI thread from the fourth click, so the sweep is bounded:
    // a panel that is simply not detectable must not freeze the dialog.
    const auto started = std::chrono::steady_clock::now();
    auto outOfTime = [&] {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now() - started)
                   .count() > budgetMs;
    };

    // Pass 1 -- the routes that cost one detector call: the caller's size hint,
    // then the self-sizing detector. Both are tried on every variant before the
    // sweep, which is an order of magnitude more expensive.
    for (const auto &v : variants) {
        if (hintCols >= kMinGrid && hintRows >= kMinGrid &&
            (tryOne(v, hintCols, hintRows, true) || tryOne(v, hintRows, hintCols, true)))
            return out;
#if CV_VERSION_MAJOR > 4 || (CV_VERSION_MAJOR == 4 && CV_VERSION_MINOR >= 3)
        if (trySelfSizing(v)) return out;
#endif
        if (outOfTime()) return out;
    }

    // Pass 2 -- sweep the plausible grid sizes.
    for (const auto &v : variants) {
        int tried = 0;
        for (const auto &cd : cands) {
            if (tried >= maxCandidates || outOfTime()) break;
            if (cd.aerr > aspectGate) continue;
            ++tried;
            if (tryOne(v, cd.cols, cd.rows, tried <= 3)) return out;
        }
        if (outOfTime()) break;
    }
    return out;
}

// Refine points that sit inside the image margin; reject refinements that drift
// further than `maxDrift` (keep the input there). Returns a same-size vector.
std::vector<cv::Point2f> refineInside(const cv::Mat &gray, const std::vector<cv::Point2f> &raw,
                                      int win, double maxDrift) {
    std::vector<cv::Point2f> res = raw;
    std::vector<cv::Point2f> in;
    std::vector<int> idx;
    for (int i = 0; i < static_cast<int>(raw.size()); ++i)
        if (insideMargin(raw[i], win, gray.cols, gray.rows)) {
            in.push_back(raw[i]);
            idx.push_back(i);
        }
    if (in.empty()) return res;
    const std::vector<cv::Point2f> r = refineSubpix(gray, in, win);
    for (size_t k = 0; k < r.size(); ++k)
        res[idx[k]] = (cv::norm(r[k] - in[k]) > maxDrift) ? in[k] : r[k];
    return res;
}

// Blank everything outside the clicked quad (expanded so the board keeps the
// squares its outermost inner corners need) with the panel's own mean grey. The
// planes of the calibration box are adjacent, so without this the detector can
// lock onto the neighbouring panel that shares the region of interest.
void maskOutsideQuad(cv::Mat &patch, const std::array<cv::Point2f, 4> &quad, double grow) {
    cv::Point2f c(0, 0);
    for (const auto &p : quad) c += p;
    c *= 0.25f;
    std::vector<cv::Point> poly;
    poly.reserve(4);
    for (const auto &p : quad) {
        const cv::Point2f g = c + (p - c) * static_cast<float>(grow);
        poly.emplace_back(cvRound(g.x), cvRound(g.y));
    }
    cv::Mat mask(patch.size(), CV_8U, cv::Scalar(0));
    cv::fillConvexPoly(mask, poly, cv::Scalar(255));
    cv::Mat bg(patch.size(), patch.type(), cv::mean(patch, mask));
    patch.copyTo(bg, mask);
    patch = bg;
}

// Give every detected corner its canonical (row,col) index by mapping the
// detection frame onto the click quad: the user always clicks TL,TR,BR,BL, so
// the same index comes out in the left and the right image and pairing by
// (row,col) is reliable. `pts` and `quad` must share a coordinate frame.
// Returns false when the labelling is degenerate (two corners claiming one
// cell) -- which is what a transposed grid looks like, so the caller can retry
// with cols/rows swapped.
bool labelCorners(const std::array<cv::Point2f, 4> &quad, const std::vector<cv::Point2f> &pts,
                  int cols, int rows, std::vector<std::pair<int, int>> &rc) {
    if (cols < 2 || rows < 2 || pts.empty()) return false;
    const std::vector<cv::Point2f> src(quad.begin(), quad.end());
    const std::vector<cv::Point2f> canon = {
        {0.f, 0.f},
        {static_cast<float>(cols - 1), 0.f},
        {static_cast<float>(cols - 1), static_cast<float>(rows - 1)},
        {0.f, static_cast<float>(rows - 1)}};
    cv::Mat H;
    try {
        H = cv::getPerspectiveTransform(src, canon);
    } catch (const cv::Exception &) {
        return false;
    }
    if (H.empty() || !cv::checkRange(H)) return false;
    std::vector<cv::Point2f> canonPts;
    cv::perspectiveTransform(pts, canonPts, H);

    rc.assign(pts.size(), {0, 0});
    std::vector<char> taken(static_cast<size_t>(rows) * cols, 0);
    for (size_t i = 0; i < pts.size(); ++i) {
        const int col = std::clamp(static_cast<int>(std::lround(canonPts[i].x)), 0, cols - 1);
        const int row = std::clamp(static_cast<int>(std::lround(canonPts[i].y)), 0, rows - 1);
        char &slot = taken[static_cast<size_t>(row) * cols + col];
        if (slot) return false;
        slot = 1;
        rc[i] = {row, col};
    }
    return true;
}

// A detected grid in the coordinate frame it was found in: the corner
// positions plus their canonical (row,col) index, which is what makes the left
// and the right image pair up.
struct GridResult {
    bool ok = false;
    int rows = 0, cols = 0;
    std::vector<cv::Point2f> pts;
    std::vector<std::pair<int, int>> rc;
};

// Run the libcbdetect port over `patch` and cut out the board the user clicked.
//
// The detector returns whole boards with their topology already established, so
// there is no homography to fit: find the board cell nearest each of the four
// clicks, keep the sub-grid those four cells span, and re-index it with the
// top-left click at (0,0). That also crops a board that grew past the clicked
// region, and picks the right one when the neighbouring panel was detected too.
// cropToQuad=false keeps the WHOLE board the detector grew, re-indexed so the
// smallest row/col is 0. The survey wants that: there the quad was derived from
// the board itself, so cropping to it can only shave off a ring the full-
// resolution pass found and the downscaled pass missed -- which would make the
// two libcbdetect layers incomparable. The click workflow keeps the crop, where
// the quad IS the user's statement of which panel they mean.
GridResult cbGridFromPatch(const cv::Mat &patch, const std::array<cv::Point2f, 4> &quad,
                           long budgetMs, bool cropToQuad = true) {
    GridResult out;
    CbDetect::Params params;
    params.budgetMs = budgetMs;
    const std::vector<CbDetect::Corner> corners = CbDetect::findCorners(patch, params);
    if (corners.size() < 9) return out;
    const std::vector<CbDetect::Board> boards = CbDetect::boardsFromCorners(corners, params);
    if (boards.empty()) return out;

    // Pick the board whose cells sit closest to the four clicks.
    const CbDetect::Board *chosen = nullptr;
    std::array<std::pair<int, int>, 4> chosenCell{};
    double chosenCost = std::numeric_limits<double>::infinity();
    for (const CbDetect::Board &b : boards) {
        double cost = 0;
        std::array<std::pair<int, int>, 4> cell{};
        for (int k = 0; k < 4; ++k) {
            double best = std::numeric_limits<double>::infinity();
            for (int r = 0; r < b.rows; ++r)
                for (int c = 0; c < b.cols; ++c) {
                    const double d = cv::norm(corners[b.at(r, c)].p - quad[k]);
                    if (d < best) {
                        best = d;
                        cell[k] = {r, c};
                    }
                }
            cost += best;
        }
        if (cost < chosenCost) {
            chosenCost = cost;
            chosen = &b;
            chosenCell = cell;
        }
    }
    if (!chosen) return out;

    // Which board axis runs along the quad's top edge (TL -> TR)?
    const int dRowU = chosenCell[1].first - chosenCell[0].first;
    const int dColU = chosenCell[1].second - chosenCell[0].second;
    const int dRowV = chosenCell[3].first - chosenCell[0].first;
    const int dColV = chosenCell[3].second - chosenCell[0].second;
    const bool uIsCol = std::abs(dColU) >= std::abs(dRowU);
    const int spanU = uIsCol ? dColU : dRowU;
    const int spanV = uIsCol ? dRowV : dColV;
    if (std::abs(spanU) < 2 || std::abs(spanV) < 2) return out;  // degenerate
    // The two edges must use different axes, or the four clicks were not read
    // as a quadrilateral on this board.
    if (uIsCol != (std::abs(dRowV) >= std::abs(dColV))) return out;

    const int cols = std::abs(spanU) + 1, rows = std::abs(spanV) + 1;
    const int su = spanU > 0 ? 1 : -1, sv = spanV > 0 ? 1 : -1;
    const int r0 = chosenCell[0].first, c0 = chosenCell[0].second;

    int minRow = std::numeric_limits<int>::max(), minCol = std::numeric_limits<int>::max();
    int maxRow = std::numeric_limits<int>::min(), maxCol = std::numeric_limits<int>::min();
    for (int r = 0; r < chosen->rows; ++r)
        for (int c = 0; c < chosen->cols; ++c) {
            const int col = su * ((uIsCol ? c - c0 : r - r0));
            const int row = sv * ((uIsCol ? r - r0 : c - c0));
            if (cropToQuad && (col < 0 || col >= cols || row < 0 || row >= rows)) continue;
            out.pts.push_back(corners[chosen->at(r, c)].p);
            out.rc.emplace_back(row, col);
            minRow = std::min(minRow, row);
            maxRow = std::max(maxRow, row);
            minCol = std::min(minCol, col);
            maxCol = std::max(maxCol, col);
        }
    if (out.pts.empty()) return out;

    if (cropToQuad) {
        out.rows = rows;
        out.cols = cols;
        out.ok = static_cast<int>(out.pts.size()) >= rows * cols;  // sub-grid must be complete
    } else {
        // Whole board: shift the indices so the outermost ring starts at (0,0).
        for (auto &rc : out.rc) {
            rc.first -= minRow;
            rc.second -= minCol;
        }
        out.rows = maxRow - minRow + 1;
        out.cols = maxCol - minCol + 1;
        out.ok = static_cast<int>(out.pts.size()) >= out.rows * out.cols;
    }
    return out;
}

}  // namespace

std::vector<Corner> recoverPanelCorners(const cv::Mat &image,
                                        const std::array<cv::Point2f, 4> &outerIn, int rows,
                                        int cols, bool refine, int subpixWindow) {
    std::vector<Corner> result;
    if (rows < 2 || cols < 2) return result;
    const cv::Mat gray = ensureGray(image);
    if (gray.empty()) return result;
    const int w = gray.cols, h = gray.rows;

    std::vector<cv::Point2f> outer(outerIn.begin(), outerIn.end());
    // Step 1 -- refine the 4 clicks with a generous window.
    if (refine) outer = refineSubpix(gray, outer, 15);

    // canonical (col,row): (0,0),(cols-1,0),(cols-1,rows-1),(0,rows-1)
    const std::vector<cv::Point2f> canonical = {{0.f, 0.f},
                                                {static_cast<float>(cols - 1), 0.f},
                                                {static_cast<float>(cols - 1),
                                                 static_cast<float>(rows - 1)},
                                                {0.f, static_cast<float>(rows - 1)}};

    // Step 2 -- initial homography.
    cv::Mat H = cv::findHomography(canonical, outer, 0);
    if (H.empty() || !cv::checkRange(H)) return result;

    // Every (col,row) inner-corner coordinate, row-major.
    std::vector<cv::Point2f> grid;
    grid.reserve(static_cast<size_t>(rows) * cols);
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < cols; ++c) grid.emplace_back(static_cast<float>(c), static_cast<float>(r));

    std::vector<cv::Point2f> predicted;
    cv::perspectiveTransform(grid, predicted, H);

    std::vector<char> inside(grid.size(), 0);

    if (refine) {
        // Step 3 -- coarse refinement with an adaptive window sized to the
        // panel's local grid spacing (so we never snap onto a neighbour corner).
        const double edges[4] = {cv::norm(outer[1] - outer[0]), cv::norm(outer[2] - outer[3]),
                                 cv::norm(outer[3] - outer[0]), cv::norm(outer[2] - outer[1])};
        const double avgEdge = (edges[0] + edges[1] + edges[2] + edges[3]) / 4.0;
        const double avgSteps = std::max((cols + rows) / 2.0 - 1.0, 1.0);
        const double avgSpacing = avgEdge / avgSteps;
        const int coarseWindow = static_cast<int>(std::clamp(avgSpacing * 0.4, 10.0, 30.0));

        std::vector<int> insideIdx;  // global grid indices inside the coarse margin
        std::vector<cv::Point2f> coarsePts;
        for (int i = 0; i < static_cast<int>(predicted.size()); ++i)
            if (insideMargin(predicted[i], coarseWindow, w, h)) {
                insideIdx.push_back(i);
                coarsePts.push_back(predicted[i]);
            }
        if (coarsePts.empty()) return result;

        const std::vector<cv::Point2f> refinedCoarse = refineSubpix(gray, coarsePts, coarseWindow);

        std::vector<double> drift(refinedCoarse.size());
        for (size_t i = 0; i < drift.size(); ++i) drift[i] = cv::norm(refinedCoarse[i] - coarsePts[i]);
        const double driftThresh = std::max(3.0 * median(drift) + 4.0, coarseWindow * 1.4);

        std::vector<char> coarseBad(refinedCoarse.size(), 0);
        int goodCount = 0;
        for (size_t i = 0; i < drift.size(); ++i) {
            coarseBad[i] = drift[i] > driftThresh ? 1 : 0;
            if (!coarseBad[i]) ++goodCount;
        }

        // Step 4 -- re-fit the homography from the good coarse corners (RANSAC),
        // then re-predict ALL corners from the better-fitting transform.
        if (goodCount >= 6) {
            std::vector<cv::Point2f> srcGood, dstGood;
            for (size_t i = 0; i < refinedCoarse.size(); ++i)
                if (!coarseBad[i]) {
                    srcGood.push_back(grid[insideIdx[i]]);
                    dstGood.push_back(refinedCoarse[i]);
                }
            cv::Mat Href = cv::findHomography(srcGood, dstGood, cv::RANSAC, 3.0);
            if (!Href.empty() && cv::checkRange(Href))
                cv::perspectiveTransform(grid, predicted, Href);
        }

        // Step 5 -- fine refinement with the small window for sub-pixel accuracy;
        // reject refinements that drifted too far (keep the prediction there).
        std::vector<int> fineIdx;
        std::vector<cv::Point2f> finePts;
        for (int i = 0; i < static_cast<int>(predicted.size()); ++i)
            if (insideMargin(predicted[i], subpixWindow, w, h)) {
                inside[i] = 1;
                fineIdx.push_back(i);
                finePts.push_back(predicted[i]);
            }
        if (!finePts.empty()) {
            std::vector<cv::Point2f> refinedFine = refineSubpix(gray, finePts, subpixWindow);
            for (size_t k = 0; k < refinedFine.size(); ++k) {
                if (cv::norm(refinedFine[k] - finePts[k]) > 2.5 * subpixWindow)
                    refinedFine[k] = finePts[k];  // rejected: keep prediction
                predicted[fineIdx[k]] = refinedFine[k];
            }
        }
    } else {
        for (int i = 0; i < static_cast<int>(predicted.size()); ++i)
            inside[i] = insideMargin(predicted[i], subpixWindow, w, h) ? 1 : 0;
    }

    // Emit row-major.
    int idx = 0;
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < cols; ++c, ++idx)
            if (inside[idx]) result.push_back({r, c, predicted[idx]});
    return result;
}

PanelDetection detectPanelOriginal(const cv::Mat &image, const std::array<cv::Point2f, 4> &outerIn,
                                   int hintRows, int hintCols) {
    PanelDetection out;
    const cv::Mat gray = ensureGray(image);
    if (gray.empty()) return out;
    const int W = gray.cols, H = gray.rows;

    std::vector<cv::Point2f> outer(outerIn.begin(), outerIn.end());
    // Snap the clicks to the real corners with a window that cannot reach the
    // neighbouring one: a fixed 15 (a 31x31 search) jumps a whole square on the
    // off-axis panels, where the squares are compressed to a few pixels.
    double minEdge = 1e18;
    for (int i = 0; i < 4; ++i) minEdge = std::min(minEdge, cv::norm(outer[(i + 1) % 4] - outer[i]));
    outer = refineSubpix(gray, outer, clickWindowFor(minEdge));

    // Padded bounding box of the 4 clicks = the region we detect in.
    float xmn = 1e9f, xmx = -1e9f, ymn = 1e9f, ymx = -1e9f;
    for (const auto &p : outer) {
        xmn = std::min(xmn, p.x);
        xmx = std::max(xmx, p.x);
        ymn = std::min(ymn, p.y);
        ymx = std::max(ymx, p.y);
    }
    const float bw = xmx - xmn, bh = ymx - ymn;
    if (bw < 10 || bh < 10) return out;
    // 25% keeps the square that lies beyond each outermost inner corner inside
    // the region of interest even for a coarse board.
    const int pad = static_cast<int>(std::max(20.0f, 0.25f * std::max(bw, bh)));
    const int x0 = std::max(0, static_cast<int>(std::floor(xmn)) - pad);
    const int y0 = std::max(0, static_cast<int>(std::floor(ymn)) - pad);
    const int x1 = std::min(W, static_cast<int>(std::ceil(xmx)) + pad);
    const int y1 = std::min(H, static_cast<int>(std::ceil(ymx)) + pad);
    if (x1 - x0 < 10 || y1 - y0 < 10) return out;
    const cv::Rect roi(x0, y0, x1 - x0, y1 - y0);
    cv::Mat sub = gray(roi).clone();

    // Blank the neighbouring panels that share the bounding box (the clicked
    // quad is rotated on the off-axis views, so its box holds a lot of them).
    const std::array<cv::Point2f, 4> quadRoi{outer[0] - cv::Point2f(x0, y0),
                                             outer[1] - cv::Point2f(x0, y0),
                                             outer[2] - cv::Point2f(x0, y0),
                                             outer[3] - cv::Point2f(x0, y0)};
    maskOutsideQuad(sub, quadRoi, 1.6);

    const double wavg = 0.5 * (cv::norm(outer[1] - outer[0]) + cv::norm(outer[2] - outer[3]));
    const double havg = 0.5 * (cv::norm(outer[3] - outer[0]) + cv::norm(outer[2] - outer[1]));
    // Wide gate: this aspect is measured on the distorted image, so it is only a
    // weak hint about the grid shape.
    const double aspect = havg > 1e-6 ? wavg / havg : 1.0;

    const BoardScan scan = scanBoard(sub, aspect, hintRows, hintCols, 3.0, 0.5, 30, 1500);
    if (!scan.ok) return out;

    std::vector<cv::Point2f> found = scan.corners;
    for (auto &p : found) {  // ROI -> full-image coords
        p.x += x0;
        p.y += y0;
    }
    // Canonical (row,col) from the 4 clicks (labelling only; the positions come
    // straight from the on-original detection).
    const std::array<cv::Point2f, 4> quad{outer[0], outer[1], outer[2], outer[3]};
    std::vector<std::pair<int, int>> rc;
    int cols = scan.cols, rows = scan.rows;
    if (!labelCorners(quad, found, cols, rows, rc)) {
        std::swap(cols, rows);  // grid detected transposed to the clicked quad
        if (!labelCorners(quad, found, cols, rows, rc)) return out;
    }

    out.rows = rows;
    out.cols = cols;
    for (size_t i = 0; i < found.size(); ++i)
        out.corners.push_back({rc[i].first, rc[i].second, found[i]});
    return out;
}

PanelDetection detectPanelOriginalCb(const cv::Mat &image, const std::array<cv::Point2f, 4> &outerIn,
                                     int, int, bool cropToQuad) {
    PanelDetection out;
    const cv::Mat gray = ensureGray(image);
    if (gray.empty()) return out;
    const int W = gray.cols, H = gray.rows;

    std::vector<cv::Point2f> outer(outerIn.begin(), outerIn.end());
    double minEdge = 1e18;
    for (int i = 0; i < 4; ++i) minEdge = std::min(minEdge, cv::norm(outer[(i + 1) % 4] - outer[i]));
    if (minEdge < 20) return out;
    outer = refineSubpix(gray, outer, clickWindowFor(minEdge));

    float xmn = 1e9f, xmx = -1e9f, ymn = 1e9f, ymx = -1e9f;
    for (const auto &p : outer) {
        xmn = std::min(xmn, p.x);
        xmx = std::max(xmx, p.x);
        ymn = std::min(ymn, p.y);
        ymx = std::max(ymx, p.y);
    }
    const float bw = xmx - xmn, bh = ymx - ymn;
    if (bw < 10 || bh < 10) return out;
    const int pad = static_cast<int>(std::max(20.0f, 0.25f * std::max(bw, bh)));
    const int x0 = std::max(0, static_cast<int>(std::floor(xmn)) - pad);
    const int y0 = std::max(0, static_cast<int>(std::floor(ymn)) - pad);
    const int x1 = std::min(W, static_cast<int>(std::ceil(xmx)) + pad);
    const int y1 = std::min(H, static_cast<int>(std::ceil(ymx)) + pad);
    if (x1 - x0 < 10 || y1 - y0 < 10) return out;
    cv::Mat sub = gray(cv::Rect(x0, y0, x1 - x0, y1 - y0)).clone();

    const std::array<cv::Point2f, 4> quad{outer[0] - cv::Point2f(x0, y0),
                                          outer[1] - cv::Point2f(x0, y0),
                                          outer[2] - cv::Point2f(x0, y0),
                                          outer[3] - cv::Point2f(x0, y0)};
    maskOutsideQuad(sub, quad, 1.6);

    const GridResult grid = cbGridFromPatch(sub, quad, 4000, cropToQuad);
    if (!grid.ok) return out;

    // No extra polish: the detector's own refinement is a Foerstner solve on
    // these very pixels, which is what cornerSubPix would redo less carefully.
    out.rows = grid.rows;
    out.cols = grid.cols;
    for (size_t i = 0; i < grid.pts.size(); ++i) {
        const cv::Point2f p = grid.pts[i] + cv::Point2f(x0, y0);
        if (p.x < 0 || p.y < 0 || p.x >= W || p.y >= H) continue;
        out.corners.push_back({grid.rc[i].first, grid.rc[i].second, p});
    }
    if (out.corners.empty()) return PanelDetection{};
    return out;
}

std::vector<SurveyLayer> surveyImage(const cv::Mat &image, int maxDetectPixels) {
    std::vector<SurveyLayer> out;
    const cv::Mat gray = ensureGray(image);
    if (gray.empty()) return out;

    // The corner likelihood is a stack of large convolutions, so a full 9 MP
    // fisheye frame is scaled down for the search; the quads it produces are
    // scaled back up and every method re-detects at full resolution.
    double scale = 1.0;
    if (static_cast<double>(gray.total()) > maxDetectPixels)
        scale = std::sqrt(maxDetectPixels / static_cast<double>(gray.total()));
    cv::Mat small;
    if (scale < 1.0)
        cv::resize(gray, small, cv::Size(), scale, scale, cv::INTER_AREA);
    else
        small = gray;
    const double inv = 1.0 / scale;

    CbDetect::Params params;
    params.budgetMs = 8000;

    auto clockMs = [](const std::chrono::steady_clock::time_point &t) {
        return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t)
            .count();
    };

    // Names carry the two things that actually differ between these layers --
    // WHERE the detector ran and at WHICH resolution -- because every layer but
    // the last is the same libcbdetect and a bare method name explains nothing.
    auto sizeTag = [](const cv::Mat &m, double s) {
        std::ostringstream os;
        os << m.cols << "x" << m.rows << " px";
        if (s < 0.999) os << " (" << std::fixed << std::setprecision(2) << s << "x downscaled)";
        return os.str();
    };
    const std::string wholeTag = "whole frame @ " + sizeTag(small, scale);
    const std::string fullTag = "per board @ " + sizeTag(gray, 1.0);

    // Layer 0 -- every corner candidate, before any board structure.
    auto t0 = std::chrono::steady_clock::now();
    const std::vector<CbDetect::Corner> corners = CbDetect::findCorners(small, params);
    SurveyLayer raw;
    raw.name = "libcbdetect corners, " + wholeTag + ", no board growth";
    raw.ms = clockMs(t0);
    raw.points.reserve(corners.size());
    for (const auto &c : corners)
        raw.points.emplace_back(static_cast<float>(c.p.x * inv), static_cast<float>(c.p.y * inv));
    out.push_back(raw);

    // Layer 1 -- the boards those candidates form.
    t0 = std::chrono::steady_clock::now();
    const std::vector<CbDetect::Board> boards = CbDetect::boardsFromCorners(corners, params);
    SurveyLayer structured;
    structured.name = "libcbdetect boards, " + wholeTag +
                      (scale < 0.999 ? ", positions scaled back up" : "");
    structured.ms = clockMs(t0);
    structured.boards = static_cast<int>(boards.size());
    structured.structured = true;
    std::vector<std::array<cv::Point2f, 4>> quads;
    for (const auto &b : boards) {
        for (int i : b.idx)
            structured.points.emplace_back(static_cast<float>(corners[i].p.x * inv),
                                           static_cast<float>(corners[i].p.y * inv));
        // The four outer corners of the board, put into a consistent winding so
        // they read as the TL,TR,BR,BL a user would have clicked.
        std::array<cv::Point2f, 4> q{corners[b.at(0, 0)].p, corners[b.at(0, b.cols - 1)].p,
                                     corners[b.at(b.rows - 1, b.cols - 1)].p,
                                     corners[b.at(b.rows - 1, 0)].p};
        cv::Point2f centre(0, 0);
        for (auto &p : q) {
            p *= static_cast<float>(inv);
            centre += p;
        }
        centre *= 0.25f;
        std::sort(q.begin(), q.end(), [&](const cv::Point2f &a, const cv::Point2f &b2) {
            return std::atan2(a.y - centre.y, a.x - centre.x) <
                   std::atan2(b2.y - centre.y, b2.x - centre.x);
        });
        quads.push_back(q);  // clockwise from the most negative angle = top-left-ish
    }
    out.push_back(structured);

    // Layers 2.. -- hand each board's quad to every method, exactly as if the
    // four corners had been clicked.
    for (Method m : {Method::OriginalCb, Method::OriginalSb}) {
        SurveyLayer layer;
        layer.name = std::string(methodName(m)) + " boards, " + fullTag;
        layer.structured = true;
        const auto t = std::chrono::steady_clock::now();
        // The panels are independent, and a method that fails burns its whole
        // search budget on every one of them, so run them side by side.
        // cropToQuad=false: keep every corner found at full resolution instead of
        // trimming to the quad layer 1 handed over, so the counts here and in
        // layer 1 measure the same thing and can be compared directly.
        std::vector<PanelDetection> perQuad(quads.size());
        cv::parallel_for_(cv::Range(0, static_cast<int>(quads.size())), [&](const cv::Range &r) {
            for (int i = r.start; i < r.end; ++i)
                perQuad[i] = recoverPanel(image, quads[i], 0, 0, m, /*cropToQuad=*/false);
        });
        for (const auto &d : perQuad) {
            if (d.corners.empty()) continue;
            ++layer.boards;
            for (const auto &c : d.corners) layer.points.push_back(c.pt);
        }
        layer.ms = clockMs(t);
        out.push_back(layer);
    }
    return out;
}

const char *methodName(Method m) {
    switch (m) {
        case Method::Auto: return "auto (libcbdetect, then OpenCV SB)";
        case Method::OriginalSb: return "OpenCV SB";
        case Method::OriginalCb: return "libcbdetect";
    }
    return "?";
}

PanelDetection recoverPanel(const cv::Mat &image, const std::array<cv::Point2f, 4> &outer,
                            int hintRows, int hintCols, Method method, bool cropToQuad) {
    auto run = [&](Method m) -> PanelDetection {
        // cropToQuad only reaches the libcbdetect path: the SB path takes
        // whatever findChessboardCornersSB returns for the region and never
        // trims it to the quad, so it has nothing to switch off.
        return m == Method::OriginalCb
                   ? detectPanelOriginalCb(image, outer, hintRows, hintCols, cropToQuad)
                   : detectPanelOriginal(image, outer, hintRows, hintCols);
    };
    if (method != Method::Auto) return run(method);
    // libcbdetect first: on real rig frames (5-panel box, 280 deg lens) it found
    // every panel where the SB detector found one or none, and in the synthetic
    // bench it was at least as accurate everywhere (0.063 px vs 0.095 px at 65
    // deg off-axis). Neither path resamples the image -- corners are measured
    // once, on the pixels the camera produced.
    for (Method m : {Method::OriginalCb, Method::OriginalSb}) {
        PanelDetection d = run(m);
        if (!d.corners.empty()) return d;
    }
    return PanelDetection{};
}

StereoPanel recoverPanelStereoFisheye(const cv::Mat &imgL, const cv::Mat &imgR,
                                      const std::array<cv::Point2f, 4> &outerLIn,
                                      const std::array<cv::Point2f, 4> &outerRIn, int rows, int cols,
                                      const Moildev &moilL, const Moildev &moilR,
                                      const Eigen::Vector3d &camL, const Eigen::Vector3d &camR) {
    namespace M = Moil3dAlgorithm;
    StereoPanel out;
    if (rows < 2 || cols < 2) return out;
    const cv::Mat gL = ensureGray(imgL), gR = ensureGray(imgR);
    if (gL.empty() || gR.empty()) return out;
    if ((camL - camR).norm() < 1e-6) return out;  // no baseline -> can't triangulate

    std::vector<cv::Point2f> oL(outerLIn.begin(), outerLIn.end());
    std::vector<cv::Point2f> oR(outerRIn.begin(), outerRIn.end());
    oL = refineSubpix(gL, oL, 15);
    oR = refineSubpix(gR, oR, 15);

    // Triangulate the 4 outer clicks to 3D corners (TL,TR,BR,BL).
    for (int k = 0; k < 4; ++k) {
        bool okL = false, okR = false;
        const auto abL = moilL.getAlphaBeta(oL[k].x, oL[k].y, 1, okL);
        const auto abR = moilR.getAlphaBeta(oR[k].x, oR[k].y, 1, okR);
        if (!okL || !okR) return out;
        const M::TriFull tri =
            M::triangulateAlignedFromAlphaBeta(abL.first, abL.second, abR.first, abR.second, camL,
                                               camR);
        out.corners3d[k] = tri.mid;
    }

    const M::Reprojector3d rpL(&moilL, camL);
    const M::Reprojector3d rpR(&moilR, camR);
    auto bilinear = [&](double u, double v) -> Eigen::Vector3d {
        return (1 - u) * (1 - v) * out.corners3d[0] + u * (1 - v) * out.corners3d[1] +
               u * v * out.corners3d[2] + (1 - u) * v * out.corners3d[3];
    };

    // Project every inner corner through each fisheye model.
    out.left.rows = out.right.rows = rows;
    out.left.cols = out.right.cols = cols;
    std::vector<std::pair<int, int>> rc;
    std::vector<cv::Point2f> Lraw, Rraw;
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < cols; ++c) {
            const double u = cols > 1 ? static_cast<double>(c) / (cols - 1) : 0.0;
            const double v = rows > 1 ? static_cast<double>(r) / (rows - 1) : 0.0;
            const Eigen::Vector3d p3 = bilinear(u, v);
            bool okl = false, okr = false;
            const auto pl = rpL.reprojectPoint(p3, okl);
            const auto pr = rpR.reprojectPoint(p3, okr);
            if (!okl || !okr) continue;
            rc.push_back({r, c});
            Lraw.emplace_back(static_cast<float>(pl.first), static_cast<float>(pl.second));
            Rraw.emplace_back(static_cast<float>(pr.first), static_cast<float>(pr.second));
        }
    const std::vector<cv::Point2f> Lref = refineInside(gL, Lraw, 7, 2.5 * 7);
    const std::vector<cv::Point2f> Rref = refineInside(gR, Rraw, 7, 2.5 * 7);
    for (size_t i = 0; i < rc.size(); ++i) {
        if (Lref[i].x >= 0 && Lref[i].y >= 0 && Lref[i].x < gL.cols && Lref[i].y < gL.rows)
            out.left.corners.push_back({rc[i].first, rc[i].second, Lref[i]});
        if (Rref[i].x >= 0 && Rref[i].y >= 0 && Rref[i].x < gR.cols && Rref[i].y < gR.rows)
            out.right.corners.push_back({rc[i].first, rc[i].second, Rref[i]});
    }

    // Curved boundary: sample the 4 edges in 3D and project each sample.
    auto buildBoundary = [&](const M::Reprojector3d &rp) {
        std::vector<cv::Point2f> poly;
        const int seg = 20;
        auto addEdge = [&](double u0, double v0, double u1, double v1) {
            for (int s = 0; s <= seg; ++s) {
                const double t = static_cast<double>(s) / seg;
                bool ok = false;
                const auto p = rp.reprojectPoint(bilinear(u0 + (u1 - u0) * t, v0 + (v1 - v0) * t), ok);
                if (ok) poly.emplace_back(static_cast<float>(p.first), static_cast<float>(p.second));
            }
        };
        addEdge(0, 0, 1, 0);  // TL -> TR
        addEdge(1, 0, 1, 1);  // TR -> BR
        addEdge(1, 1, 0, 1);  // BR -> BL
        addEdge(0, 1, 0, 0);  // BL -> TL
        return poly;
    };
    out.boundaryLeft = buildBoundary(rpL);
    out.boundaryRight = buildBoundary(rpR);

    out.ok = !out.left.corners.empty() && !out.right.corners.empty();
    return out;
}

}  // namespace PanelCorner
