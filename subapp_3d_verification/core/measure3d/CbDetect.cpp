// C++ port of libcbdetect (Geiger et al., ICRA 2012). See CbDetect.h for the
// citation and the GPLv3 notice inherited from the original MATLAB source.
//
// The port follows the MATLAB reference file by file: findCorners.m,
// createCorrelationPatch.m, nonMaximumSuppression.m, refineCorners.m,
// findModesMeanShift.m, scoreCorners.m, cornerCorrelationScore.m,
// chessboardsFromCorners.m, initChessboard.m, growChessboard.m and
// chessboardEnergy.m. Deviations from the MATLAB are marked "NOTE".

#include "CbDetect.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <numeric>

#include <opencv2/imgproc.hpp>

namespace CbDetect {
namespace {

constexpr double kPi = 3.14159265358979323846;

// --- createCorrelationPatch.m --------------------------------------------
// The four quadrant kernels of a checkerboard prototype cut by two lines at
// `angle1` / `angle2`, each weighted by a Gaussian of sigma = radius/2.
struct Patch {
    cv::Mat a1, a2, b1, b2;
};

Patch createCorrelationPatch(double angle1, double angle2, int radius) {
    const int side = radius * 2 + 1;
    Patch t;
    t.a1 = cv::Mat::zeros(side, side, CV_32F);
    t.a2 = cv::Mat::zeros(side, side, CV_32F);
    t.b1 = cv::Mat::zeros(side, side, CV_32F);
    t.b2 = cv::Mat::zeros(side, side, CV_32F);

    const double n1x = -std::sin(angle1), n1y = std::cos(angle1);
    const double n2x = -std::sin(angle2), n2y = std::cos(angle2);
    const double sigma = radius / 2.0;

    for (int v = 0; v < side; ++v)
        for (int u = 0; u < side; ++u) {
            const double du = u - radius, dv = v - radius;
            const double dist = std::hypot(du, dv);
            const double s1 = du * n1x + dv * n1y;
            const double s2 = du * n2x + dv * n2y;
            const float w =
                static_cast<float>(std::exp(-dist * dist / (2.0 * sigma * sigma)));
            if (s1 <= -0.1 && s2 <= -0.1)
                t.a1.at<float>(v, u) = w;
            else if (s1 >= 0.1 && s2 >= 0.1)
                t.a2.at<float>(v, u) = w;
            else if (s1 <= -0.1 && s2 >= 0.1)
                t.b1.at<float>(v, u) = w;
            else if (s1 >= 0.1 && s2 <= -0.1)
                t.b2.at<float>(v, u) = w;
        }

    for (cv::Mat *m : {&t.a1, &t.a2, &t.b1, &t.b2}) {
        const double s = cv::sum(*m)[0];
        if (s > 1e-12) *m /= s;
    }
    return t;
}

// --- findModesMeanShift.m -------------------------------------------------
// Mean shift on a circular histogram, approximated by smoothing then hill
// climbing from every bin. Returns (bin, height) sorted by height, descending.
std::vector<std::pair<int, double>> findModesMeanShift(const std::vector<double> &hist,
                                                       double sigma) {
    const int n = static_cast<int>(hist.size());
    std::vector<double> sm(n, 0.0);
    const int half = static_cast<int>(std::lround(2 * sigma));
    for (int i = 0; i < n; ++i)
        for (int j = -half; j <= half; ++j) {
            const int k = ((i + j) % n + n) % n;
            sm[i] += hist[k] * std::exp(-j * j / (2.0 * sigma * sigma));
        }

    std::vector<std::pair<int, double>> modes;
    // A flat histogram would make the hill climb below run forever.
    bool flat = true;
    for (int i = 0; i < n && flat; ++i) flat = std::abs(sm[i] - sm[0]) < 1e-5;
    if (flat) return modes;

    std::vector<char> seen(n, 0);
    for (int i = 0; i < n; ++i) {
        int j = i;
        for (int guard = 0; guard < 2 * n; ++guard) {
            const double h0 = sm[j];
            const int j1 = (j + 1) % n, j2 = (j - 1 + n) % n;
            const double h1 = sm[j1], h2 = sm[j2];
            if (h1 >= h0 && h1 >= h2)
                j = j1;
            else if (h2 > h0 && h2 > h1)
                j = j2;
            else
                break;
        }
        if (!seen[j]) {
            seen[j] = 1;
            modes.emplace_back(j, sm[j]);
        }
    }
    std::sort(modes.begin(), modes.end(),
              [](const std::pair<int, double> &a, const std::pair<int, double> &b) {
                  return a.second > b.second;
              });
    return modes;
}

// --- refineCorners.m / edgeOrientations -----------------------------------
// The two dominant edge directions in a window, from a gradient-weighted
// histogram of edge angles. Returns false when fewer than two well-separated
// modes exist (i.e. this is not a corner).
bool edgeOrientations(const cv::Mat &angle, const cv::Mat &weight, cv::Point2f &v1,
                      cv::Point2f &v2) {
    constexpr int kBins = 32;
    std::vector<double> hist(kBins, 0.0);
    for (int y = 0; y < angle.rows; ++y) {
        const float *a = angle.ptr<float>(y);
        const float *w = weight.ptr<float>(y);
        for (int x = 0; x < angle.cols; ++x) {
            // Normal angle -> edge direction, folded back into [0, pi).
            double t = a[x] + kPi / 2;
            if (t > kPi) t -= kPi;
            int bin = static_cast<int>(std::floor(t / (kPi / kBins)));
            bin = std::clamp(bin, 0, kBins - 1);
            hist[bin] += w[x];
        }
    }

    const auto modes = findModesMeanShift(hist, 1.0);
    if (modes.size() <= 1) return false;

    double a1 = modes[0].first * kPi / kBins;
    double a2 = modes[1].first * kPi / kBins;
    if (a1 > a2) std::swap(a1, a2);
    const double delta = std::min(a2 - a1, a1 + kPi - a2);
    if (delta <= 0.3) return false;

    v1 = cv::Point2f(static_cast<float>(std::cos(a1)), static_cast<float>(std::sin(a1)));
    v2 = cv::Point2f(static_cast<float>(std::cos(a2)), static_cast<float>(std::sin(a2)));
    return true;
}

// Eigenvector of the SMALLEST eigenvalue of the symmetric [[a,b],[b,c]] --
// MATLAB's eig() returns eigenvalues ascending and the code takes column 1.
// For a gradient scatter matrix this is the direction of least gradient
// variation, i.e. the edge direction.
cv::Point2f smallestEigenVector(double a, double b, double c) {
    const double tr = a + c, det = std::sqrt((a - c) * (a - c) + 4 * b * b);
    const double lo = 0.5 * (tr - det);
    cv::Point2f v(static_cast<float>(b), static_cast<float>(lo - a));
    if (std::abs(b) < 1e-12 && std::abs(lo - a) < 1e-12)
        v = cv::Point2f(static_cast<float>(lo - c), static_cast<float>(b));
    const double n = std::hypot(v.x, v.y);
    if (n < 1e-12) return {0.f, 0.f};
    return {static_cast<float>(v.x / n), static_cast<float>(v.y / n)};
}

// --- cornerCorrelationScore.m ---------------------------------------------
double cornerCorrelationScore(const cv::Mat &img, const cv::Mat &weight, const cv::Point2f &v1,
                              const cv::Point2f &v2) {
    const int side = weight.rows;
    const double c = (side - 1) / 2.0;

    // Gradient score: how well the gradient magnitude follows the two edges.
    std::vector<double> vw, vf;
    vw.reserve(side * side);
    vf.reserve(side * side);
    for (int y = 0; y < side; ++y)
        for (int x = 0; x < side; ++x) {
            const double px = x - c, py = y - c;
            const double t1 = px * v1.x + py * v1.y;
            const double t2 = px * v2.x + py * v2.y;
            const double d1 = std::hypot(px - t1 * v1.x, py - t1 * v1.y);
            const double d2 = std::hypot(px - t2 * v2.x, py - t2 * v2.y);
            vf.push_back((d1 <= 1.5 || d2 <= 1.5) ? 1.0 : -1.0);
            vw.push_back(weight.at<float>(y, x));
        }
    auto zscore = [](std::vector<double> &v) {
        const double n = static_cast<double>(v.size());
        const double mean = std::accumulate(v.begin(), v.end(), 0.0) / n;
        double var = 0;
        for (double x : v) var += (x - mean) * (x - mean);
        var /= (n - 1);  // MATLAB std(): N-1
        const double sd = std::sqrt(var);
        for (double &x : v) x = sd > 1e-12 ? (x - mean) / sd : 0.0;
    };
    zscore(vw);
    zscore(vf);
    double dot = 0;
    for (size_t i = 0; i < vw.size(); ++i) dot += vw[i] * vf[i];
    const double scoreGradient = std::max(dot / (static_cast<double>(vw.size()) - 1), 0.0);

    // Intensity score: the checkerboard prototype response at this orientation.
    const Patch t = createCorrelationPatch(std::atan2(v1.y, v1.x), std::atan2(v2.y, v2.x),
                                           (side - 1) / 2);
    const double a1 = img.dot(t.a1), a2 = img.dot(t.a2);
    const double b1 = img.dot(t.b1), b2 = img.dot(t.b2);
    const double mu = (a1 + a2 + b1 + b2) / 4;
    const double s1 = std::min(std::min(a1 - mu, a2 - mu), std::min(mu - b1, mu - b2));
    const double s2 = std::min(std::min(mu - a1, mu - a2), std::min(b1 - mu, b2 - mu));
    const double scoreIntensity = std::max(std::max(s1, s2), 0.0);

    return scoreGradient * scoreIntensity;
}

// --- nonMaximumSuppression.m ----------------------------------------------
std::vector<cv::Point> nonMaximumSuppression(const cv::Mat &img, int n, double tau, int margin) {
    const int width = img.cols, height = img.rows;
    std::vector<cv::Point> maxima;
    for (int i = n + margin; i <= width - n - margin - 1; i += n + 1)
        for (int j = n + margin; j <= height - n - margin - 1; j += n + 1) {
            int maxi = i, maxj = j;
            float maxval = img.at<float>(j, i);
            for (int i2 = i; i2 <= i + n; ++i2)
                for (int j2 = j; j2 <= j + n; ++j2)
                    if (img.at<float>(j2, i2) > maxval) {
                        maxi = i2;
                        maxj = j2;
                        maxval = img.at<float>(j2, i2);
                    }
            bool failed = false;
            const int i2hi = std::min(maxi + n, width - margin - 1);
            const int j2hi = std::min(maxj + n, height - margin - 1);
            for (int i2 = maxi - n; i2 <= i2hi && !failed; ++i2)
                for (int j2 = maxj - n; j2 <= j2hi; ++j2)
                    if (img.at<float>(j2, i2) > maxval &&
                        (i2 < i || i2 > i + n || j2 < j || j2 > j + n)) {
                        failed = true;
                        break;
                    }
            if (!failed && maxval >= tau) maxima.emplace_back(maxi, maxj);
        }
    return maxima;
}

// --- chessboardEnergy.m ---------------------------------------------------
struct Grid {
    int rows = 0, cols = 0;
    std::vector<int> a;
    int &at(int r, int c) { return a[static_cast<size_t>(r) * cols + c]; }
    int at(int r, int c) const { return a[static_cast<size_t>(r) * cols + c]; }
    bool empty() const { return a.empty(); }
};

double boardEnergy(const Grid &g, const std::vector<Corner> &c) {
    if (g.empty()) return 1e18;
    const double n = static_cast<double>(g.rows) * g.cols;
    double structure = 0;
    auto triple = [&](int i1, int i2, int i3) {
        const cv::Point2f &x1 = c[i1].p, &x2 = c[i2].p, &x3 = c[i3].p;
        const double den = cv::norm(x1 - x3);
        if (den < 1e-9) return 1e9;
        return cv::norm(x1 + x3 - 2 * x2) / den;
    };
    for (int r = 0; r < g.rows; ++r)
        for (int k = 0; k + 2 < g.cols; ++k)
            structure = std::max(structure, triple(g.at(r, k), g.at(r, k + 1), g.at(r, k + 2)));
    for (int col = 0; col < g.cols; ++col)
        for (int k = 0; k + 2 < g.rows; ++k)
            structure = std::max(structure, triple(g.at(k, col), g.at(k + 1, col), g.at(k + 2, col)));
    return -n + n * structure;
}

// --- initChessboard.m -----------------------------------------------------
// Nearest unused corner in direction v, cost = distance along v plus five times
// the distance off the line (so the search stays on the edge).
int directionalNeighbor(int idx, const cv::Point2f &v, const Grid &g,
                        const std::vector<Corner> &c, double &minDist,
                        const std::vector<char> &taken) {
    std::vector<char> used = taken;
    for (int i : g.a)
        if (i >= 0) used[i] = 1;

    minDist = std::numeric_limits<double>::infinity();
    int best = -1;
    for (size_t i = 0; i < c.size(); ++i) {
        if (used[i]) continue;
        const cv::Point2f d = c[i].p - c[idx].p;
        const double along = d.x * v.x + d.y * v.y;
        if (along < 0) continue;  // MATLAB sets these to inf
        const double off = std::hypot(d.x - along * v.x, d.y - along * v.y);
        const double cost = along + 5 * off;
        if (cost < minDist) {
            minDist = cost;
            best = static_cast<int>(i);
        }
    }
    return best;
}

Grid initBoard(const std::vector<Corner> &c, int seed, double homogeneity,
               const std::vector<char> &taken) {
    Grid g;
    if (c.size() < 9) return g;
    g.rows = g.cols = 3;
    g.a.assign(9, -1);

    const cv::Point2f v1 = c[seed].v1, v2 = c[seed].v2;
    g.at(1, 1) = seed;

    double d1[2], d2[6];
    g.at(1, 2) = directionalNeighbor(seed, v1, g, c, d1[0], taken);
    g.at(1, 0) = directionalNeighbor(seed, -v1, g, c, d1[1], taken);
    g.at(2, 1) = directionalNeighbor(seed, v2, g, c, d2[0], taken);
    g.at(0, 1) = directionalNeighbor(seed, -v2, g, c, d2[1], taken);
    if (g.at(1, 0) < 0 || g.at(1, 2) < 0 || g.at(2, 1) < 0 || g.at(0, 1) < 0) return Grid();
    g.at(0, 0) = directionalNeighbor(g.at(1, 0), -v2, g, c, d2[2], taken);
    g.at(2, 0) = directionalNeighbor(g.at(1, 0), v2, g, c, d2[3], taken);
    g.at(0, 2) = directionalNeighbor(g.at(1, 2), -v2, g, c, d2[4], taken);
    g.at(2, 2) = directionalNeighbor(g.at(1, 2), v2, g, c, d2[5], taken);

    for (int i : g.a)
        if (i < 0) return Grid();

    // The nine corners must be homogeneously spaced, or this is not a board.
    auto relStd = [](const double *v, int n) {
        double mean = 0;
        for (int i = 0; i < n; ++i) {
            if (!std::isfinite(v[i])) return 1e9;
            mean += v[i];
        }
        mean /= n;
        double var = 0;
        for (int i = 0; i < n; ++i) var += (v[i] - mean) * (v[i] - mean);
        var /= (n - 1);
        return std::abs(mean) < 1e-12 ? 1e9 : std::sqrt(var) / mean;
    };
    if (relStd(d1, 2) > homogeneity || relStd(d2, 6) > homogeneity) return Grid();
    return g;
}

// --- growChessboard.m -----------------------------------------------------
// "Replica prediction": extrapolate the next corner from the last three by
// continuing both the turn and the change of spacing. This is what lets a board
// be traced across strong (fisheye) distortion, where a linear extrapolation
// would overshoot.
std::vector<cv::Point2f> predictCorners(const std::vector<cv::Point2f> &p1,
                                        const std::vector<cv::Point2f> &p2,
                                        const std::vector<cv::Point2f> &p3) {
    std::vector<cv::Point2f> pred(p3.size());
    for (size_t i = 0; i < p3.size(); ++i) {
        const cv::Point2f v1 = p2[i] - p1[i], v2 = p3[i] - p2[i];
        const double a1 = std::atan2(v1.y, v1.x), a2 = std::atan2(v2.y, v2.x);
        const double a3 = 2 * a2 - a1;
        const double s1 = cv::norm(v1), s2 = cv::norm(v2);
        const double s3 = 2 * s2 - s1;
        // The 0.75 is the original's: under extreme distortion it biases the
        // prediction towards the nearer candidate.
        pred[i] = p3[i] + cv::Point2f(static_cast<float>(0.75 * s3 * std::cos(a3)),
                                      static_cast<float>(0.75 * s3 * std::sin(a3)));
    }
    return pred;
}

// Greedy global-minimum assignment of candidates to predictions.
std::vector<int> assignClosestCorners(const std::vector<cv::Point2f> &cand,
                                      const std::vector<cv::Point2f> &pred) {
    if (cand.size() < pred.size()) return {};
    const int nc = static_cast<int>(cand.size()), np = static_cast<int>(pred.size());
    std::vector<double> D(static_cast<size_t>(nc) * np);
    for (int j = 0; j < np; ++j)
        for (int i = 0; i < nc; ++i) D[static_cast<size_t>(i) * np + j] = cv::norm(cand[i] - pred[j]);

    std::vector<int> idx(np, -1);
    std::vector<char> rowUsed(nc, 0), colUsed(np, 0);
    for (int k = 0; k < np; ++k) {
        double best = std::numeric_limits<double>::infinity();
        int bi = -1, bj = -1;
        for (int i = 0; i < nc; ++i) {
            if (rowUsed[i]) continue;
            for (int j = 0; j < np; ++j) {
                if (colUsed[j]) continue;
                const double d = D[static_cast<size_t>(i) * np + j];
                if (d < best) {
                    best = d;
                    bi = i;
                    bj = j;
                }
            }
        }
        if (bi < 0) return {};
        idx[bj] = bi;
        rowUsed[bi] = 1;
        colUsed[bj] = 1;
    }
    return idx;
}

Grid growBoard(const Grid &g, const std::vector<Corner> &c, int borderType,
               const std::vector<char> &taken) {
    if (g.empty()) return g;
    std::vector<char> used = taken;
    for (int i : g.a)
        if (i >= 0) used[i] = 1;
    std::vector<int> unused;
    std::vector<cv::Point2f> cand;
    for (size_t i = 0; i < c.size(); ++i)
        if (!used[i]) {
            unused.push_back(static_cast<int>(i));
            cand.push_back(c[i].p);
        }

    auto col = [&](int j) {
        std::vector<cv::Point2f> v(g.rows);
        for (int r = 0; r < g.rows; ++r) v[r] = c[g.at(r, j)].p;
        return v;
    };
    auto row = [&](int i) {
        std::vector<cv::Point2f> v(g.cols);
        for (int cc = 0; cc < g.cols; ++cc) v[cc] = c[g.at(i, cc)].p;
        return v;
    };

    std::vector<cv::Point2f> pred;
    switch (borderType) {
        case 0: pred = predictCorners(col(g.cols - 3), col(g.cols - 2), col(g.cols - 1)); break;
        case 1: pred = predictCorners(row(g.rows - 3), row(g.rows - 2), row(g.rows - 1)); break;
        case 2: pred = predictCorners(col(2), col(1), col(0)); break;
        default: pred = predictCorners(row(2), row(1), row(0)); break;
    }
    const std::vector<int> pick = assignClosestCorners(cand, pred);
    if (pick.empty()) return g;

    Grid out;
    if (borderType == 0 || borderType == 2) {
        out.rows = g.rows;
        out.cols = g.cols + 1;
        out.a.assign(static_cast<size_t>(out.rows) * out.cols, -1);
        for (int r = 0; r < g.rows; ++r) {
            const int shift = (borderType == 2) ? 1 : 0;
            for (int cc = 0; cc < g.cols; ++cc) out.at(r, cc + shift) = g.at(r, cc);
            out.at(r, borderType == 0 ? g.cols : 0) = unused[pick[r]];
        }
    } else {
        out.rows = g.rows + 1;
        out.cols = g.cols;
        out.a.assign(static_cast<size_t>(out.rows) * out.cols, -1);
        const int shift = (borderType == 3) ? 1 : 0;
        for (int r = 0; r < g.rows; ++r)
            for (int cc = 0; cc < g.cols; ++cc) out.at(r + shift, cc) = g.at(r, cc);
        for (int cc = 0; cc < g.cols; ++cc)
            out.at(borderType == 1 ? g.rows : 0, cc) = unused[pick[cc]];
    }
    return out;
}

}  // namespace

std::vector<Corner> findCorners(const cv::Mat &image, const Params &params) {
    std::vector<Corner> corners;
    if (image.empty()) return corners;

    cv::Mat gray;
    if (image.channels() == 3)
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    else
        gray = image;
    cv::Mat img;
    gray.convertTo(img, CV_32F, 1.0 / 255.0);

    // Sobel-like derivatives. NOTE: MATLAB uses conv2 (which flips the kernel);
    // filter2D correlates, so the kernels here are the flipped ones. Only the
    // sign changes, and every use below is quadratic in the derivatives.
    const cv::Mat du = (cv::Mat_<float>(3, 3) << 1, 0, -1, 1, 0, -1, 1, 0, -1);
    const cv::Mat dv = (cv::Mat_<float>(3, 3) << 1, 1, 1, 0, 0, 0, -1, -1, -1);
    cv::Mat imgDu, imgDv;
    cv::filter2D(img, imgDu, CV_32F, du, cv::Point(-1, -1), 0, cv::BORDER_CONSTANT);
    cv::filter2D(img, imgDv, CV_32F, dv, cv::Point(-1, -1), 0, cv::BORDER_CONSTANT);

    cv::Mat imgAngle(img.size(), CV_32F), imgWeight(img.size(), CV_32F);
    for (int y = 0; y < img.rows; ++y) {
        const float *pu = imgDu.ptr<float>(y);
        const float *pv = imgDv.ptr<float>(y);
        float *pa = imgAngle.ptr<float>(y);
        float *pw = imgWeight.ptr<float>(y);
        for (int x = 0; x < img.cols; ++x) {
            double a = std::atan2(pv[x], pu[x]);
            if (a < 0) a += kPi;
            if (a > kPi) a -= kPi;
            pa[x] = static_cast<float>(a);
            pw[x] = static_cast<float>(std::hypot(pu[x], pv[x]));
        }
    }

    double lo = 0, hi = 0;
    cv::minMaxLoc(img, &lo, &hi);
    if (hi - lo < 1e-9) return corners;
    img = (img - lo) / (hi - lo);

    // Corner likelihood: the best checkerboard prototype response over two
    // orientations (axis-aligned and diagonal) at every radius.
    cv::Mat likelihood = cv::Mat::zeros(img.size(), CV_32F);
    for (int radius : params.radii) {
        if (radius < 2 || 2 * radius + 1 >= std::min(img.cols, img.rows)) continue;
        for (int orientation = 0; orientation < 2; ++orientation) {
            const double a1 = orientation == 0 ? 0.0 : kPi / 4;
            const double a2 = orientation == 0 ? kPi / 2 : -kPi / 4;
            const Patch t = createCorrelationPatch(a1, a2, radius);
            cv::Mat ra1, ra2, rb1, rb2;
            // NOTE: filter2D correlates where MATLAB convolves; a 180 deg
            // rotation swaps a1<->a2 and b1<->b2, and the combination below is
            // symmetric in those pairs, so the response is identical.
            cv::filter2D(img, ra1, CV_32F, t.a1, cv::Point(-1, -1), 0, cv::BORDER_CONSTANT);
            cv::filter2D(img, ra2, CV_32F, t.a2, cv::Point(-1, -1), 0, cv::BORDER_CONSTANT);
            cv::filter2D(img, rb1, CV_32F, t.b1, cv::Point(-1, -1), 0, cv::BORDER_CONSTANT);
            cv::filter2D(img, rb2, CV_32F, t.b2, cv::Point(-1, -1), 0, cv::BORDER_CONSTANT);
            const cv::Mat mu = (ra1 + ra2 + rb1 + rb2) / 4;
            // Case 1: the "a" quadrants are white; case 2: the "b" ones are.
            // A corner is only as good as its weakest quadrant, hence the mins.
            const cv::Mat ea1 = ra1 - mu, ea2 = ra2 - mu, eb1 = mu - rb1, eb2 = mu - rb2;
            cv::Mat ta, tb, c1, c2, best;
            cv::min(ea1, ea2, ta);
            cv::min(eb1, eb2, tb);
            cv::min(ta, tb, c1);
            const cv::Mat na1 = -ea1, na2 = -ea2, nb1 = -eb1, nb2 = -eb2;
            cv::min(na1, na2, ta);
            cv::min(nb1, nb2, tb);
            cv::min(ta, tb, c2);
            cv::max(c1, c2, best);
            cv::max(likelihood, best, likelihood);
        }
    }

    const std::vector<cv::Point> seeds =
        nonMaximumSuppression(likelihood, params.nmsWindow, params.tauNms, params.nmsMargin);

    const int W = img.cols, H = img.rows;
    const int r = 10;  // refinement window, as in the MATLAB
    for (const cv::Point &s : seeds) {
        Corner c;
        c.p = cv::Point2f(static_cast<float>(s.x), static_cast<float>(s.y));

        const int u0 = std::max(s.x - r, 0), u1 = std::min(s.x + r, W - 1);
        const int v0 = std::max(s.y - r, 0), v1 = std::min(s.y + r, H - 1);
        const cv::Rect win(u0, v0, u1 - u0 + 1, v1 - v0 + 1);
        if (!edgeOrientations(imgAngle(win), imgWeight(win), c.v1, c.v2)) continue;

        if (params.refine) {
            // Orientation refinement: least-variance direction of the gradients
            // that are (nearly) perpendicular to each current estimate.
            double A1[3] = {0, 0, 0}, A2[3] = {0, 0, 0};  // xx, xy, yy
            for (int v = v0; v <= v1; ++v)
                for (int u = u0; u <= u1; ++u) {
                    const double gx = imgDu.at<float>(v, u), gy = imgDv.at<float>(v, u);
                    const double n = std::hypot(gx, gy);
                    if (n < 0.1) continue;
                    const double ox = gx / n, oy = gy / n;
                    if (std::abs(ox * c.v1.x + oy * c.v1.y) < 0.25) {
                        A1[0] += gx * gx;
                        A1[1] += gx * gy;
                        A1[2] += gy * gy;
                    }
                    if (std::abs(ox * c.v2.x + oy * c.v2.y) < 0.25) {
                        A2[0] += gx * gx;
                        A2[1] += gx * gy;
                        A2[2] += gy * gy;
                    }
                }
            const cv::Point2f n1 = smallestEigenVector(A1[0], A1[1], A1[2]);
            const cv::Point2f n2 = smallestEigenVector(A2[0], A2[1], A2[2]);
            if (n1.x != 0 || n1.y != 0) c.v1 = n1;
            if (n2.x != 0 || n2.y != 0) c.v2 = n2;

            // Location refinement: intersect the two edge lines in the least
            // squares sense over the gradients that belong to them (Foerstner).
            double G[3] = {0, 0, 0};
            double b[2] = {0, 0};
            for (int v = v0; v <= v1; ++v)
                for (int u = u0; u <= u1; ++u) {
                    if (u == s.x && v == s.y) continue;
                    const double gx = imgDu.at<float>(v, u), gy = imgDv.at<float>(v, u);
                    const double n = std::hypot(gx, gy);
                    if (n < 0.1) continue;
                    const double ox = gx / n, oy = gy / n;
                    const double wx = u - s.x, wy = v - s.y;
                    const double t1 = wx * c.v1.x + wy * c.v1.y;
                    const double t2 = wx * c.v2.x + wy * c.v2.y;
                    const double d1 = std::hypot(wx - t1 * c.v1.x, wy - t1 * c.v1.y);
                    const double d2 = std::hypot(wx - t2 * c.v2.x, wy - t2 * c.v2.y);
                    const bool on1 = d1 < 3 && std::abs(ox * c.v1.x + oy * c.v1.y) < 0.25;
                    const bool on2 = d2 < 3 && std::abs(ox * c.v2.x + oy * c.v2.y) < 0.25;
                    if (!on1 && !on2) continue;
                    const double hxx = gx * gx, hxy = gx * gy, hyy = gy * gy;
                    G[0] += hxx;
                    G[1] += hxy;
                    G[2] += hyy;
                    b[0] += hxx * u + hxy * v;
                    b[1] += hxy * u + hyy * v;
                }
            const double det = G[0] * G[2] - G[1] * G[1];
            if (std::abs(det) < 1e-9) continue;  // rank deficient -> reject
            const double nx = (G[2] * b[0] - G[1] * b[1]) / det;
            const double ny = (G[0] * b[1] - G[1] * b[0]) / det;
            if (std::hypot(nx - s.x, ny - s.y) >= 4) continue;  // moved too far -> reject
            c.p = cv::Point2f(static_cast<float>(nx), static_cast<float>(ny));
        }

        // Score at every radius, keep the best.
        const int ur = static_cast<int>(std::lround(c.p.x)), vr = static_cast<int>(std::lround(c.p.y));
        double best = 0;
        for (int radius : params.radii) {
            if (ur < radius || ur >= W - radius || vr < radius || vr >= H - radius) continue;
            const cv::Rect roi(ur - radius, vr - radius, 2 * radius + 1, 2 * radius + 1);
            best = std::max(best, cornerCorrelationScore(img(roi), imgWeight(roi), c.v1, c.v2));
        }
        c.score = best;
        if (c.score < params.tauScore) continue;

        // Canonical frame: v1 pointing into the positive quadrant and (v1,v2)
        // right-handed, which cuts the matching ambiguity from 8 ways to 4.
        if (c.v1.x + c.v1.y < 0) c.v1 = -c.v1;
        const float flip = -((c.v1.y * c.v2.x - c.v1.x * c.v2.y) >= 0 ? 1.f : -1.f);
        c.v2 *= flip;
        corners.push_back(c);
    }

    // Keep the strongest corners when there are more than the structure stage
    // can chew through.
    if (static_cast<int>(corners.size()) > params.maxCorners) {
        std::nth_element(corners.begin(), corners.begin() + params.maxCorners, corners.end(),
                         [](const Corner &a, const Corner &b) { return a.score > b.score; });
        corners.resize(params.maxCorners);
    }
    return corners;
}

std::vector<Board> boardsFromCorners(const std::vector<Corner> &corners, const Params &params,
                                     Stats *stats) {
    Stats localStats;
    Stats &st = stats ? *stats : localStats;
    std::vector<Grid> boards;
    std::vector<double> energies;
    if (corners.size() < 9) return {};

    const auto started = std::chrono::steady_clock::now();
    auto outOfTime = [&] {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now() - started)
                   .count() > params.budgetMs;
    };

    // Corners already owned by an accepted board. The original algorithm has no
    // such thing: it runs one pass, and every board that shares a corner with a
    // better one is thrown away WHOLE -- its other corners with it.
    //
    // That is fatal on a calibration box. The pattern runs continuously across
    // the seams between panels, so growth from any seed wanders into the
    // neighbouring panel; every board then overlaps the one winning strip and is
    // discarded, and whole panels end up with no board at all even though every
    // one of their corners was detected. Which strip wins is arbitrary, so the
    // left and the right camera can disagree about which panels exist.
    //
    // So: run the original pass, freeze what it accepted, and run it again on
    // the corners left over. A second-pass seed can no longer grow through a
    // frozen panel, so it is forced to build its own board on the panel it
    // started from. Repeat until a pass adds nothing.
    std::vector<char> taken(corners.size(), 0);

    for (int pass = 0; pass < params.maxPasses && !outOfTime(); ++pass) {
        const size_t before = boards.size();

        for (size_t seed = 0; seed < corners.size(); ++seed) {
            if (outOfTime()) break;
            if (taken[seed]) continue;
            ++st.seeds;
            Grid g = initBoard(corners, static_cast<int>(seed), params.seedHomogeneity, taken);
            if (g.empty()) {
                ++st.seedNoInit;
                continue;
            }
            if (boardEnergy(g, corners) > 0) {
                ++st.seedBadEnergy;
                continue;
            }
            ++st.grown;

            for (;;) {
                const double energy = boardEnergy(g, corners);
                double bestE = energy;
                Grid best;
                for (int type = 0; type < 4; ++type) {
                    Grid p = growBoard(g, corners, type, taken);
                    if (p.empty() || (p.rows == g.rows && p.cols == g.cols)) continue;
                    const double e = boardEnergy(p, corners);
                    if (e < bestE) {
                        bestE = e;
                        best = p;
                    }
                }
                if (best.empty()) break;
                g = best;
            }

            const double e = boardEnergy(g, corners);
            if (e >= params.energyAccept) {
                ++st.rejectedEnergy;
                continue;
            }

            // Replace any overlapping board from THIS pass, but only if better.
            // Boards frozen by earlier passes cannot be overlapped at all.
            std::vector<int> overlapping;
            for (size_t j = 0; j < boards.size(); ++j) {
                bool shares = false;
                for (int i : boards[j].a)
                    if (std::find(g.a.begin(), g.a.end(), i) != g.a.end()) {
                        shares = true;
                        break;
                    }
                if (shares) overlapping.push_back(static_cast<int>(j));
            }
            if (overlapping.empty()) {
                boards.push_back(g);
                energies.push_back(e);
                continue;
            }
            bool better = true;
            for (int j : overlapping)
                if (energies[j] <= e) better = false;
            if (!better) {
                ++st.rejectedOverlap;
                continue;
            }
            for (auto it = overlapping.rbegin(); it != overlapping.rend(); ++it) {
                boards.erase(boards.begin() + *it);
                energies.erase(energies.begin() + *it);
            }
            boards.push_back(g);
            energies.push_back(e);
        }

        if (boards.size() == before) break;  // nothing new: done
        std::fill(taken.begin(), taken.end(), 0);
        for (const Grid &b : boards)
            for (int i : b.a)
                if (i >= 0) taken[i] = 1;
        ++st.passes;
    }

    std::vector<Board> out;
    out.reserve(boards.size());
    for (size_t i = 0; i < boards.size(); ++i)
        out.push_back({boards[i].rows, boards[i].cols, boards[i].a, energies[i]});
    std::sort(out.begin(), out.end(),
              [](const Board &a, const Board &b) { return a.energy < b.energy; });
    return out;
}

}  // namespace CbDetect
