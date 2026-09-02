#include "moilcali_algorithm.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numeric>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace MoilCali {

// Decode once, then the SAME preparation blur_gray does -- called, not repeated.
// The header promises "same preparation as load_blurred_gray", and with raw nodes
// on that promise was false: this one blurred unconditionally while the Mat
// version skipped the blur, so the same image fed as a path and as bytes gave
// node positions a fraction of a pixel apart.
cv::Mat load_blurred_gray(const std::string &imgPath) {
    return blur_gray(cv::imread(imgPath));
}

std::vector<int> get_pattern_histogram_gray(const std::string &imgPath,
                                         cv::Point center,
                                         const std::string &direction) {
    return get_pattern_histogram_gray(load_blurred_gray(imgPath), center, direction);
}

std::vector<int> get_pattern_histogram_gray(const cv::Mat &gray, cv::Point center,
                                            const std::string &direction) {
    if (gray.empty()) return {};
    const int h = gray.rows, w = gray.cols;
    const int x = center.x, y = center.y;
    const auto at = [&](int r, int c) { return static_cast<int>(gray.at<uchar>(r, c)); };

    std::vector<int> data;
    const int diag = static_cast<int>(y / std::sqrt(2.0));
    if (direction == "n") {
        for (int i = y; i >= 0; --i) data.push_back(at(i, x));
    } else if (direction == "s") {
        for (int i = y; i < h; ++i) data.push_back(at(i, x));
    } else if (direction == "w") {
        for (int i = x; i >= 0; --i) data.push_back(at(y, i));
    } else if (direction == "e") {
        for (int i = x; i < w; ++i) data.push_back(at(y, i));
    } else if (direction == "nw") {
        for (int i = 0; i < diag; ++i) data.push_back(at(y - i, x - i));
    } else if (direction == "ne") {
        for (int i = 0; i < diag; ++i) data.push_back(at(y - i, x + i));
    } else if (direction == "sw") {
        for (int i = 0; i < diag; ++i) data.push_back(at(y + i, x - i));
    } else if (direction == "se") {
        for (int i = 0; i < diag; ++i) data.push_back(at(y + i, x + i));
    }
    return data;
}

std::vector<int> get_pattern_histogram_color(const std::string &, cv::Point,
                                             const std::string &, const std::string &) {
    // Python original is a `pass` stub.
    return {};
}

cv::Mat draw_edge_circle_on_imgpath(const std::string &path, int cpx, int cpy,
                                int radius, cv::Scalar color, int thickness) {
    cv::Mat image = cv::imread(path);
    if (image.empty()) return image;
    const int h = image.rows, w = image.cols;
    cv::circle(image, {cpx, cpy}, radius, color, thickness);
    cv::line(image, {0, cpy}, {w, cpy}, color, thickness);
    cv::line(image, {cpx, 0}, {cpx, h}, color, thickness);
    return image;
}

cv::Mat &draw_center_roi_on_cv2obj(cv::Mat &img, int cpx, int cpy, int radius) {
    const cv::Point lu(cpx - radius, cpy - radius);
    const cv::Point ld(cpx - radius, cpy + radius);
    const cv::Point ru(cpx + radius, cpy - radius);
    const cv::Point rd(cpx + radius, cpy + radius);
    const cv::Scalar color(0, 0, 255);
    const int thickness = std::lround(img.cols / 640.0);
    cv::rectangle(img, lu, rd, color, thickness);
    cv::line(img, lu, rd, color, thickness);
    cv::line(img, ld, ru, color, thickness);
    cv::circle(img, {cpx, cpy}, radius, color, thickness);
    return img;
}

namespace {
// Global toggle for node-noise cleaning + bezel-aware table layout. Default ON
// (this is the intended workflow); the UI checkboxes can turn it off for raw data.
// OFF, as the header documents. It shipped as `true`, so every table silently
// lost nodes the spacing filter judged too closely spaced -- including real
// rings. Dropping a node is not free: it leaves a hole in one direction's column
// while the opposite directions keep theirs, and nothing in the UI says why.
// Cleaning stays available behind the Clean Noise button; it is just no longer
// the default.
bool g_noiseCleaning = false;
// No preprocessing at all: no blur, no near-black crossing guard. Off unless a
// caller asks, so nothing that exists today changes. See set_raw_nodes.
bool g_rawNodes = false;

// Black-area detection (the user's histogram criterion). A real ring shows the
// two curves as "atas-bawah" — one high, one low — so at least one reaches the
// bright level. Noise is where BOTH curves stay in the black zone (below ~100).
// The dark run is then grown FORWARD (to the right, away from centre) through the
// bezel's recovery ramp until a full-bright ring (>= kBrightConfirm) is reached,
// so transition crossings — even where one curve bumps a little above 100 — go
// too. Only the fully-recovered rings after the ramp are kept.
constexpr int kBlackLevel = 100;     // no curve reaches this nearby -> black core
constexpr int kBrightConfirm = 180;  // a genuine ring peaks at/above this
constexpr int kBlackWindow = 5;      // samples each side for the local peak
constexpr int kNodeGrayMin = 60;     // grayscale AT the crossing below this -> noise

double median_of(std::vector<double> v) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    const size_t m = v.size() / 2;
    return (v.size() % 2) ? v[m] : 0.5 * (v[m - 1] + v[m]);
}

// Per-sample local peak of the brighter curve within +-kBlackWindow.
std::vector<int> compute_local_peak(const std::vector<int> &pos, const std::vector<int> &neg,
                                    int n) {
    std::vector<int> smax(n < 0 ? 0 : n, 0);
    for (int i = 0; i < n; ++i) {
        const int lo = std::max(0, i - kBlackWindow);
        const int hi = std::min(n - 1, i + kBlackWindow);
        int mx = 0;
        for (int k = lo; k <= hi; ++k) mx = std::max(mx, std::max(pos[k], neg[k]));
        smax[i] = mx;
    }
    return smax;
}

constexpr double kPairTol = 5.0;  // paired directions must agree within this (px)

// Paired opposite directions (N&S, W&E) see the same rings, so their node lists
// should match value-for-value within kPairTol. Drop a node ONLY when skipping it
// realigns the two sequences (a genuine insertion / residual straggler); a real
// ring the partner merely missed, and any trailing tail, is kept.
void reconcile_pair(std::vector<double> &A, std::vector<double> &B, double tol) {
    std::vector<double> oa, ob;
    size_t i = 0, j = 0;
    while (i < A.size() && j < B.size()) {
        const double d = A[i] - B[j];
        if (std::abs(d) <= tol) {
            oa.push_back(A[i++]);
            ob.push_back(B[j++]);
        } else if (d < 0) {  // A[i] is the smaller, unmatched value
            if (i + 1 < A.size() && std::abs(A[i + 1] - B[j]) <= tol) ++i;  // drop A[i] insertion
            else oa.push_back(A[i++]);                                       // keep (B missed it)
        } else {  // B[j] is the smaller, unmatched value
            if (j + 1 < B.size() && std::abs(A[i] - B[j + 1]) <= tol) ++j;  // drop B[j] insertion
            else ob.push_back(B[j++]);
        }
    }
    while (i < A.size()) oa.push_back(A[i++]);  // trailing kept
    while (j < B.size()) ob.push_back(B[j++]);
    A.swap(oa);
    B.swap(ob);
}

// Force every list in a group to the SMALLEST node count: from each longer list
// drop the least-corroborated nodes (fewest matches within tol among the others)
// first, so residual bezel stragglers go before consensus rings. Works for a pair
// (N&S, W&E) or the four diagonals alike.
void equalize_counts(const std::vector<std::vector<double> *> &lists, double tol) {
    if (lists.empty()) return;
    size_t target = lists[0]->size();
    for (auto *L : lists) target = std::min(target, L->size());
    const int cap = static_cast<int>(lists.size()) + 1;
    for (auto *L : lists) {
        while (L->size() > target) {
            int worst = -1, worstSupport = cap;
            for (size_t k = 0; k < L->size(); ++k) {
                int support = 0;
                for (auto *O : lists) {
                    if (O == L) continue;
                    for (double v : *O)
                        if (std::abs(v - (*L)[k]) <= tol) { ++support; break; }
                }
                if (support < worstSupport) { worstSupport = support; worst = static_cast<int>(k); }
            }
            if (worst < 0) break;
            L->erase(L->begin() + worst);
        }
    }
}
}  // namespace

void set_noise_cleaning(bool enabled) { g_noiseCleaning = enabled; }
bool noise_cleaning_enabled() { return g_noiseCleaning; }

void set_raw_nodes(bool enabled) { g_rawNodes = enabled; }
bool raw_nodes_enabled() { return g_rawNodes; }

// Remove dense clusters of false nodes left by the monitor's dark panel gaps,
// while preserving the genuine fisheye spacing compression toward the edge
// (where real spacing legitimately shrinks to ~15 px).
//
// A gap between two consecutive nodes is "noise" when it falls well below the
// LOCAL spacing trend (a windowed median — robust both to the noise itself and
// to the smooth center->edge variation; a global median would wrongly flag the
// compressed edge). BOTH endpoints of a noise gap are dropped, so a cluster like
// 711 / 737 / 761 sitting inside a monitor gap is removed whole, including the
// border node whose *incoming* gap still looked normal (that is why the earlier
// greedy variant kept 711). Ported from the user's diff+median mask idea.
std::vector<double> clean_intersecting_nodes(const std::vector<double> &nodes, double factor) {
    const int n = static_cast<int>(nodes.size());
    if (n < 5) return nodes;  // too few to establish a local trend

    std::vector<double> gaps(n - 1);
    for (int i = 0; i < n - 1; ++i) gaps[i] = nodes[i + 1] - nodes[i];

    constexpr int W = 4;  // half-window over the gap sequence for the local median
    std::vector<bool> bad(n, false);
    for (int i = 0; i < n - 1; ++i) {
        const int lo = std::max(0, i - W);
        const int hi = std::min(static_cast<int>(gaps.size()) - 1, i + W);
        const double med = median_of({gaps.begin() + lo, gaps.begin() + hi + 1});
        if (med > 0.0 && gaps[i] < factor * med) {
            bad[i] = true;
            bad[i + 1] = true;  // expand to both endpoints of the noisy interval
        }
    }

    std::vector<double> out;
    out.reserve(n);
    for (int i = 0; i < n; ++i)
        if (!bad[i]) out.push_back(nodes[i]);
    return out;
}

std::vector<double> get_list_intersecting_nodes(const std::vector<int> &pos,
                                             const std::vector<int> &neg) {
    const int length = static_cast<int>(std::min(pos.size(), neg.size()));
    const bool clean = g_noiseCleaning;

    // Sample-level black mask (only when cleaning): black core = neither curve
    // reaches kBlackLevel nearby; then grow each run FORWARD through the recovery
    // ramp until a full-bright ring (>= kBrightConfirm) is reached.
    std::vector<char> black;
    if (clean) {
        const std::vector<int> smax = compute_local_peak(pos, neg, length);
        black.assign(length, 0);
        for (int i = 0; i < length; ++i) black[i] = (smax[i] < kBlackLevel);
        bool inBlack = false;
        for (int i = 0; i < length; ++i) {
            if (black[i]) {
                inBlack = true;
            } else if (inBlack) {
                if (smax[i] < kBrightConfirm) black[i] = 1;  // recovery ramp -> black
                else inBlack = false;                         // full ring -> stop growing
            }
        }
    }

    std::vector<double> nodes;
    for (int i = 0; i < length - 1; ++i) {
        const bool cross =
            (pos[i] > neg[i] && pos[i + 1] < neg[i + 1]) ||
            (pos[i] < neg[i] && pos[i + 1] > neg[i + 1]) ||
            (pos[i] == neg[i] && pos[i + 1] > neg[i + 1]) ||
            (pos[i] == neg[i] && pos[i + 1] < neg[i + 1]);
        // pos[i] > 5 drops a crossing whose positive sample is nearly black. It
        // is a filter, so raw mode does not apply it -- with the consequence
        // documented at set_raw_nodes: outside the fisheye circle both curves sit
        // at ~0 and the equality cases above turn sensor noise into crossings.
        if (cross && (g_rawNodes || pos[i] > 5)) {
            if (clean && black[i]) continue;  // black core or recovery ramp
            const int posM = -(pos[i] - pos[i + 1]);
            const int negM = -(neg[i] - neg[i + 1]);
            const int posB = pos[i] - posM * i;
            const int negB = neg[i] - negM * i;
            if (posM - negM != 0) {
                const double x = static_cast<double>(negB - posB) / (posM - negM);
                if (clean) {
                    // Grayscale AT the crossing: below kNodeGrayMin it sits in the
                    // dark zone -> noise (real rings, even dim ones, stay >= ~87).
                    const int xi = static_cast<int>(std::round(x));
                    const int gray = (xi >= 0 && xi < length) ? std::max(pos[xi], neg[xi]) : 0;
                    if (gray < kNodeGrayMin) continue;
                }
                nodes.push_back(std::round(x * 100.0) / 100.0);
            }
        }
    }
    return nodes;
}

std::pair<cv::Point, int> detect_fisheye_edge(const cv::Mat &image, int threshold) {
    cv::Mat gray, th;
    cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    cv::threshold(gray, th, threshold, 255, cv::THRESH_BINARY);

    std::vector<cv::Point> left, right;
    bool leftFlag = false, rightFlag = false;
    for (int i = 0; i < th.cols; ++i) {
        if (!leftFlag) {
            for (int j = 0; j < th.rows; ++j)
                if (th.at<uchar>(j, i) == 255) { left.emplace_back(i, j); leftFlag = true; break; }
        }
        if (!rightFlag) {
            const int c = th.cols - i - 1;
            for (int j = 0; j < th.rows; ++j)
                if (th.at<uchar>(j, c) == 255) { right.emplace_back(c, j); rightFlag = true; break; }
        }
        if (leftFlag && rightFlag) break;
    }

    const auto avg = [](const std::vector<cv::Point> &pts) {
        long sx = 0, sy = 0;
        for (const auto &p : pts) { sx += p.x; sy += p.y; }
        const int n = static_cast<int>(pts.size());
        return cv::Point(n ? static_cast<int>(sx / n) : 0, n ? static_cast<int>(sy / n) : 0);
    };
    const cv::Point lp = avg(left), rp = avg(right);
    const cv::Point center = avg({lp, rp});
    const int r = std::abs((rp.x - lp.x) / 2);
    return {center, r};
}

cv::Point detect_roi(const cv::Mat &image, int orgX, int orgY, int thr) {
    cv::Mat gray, bin;
    cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    cv::threshold(gray, bin, thr, 255, cv::THRESH_BINARY);

    const auto edge = [&](int cx, int cy) -> cv::Point {
        const uchar centerVal = bin.at<uchar>(orgY, orgX);
        int x = orgX, y = orgY;
        for (int i = 0; i < bin.rows; ++i) {
            const int ny = orgY + cx * i, nx = orgX + cy * i;
            if (ny >= bin.rows || nx >= bin.cols || ny < 0 || nx < 0) return {x, y};
            if (bin.at<uchar>(ny, nx) != centerVal) return {nx, ny};
        }
        return {x, y};
    };
    const cv::Point w = edge(-1, 0), e = edge(1, 0);
    const int centerY = (e.y + w.y) / 2;
    const cv::Point s = edge(0, 1), n = edge(0, -1);
    const int centerX = (s.x + n.x) / 2;
    return {centerX, centerY};
}

// ---- Centre from the concentric pattern's ring structure -------------------

namespace {

constexpr double kRingMinR = 8.0;     // skip the innermost blur blob
constexpr double kRingMaxR = 900.0;   // generous; extra edges past the pattern are ignored
constexpr double kRingStep = 0.5;     // radial sampling step, px
constexpr double kRingMinAmp = 40.0;  // local peak-to-peak below this is not a ring edge
constexpr double kRingMinGap = 6.0;   // two crossings closer than this are one edge

inline float sampleBilinear(const cv::Mat &g, double x, double y) {
    const int x0 = static_cast<int>(x), y0 = static_cast<int>(y);
    if (x0 < 0 || y0 < 0 || x0 + 1 >= g.cols || y0 + 1 >= g.rows) return -1.f;
    const double fx = x - x0, fy = y - y0;
    const float *r0 = g.ptr<float>(y0), *r1 = g.ptr<float>(y0 + 1);
    return static_cast<float>((r0[x0] * (1 - fx) + r0[x0 + 1] * fx) * (1 - fy) +
                              (r1[x0] * (1 - fx) + r1[x0 + 1] * fx) * fy);
}

// Ring crossings along one ray. The threshold is the LOCAL mid-level between the
// running min and max, so vignetting (which a fixed threshold trips over) drops
// out; the amplitude gate then rejects flat stretches such as the monitor bezel.
void ringCrossings(const cv::Mat &g, double cx, double cy, double ang,
                   std::vector<double> &out) {
    out.clear();
    const double dx = std::cos(ang), dy = std::sin(ang);
    const int n = static_cast<int>((kRingMaxR - kRingMinR) / kRingStep);
    if (n < 80) return;

    // Scratch reused across calls: this runs ~10^5 times per search, so a fresh
    // allocation per ray dominated the cost.
    static thread_local std::vector<float> prof, lo, hi;
    static thread_local std::vector<int> dqMin, dqMax;
    prof.assign(n, -1.f);

    // A ray leaves the centre and never comes back, so validity is a prefix.
    int last = -1;
    for (int i = 0; i < n; ++i) {
        const double r = kRingMinR + i * kRingStep;
        const float v = sampleBilinear(g, cx + dx * r, cy + dy * r);
        if (v < 0) break;
        prof[i] = v;
        last = i;
    }
    if (last < 60) return;

    // Local min/max over a centred window, via monotonic deques: O(n) instead of
    // O(n*window). The window is ~one ring period, so the mid-level between them
    // tracks vignetting without smearing neighbouring rings together.
    const int win = 61, h = win / 2;
    lo.resize(last + 1);
    hi.resize(last + 1);
    dqMin.clear();
    dqMax.clear();
    int head_mn = 0, head_mx = 0, j = 0;
    for (int i = 0; i <= last; ++i) {
        const int jEnd = std::min(last, i + h);
        for (; j <= jEnd; ++j) {
            while (dqMin.size() > static_cast<size_t>(head_mn) && prof[dqMin.back()] >= prof[j])
                dqMin.pop_back();
            dqMin.push_back(j);
            while (dqMax.size() > static_cast<size_t>(head_mx) && prof[dqMax.back()] <= prof[j])
                dqMax.pop_back();
            dqMax.push_back(j);
        }
        const int iLo = i - h;
        while (dqMin[head_mn] < iLo) ++head_mn;
        while (dqMax[head_mx] < iLo) ++head_mx;
        lo[i] = prof[dqMin[head_mn]];
        hi[i] = prof[dqMax[head_mx]];
    }
    for (int i = 1; i <= last; ++i) {
        if (prof[i] < 0 || prof[i - 1] < 0) continue;
        if (hi[i] - lo[i] < kRingMinAmp) continue;
        const float mid = 0.5f * (lo[i] + hi[i]);
        const float a = prof[i - 1] - mid, b = prof[i] - mid;
        if ((a < 0 && b >= 0) || (a > 0 && b <= 0)) {
            const double t = (a == b) ? 0.0 : static_cast<double>(a) / (a - b);
            const double r = kRingMinR + (i - 1 + t) * kRingStep;
            if (out.empty() || r - out.back() > kRingMinGap) out.push_back(r);
        }
    }
}

struct RayField {
    std::vector<std::vector<double>> perRay;
    int good = 0;
};

RayField castRays(const cv::Mat &g, double cx, double cy, int nRays, int K) {
    RayField f;
    f.perRay.resize(nRays);
    std::vector<double> tmp;
    for (int i = 0; i < nRays; ++i) {
        ringCrossings(g, cx, cy, 2.0 * CV_PI * i / nRays, tmp);
        f.perRay[i] = tmp;
        if (static_cast<int>(tmp.size()) >= K) ++f.good;
    }
    return f;
}

// Least-squares fit of ring k's radius profile to
//
//     r(theta) = a0 + a1 cos t + b1 sin t + a2 cos 2t + b2 sin 2t
//
// and append the four coefficients to `detail`. See the long note on RingCenter
// for what they are for; in short, the angular STD says how much the radius
// varies and these say in what SHAPE, which is what separates a camera that is
// off-centre from one that is tilted.
//
// A real least-squares solve rather than the textbook projection
// a_n = (2/N) sum r cos(n t). That shortcut is only valid when the samples are
// spread evenly round the circle, and here they are not: rays that cross a
// monitor bezel come back with the wrong crossing count and are dropped, which
// removes a contiguous ARC. On the bezel fixture that is 199 rays of 720 in one
// sector -- feed that to the projection and the missing arc alone manufactures a
// harmonic. The normal equations handle the gap correctly, and SVD keeps the
// answer finite if a ring is so sparsely sampled that the basis goes rank
// deficient.
//
// Only ever called with detail != nullptr, i.e. once per ring at a scored point,
// never inside the search.
void fitRingHarmonics(const RayField &f, int k, int K, int nRays, RingCenter *detail) {
    double A[5][5] = {}, rhs[5] = {};
    for (int i = 0; i < nRays; ++i) {
        const std::vector<double> &ray = f.perRay[i];
        if (static_cast<int>(ray.size()) < K) continue;  // dropped ray: no ring k on it
        const double t = 2.0 * CV_PI * i / nRays;
        const double basis[5] = {1.0, std::cos(t), std::sin(t), std::cos(2 * t), std::sin(2 * t)};
        const double r = ray[k];
        for (int a = 0; a < 5; ++a) {
            rhs[a] += basis[a] * r;
            for (int b = 0; b < 5; ++b) A[a][b] += basis[a] * basis[b];
        }
    }

    cv::Mat Am(5, 5, CV_64F, &A[0][0]), bm(5, 1, CV_64F, rhs), x;
    if (!cv::solve(Am, bm, x, cv::DECOMP_SVD)) {
        detail->ringA1.push_back(0.0);
        detail->ringB1.push_back(0.0);
        detail->ringA2.push_back(0.0);
        detail->ringB2.push_back(0.0);
        return;
    }
    detail->ringA1.push_back(x.at<double>(1));
    detail->ringB1.push_back(x.at<double>(2));
    detail->ringA2.push_back(x.at<double>(3));
    detail->ringB2.push_back(x.at<double>(4));
}

// Mean over rings of (angular std / mean radius). Lower is rounder.
double ringCost(const cv::Mat &g, double cx, double cy, int nRays, int K,
                RingCenter *detail = nullptr) {
    const RayField f = castRays(g, cx, cy, nRays, K);
    if (f.good < nRays / 3) return 1e9;

    double total = 0;
    int used = 0;
    std::vector<double> rk;
    for (int k = 0; k < K; ++k) {
        rk.clear();
        for (const auto &ray : f.perRay)
            if (static_cast<int>(ray.size()) >= K) rk.push_back(ray[k]);
        if (static_cast<int>(rk.size()) < nRays / 3) continue;
        const double m = std::accumulate(rk.begin(), rk.end(), 0.0) / rk.size();
        double v = 0;
        for (double r : rk) v += (r - m) * (r - m);
        v = std::sqrt(v / rk.size());
        if (m > 1) {
            total += v / m;
            ++used;
            if (detail) {
                detail->ringRadius.push_back(m);
                detail->ringSpread.push_back(v);
                fitRingHarmonics(f, k, K, nRays, detail);
            }
        }
    }
    if (!used) return 1e9;
    if (detail) {
        detail->ringsUsed = used;
        detail->raysUsed = f.good;
        detail->raysTotal = nRays;
    }
    return total / used;
}

// Modal crossing count over a ring of probe angles: the fallback when the caller
// has no ring count from the generator.
int modalRingCount(const cv::Mat &g, double cx, double cy) {
    std::vector<int> hist(128, 0);
    std::vector<double> tmp;
    for (int i = 0; i < 72; ++i) {
        ringCrossings(g, cx, cy, 2.0 * CV_PI * i / 72, tmp);
        if (tmp.size() < hist.size()) hist[tmp.size()]++;
    }
    return static_cast<int>(std::max_element(hist.begin(), hist.end()) - hist.begin());
}

}  // namespace

std::vector<double> measure_pattern_ring_radii(const cv::Mat &patternGray) {
    std::vector<double> out;
    if (patternGray.empty()) return out;
    const int cx = patternGray.cols / 2, cy = patternGray.rows / 2;
    int prev = patternGray.at<uchar>(cy, cx);
    for (int x = cx + 1; x < patternGray.cols; ++x) {
        const int v = patternGray.at<uchar>(cy, x);
        if (std::abs(v - prev) > 60) out.push_back(x - cx);
        prev = v;
    }
    return out;
}

RingCenter find_center_from_rings(const cv::Mat &gray, int expectedRings, cv::Point2d seed,
                                  double searchRadius) {
    RingCenter r;
    if (gray.empty()) return r;
    cv::Mat g;
    gray.convertTo(g, CV_32F);

    int K = expectedRings;
    if (K <= 0) K = modalRingCount(g, seed.x, seed.y);
    if (K < 3) return r;

    // Coarse grid first. The basin is only ~15 px wide and is ringed by shallower
    // local minima, so a descent straight from the seed lands in the wrong one
    // unless the seed is already nearly right. 10 px steps still sample the basin.
    const int steps = std::max(1, static_cast<int>(searchRadius / 10.0));
    const int side = 2 * steps + 1;
    std::vector<double> grid(static_cast<size_t>(side) * side, 1e9);
    cv::parallel_for_(cv::Range(0, side * side), [&](const cv::Range &rg) {
        for (int i = rg.start; i < rg.end; ++i)
            grid[i] = ringCost(g, seed.x + (i % side - steps) * 10.0,
                               seed.y + (i / side - steps) * 10.0, 120, K);
    });
    const int bestIdx = static_cast<int>(std::min_element(grid.begin(), grid.end()) - grid.begin());
    double best = grid[bestIdx];
    if (best >= 1e8) return r;
    double bx = seed.x + (bestIdx % side - steps) * 10.0;
    double by = seed.y + (bestIdx / side - steps) * 10.0;

    // Then tighten: a 4 px sweep around the coarse winner, then a local descent.
    const double gx = bx, gy = by;
    std::vector<double> fine(36, 1e9);
    cv::parallel_for_(cv::Range(0, 36), [&](const cv::Range &rg) {
        for (int i = rg.start; i < rg.end; ++i)
            fine[i] = ringCost(g, gx + (i % 6 - 3) * 4.0, gy + (i / 6 - 3) * 4.0, 180, K);
    });
    const int fi = static_cast<int>(std::min_element(fine.begin(), fine.end()) - fine.begin());
    if (fine[fi] < best) { best = fine[fi]; bx = gx + (fi % 6 - 3) * 4.0; by = gy + (fi / 6 - 3) * 4.0; }
    for (double step : {2.0, 1.0, 0.5, 0.25}) {
        bool moved = true;
        int guard = 0;
        while (moved && guard++ < 60) {
            moved = false;
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx) {
                    if (!dx && !dy) continue;
                    const double c = ringCost(g, bx + dx * step, by + dy * step, 360, K);
                    if (c < best) { best = c; bx += dx * step; by += dy * step; moved = true; }
                }
        }
    }

    r.center = {bx, by};
    r.cost = ringCost(g, bx, by, 720, K, &r);
    return r;
}

RingCenter evaluate_ring_center(const cv::Mat &gray, int expectedRings, cv::Point2d at,
                                int nRays) {
    RingCenter r;
    if (gray.empty()) return r;
    cv::Mat g;
    gray.convertTo(g, CV_32F);

    // The same preparation find_center_from_rings does, deliberately duplicated
    // rather than shared: the two must agree about K on the same image, or a
    // centre this function passes could be one the search would have rejected --
    // and the disagreement would show up as an intermittent escalation nobody
    // could reproduce.
    int K = expectedRings;
    if (K <= 0) K = modalRingCount(g, at.x, at.y);
    if (K < 3) return r;

    // ringCost fills the detail block only once it has rings to report; on either
    // of its 1e9 exits ringsUsed stays 0 while ringRadius/ringSpread may already
    // hold a partial push. Returning a fresh RingCenter rather than the partly
    // filled one keeps "no usable rings here" from arriving as a fit with two
    // rings and a nonsense cost.
    const double c = ringCost(g, at.x, at.y, nRays, K, &r);
    if (c >= 1e8) return RingCenter{};

    r.center = at;
    r.cost = c;
    return r;
}

double ring_center_offset_px(const RingCenter &r) {
    if (r.ringSpread.empty()) return -1.0;

    std::vector<double> s = r.ringSpread;
    const std::size_t mid = s.size() / 2;
    std::nth_element(s.begin(), s.begin() + mid, s.end());
    double med = s[mid];
    if (s.size() % 2 == 0) {
        // nth_element leaves everything below `mid` to its left but unordered, so
        // the lower of the two middle values is the max of that left part.
        med = 0.5 * (med + *std::max_element(s.begin(), s.begin() + mid));
    }
    return std::sqrt(2.0) * med;
}

cv::Mat blur_gray(const cv::Mat &image) {
    if (image.empty()) return {};
    cv::Mat gray;
    if (image.channels() == 1) gray = image.clone();
    else cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    // The grayscale conversion is not optional -- an intensity has to be sampled
    // from something. The BLUR is, and in raw mode it does not happen: a 3x3 box
    // moves a crossing by a fraction of a pixel and can merge two rings that are
    // close together near the edge.
    if (!g_rawNodes) cv::blur(gray, gray, cv::Size(3, 3));
    return gray;
}

std::map<std::string, std::vector<double>>
get_dict_8direction_intersecting_nodes_by_pos_neg_img_path(const std::string &posPath, const std::string &negPath,
                                   cv::Point posCenter, cv::Point negCenter) {
    // Decode each image ONCE (was 8x per image before), then compute all eight
    // directions in parallel across every CPU core via cv::parallel_for_.
    return get_dict_8direction_intersecting_nodes(load_blurred_gray(posPath),
                                                  load_blurred_gray(negPath), posCenter, negCenter);
}

std::map<std::string, std::vector<double>>
get_dict_8direction_intersecting_nodes(const cv::Mat &posGray, const cv::Mat &negGray,
                                       cv::Point posCenter, cv::Point negCenter) {
    static const std::array<std::string, 8> dirs = {"n", "s", "w", "e", "nw", "se", "sw", "ne"};
    std::array<std::vector<double>, 8> out;

    cv::parallel_for_(cv::Range(0, 8), [&](const cv::Range &range) {
        for (int i = range.start; i < range.end; ++i) {
            const std::string &d = dirs[i];
            const auto pos = get_pattern_histogram_gray(posGray, posCenter, d);
            const auto neg = get_pattern_histogram_gray(negGray, negCenter, d);
            auto nodes = get_list_intersecting_nodes(pos, neg);
            if (d == "nw" || d == "se" || d == "sw" || d == "ne")
                for (double &v : nodes) v = std::round(v * std::sqrt(2.0) * 100.0) / 100.0;
            out[i] = std::move(nodes);
        }
    });

    // Cross-direction consistency (only when cleaning is on): opposite pairs are
    // aligned then trimmed to a shared count; the four diagonals share one count.
    if (g_noiseCleaning) {
        reconcile_pair(out[0], out[1], kPairTol);  // N & S: align, drop insertions
        reconcile_pair(out[2], out[3], kPairTol);  // W & E
        equalize_counts({&out[0], &out[1]}, kPairTol);              // N & S -> equal count
        equalize_counts({&out[2], &out[3]}, kPairTol);              // W & E -> equal count
        equalize_counts({&out[4], &out[5], &out[6], &out[7]}, kPairTol);  // diagonals
    }

    std::map<std::string, std::vector<double>> result;
    for (int i = 0; i < 8; ++i) result[dirs[i]] = std::move(out[i]);
    return result;
}

}  // namespace MoilCali
