#include "ComputeOps.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonObject>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "ComputeCleaning.h"
#include "ComputeJson.h"
#include "moilcali_algorithm.h"

namespace ComputeOps {
namespace {

using namespace ComputeOps::json;

const std::array<const char *, 8> kDirs8 = {"n", "s", "w", "e", "nw", "se", "sw", "ne"};

QJsonObject encodeNodeDict(const std::map<std::string, std::vector<double>> &dict) {
    QJsonObject o;
    for (const auto &kv : dict)
        o[QString::fromStdString(kv.first)] = fromDoubleVec(kv.second);
    return o;
}

// ---- the gradient-line centre fit, shared by pattern_center and auto_center --
//
// Least-squares intersection of the ring edges' gradient lines.
//
// Every ring boundary of a concentric pattern is a circle about the common
// centre, so the line through each edge pixel along its gradient passes through
// that centre. Fitting the point closest to all of them gives the centre
// directly: no seed, no ring matching, no iteration to converge, and it uses
// every edge pixel, so it is robust. Two refinement passes then restrict the fit
// to the concentric core around the first estimate, which drops off-centre outer
// edges.
//
// Answers cx = -1 when it cannot find a reliable centre. The caller decides what
// to do about that -- it is not an error, it is "this image did not give me one",
// and on a badly aimed shot that is the correct answer.
//
// `gray` must ALREADY be single-channel and blurred as the caller wants it; the
// preparation stays with the caller because pattern_center takes a `blur` param
// and auto_center pins the blur instead (see the RawNodesPin note below).
cv::Point2d patternCenterFit(const cv::Mat &gray) {
    const cv::Point2d bad(-1, -1);
    const int W = gray.cols, H = gray.rows;
    if (W < 3 || H < 3) return bad;

    cv::Mat gx, gy, mag;
    cv::Sobel(gray, gx, CV_32F, 1, 0, 3);
    cv::Sobel(gray, gy, CV_32F, 0, 1, 3);
    cv::magnitude(gx, gy, mag);
    double mmin = 0, mmax = 0;
    cv::minMaxLoc(mag, &mmin, &mmax);
    if (mmax <= 0) return bad;
    const float thr = static_cast<float>(0.15 * mmax);  // keep clear ring edges

    // LS intersection of the gradient lines within `radius` of (cx0,cy0)
    // (radius <= 0 = whole image). Each strong edge gives a line with unit
    // normal (a,b) = (-gy,gx)/|g| and offset c = -(a*x + b*y); the residual
    // a*cx + b*cy + c is the signed distance of the centre from that line.
    const auto estimate = [&](double cx0, double cy0, double radius) {
        double Saa = 0, Sab = 0, Sbb = 0, Sac = 0, Sbc = 0;
        long cnt = 0;
        const double r2 = radius * radius;
        for (int y = 1; y < H - 1; y += 2) {
            const float *pgx = gx.ptr<float>(y);
            const float *pgy = gy.ptr<float>(y);
            const float *pm = mag.ptr<float>(y);
            for (int x = 1; x < W - 1; x += 2) {
                if (pm[x] < thr) continue;
                if (radius > 0) {
                    const double dx = x - cx0, dy = y - cy0;
                    if (dx * dx + dy * dy > r2) continue;
                }
                const double inv = 1.0 / pm[x];
                const double a = -pgy[x] * inv, b = pgx[x] * inv;
                const double c = -(a * x + b * y);
                Saa += a * a; Sab += a * b; Sbb += b * b;
                Sac += a * c; Sbc += b * c;
                ++cnt;
            }
        }
        cv::Point2d nope(-1, -1);
        if (cnt < 50) return nope;
        const double det = Saa * Sbb - Sab * Sab;
        if (std::abs(det) < 1e-9) return nope;
        return cv::Point2d((-Sac * Sbb + Sbc * Sab) / det, (Sac * Sab - Saa * Sbc) / det);
    };

    cv::Point2d c = estimate(0, 0, 0);  // whole image
    if (c.x < 0) return bad;
    const double refR = 0.22 * std::min(W, H);  // focus on the concentric core
    for (int i = 0; i < 2; ++i) {
        const cv::Point2d r = estimate(c.x, c.y, refR);
        if (r.x >= 0) c = r;
    }
    if (c.x < 0 || c.x >= W || c.y < 0 || c.y >= H) return bad;
    return c;
}

// ---- auto_center: the validated cascade ------------------------------------

// Gate A: a centre is accepted when the offset its ring spreads imply is under
// this. Measured, not guessed: on a clean synthetic pattern an exact centre
// reports 0.21 px (ring spreads ~0.15 px), so 2.0 px is about ten times the
// noise floor. That headroom is why no noise-floor subtraction is done -- see
// ring_center_offset_px in moilcali_algorithm.h.
constexpr double kGateOffsetPx = 2.0;
constexpr double kMarginalOffsetPx = 6.0;

// Gate B: the candidate must beat the best of four probes displaced kProbePx.
// With a true error e the nearest probe sits at (kProbePx - e), so the ratio is
// e/(kProbePx - e); <= 0.5 is exactly e <= 2.0 px, which is Gate A again. The two
// gates agree by construction rather than by tuning.
constexpr double kGateBasinRatio = 0.5;
constexpr double kMarginalBasinRatio = 0.8;
constexpr double kProbePx = 6.0;

// Gate C: coverage. A guard against a degenerate fit, not a quality measure --
// Gates A and B do the discriminating.
//
// The ray fraction was 0.75 until the bezel case in cpp/tests/AutoCenterTest.cpp
// (on v2.0_2026_main-cpp-ros -- that test is not in this branch)
// measured what an occluded image actually costs. A dark band across the pattern
// takes out the rays that cross it -- an angular sector, so a fixed fraction of
// the whole -- and the measurement is 86/120 rays at 120 (71.7%) and 521/720 at
// the search's own ray count (72.4%). Those rays are not evidence of a wrong
// centre: at that very point the offset was 0.306 px and the basin ratio 0.074,
// both comfortably inside their gates, and the centre was exact. 0.75 therefore
// rejected a pixel-perfect answer on precisely the kind of image the rig
// produces, since a monitor bezel IS this.
//
// 0.5 is anchored on the engine rather than on that measurement: ringCost gives
// up entirely below nRays/3 (see moilcali_algorithm.cpp), so a third is the hard
// floor already enforced one level down, and half leaves this gate doing work the
// engine does not while tolerating an occlusion of up to ~180 degrees -- far more
// than a bezel. Deliberately not set near the measured 0.72, which would be
// fitting the threshold to the one image that exposed the problem.
constexpr double kGateRingFraction = 0.8;
constexpr double kGateRayFraction = 0.5;
constexpr int kMinRings = 3;

constexpr int kValidateRays = 120;
constexpr double kSeededSearchRadius = 30.0;
constexpr double kFullSearchRadius = 150.0;
constexpr int kDefaultMaxSeconds = 30;

// Rings whose angular spread exceeds this multiple of the median spread are left
// out of the VALIDATION cost (never out of the reported arrays).
//
// This is not a refinement -- without it Gate B does not work on a contaminated
// image. Measured on the synthetic bezel case in cpp/tests/RingCenterDump.cpp
// (on v2.0_2026_main-cpp-ros -- that test is not in this branch): a
// flat dark band across the pattern gets counted as an extra outermost ring on
// the rays that survive it, and that ring comes back with a 63 px spread against
// 0.15 px for the inner rings. The engine's cost is a plain MEAN over rings, so
// that one term is ~400x every other and dominates the total. It is also
// common-mode -- it describes the bezel, not where the centre is -- so it lands
// in the candidate and in all four probes alike and cancels toward 1:
//
//     at the TRUE centre, untrimmed:  cost 0.02595  minProbe 0.04922  ratio 0.527
//     at the TRUE centre, trimmed:    cost 0.00143  minProbe 0.02999  ratio 0.048
//
// 0.527 FAILS the 0.5 gate. Untrimmed, a pixel-perfect centre would be rejected
// and escalated for nothing, on exactly the kind of image the escalation cannot
// improve. Trimmed it passes with an order of magnitude to spare.
//
// Why 5 and not 3 or 10: the measurement cannot separate them -- contamination
// there is 30x and 409x the median, so every factor from 3 to 10 drops the same
// two rings, and on the clean control every factor keeps all 8 and moves the
// ratio not at all. The number is therefore reasoned from the gap, not fitted to
// it. Clean ring spreads vary by about +/-6% around their median, and a real
// capture can legitimately have outer rings a few times worse than inner ones
// through lens distortion or panel tilt -- which ring_center_offset_px's comment
// calls out as a signal to READ, not an error to remove. 5x sits above that
// legitimate band and far below the 30x floor of actual contamination.
constexpr double kSpreadOutlierFactor = 5.0;

// Which rings the validation cost may use. Positional, and computed ONCE at the
// candidate: the same ring indices are then scored at every probe, so numerator
// and denominator sum over the same rings and the ratio is a function of centre
// position alone. Recomputing the outlier set per probe would let a probe trim a
// different ring and quietly compare two different quantities.
std::vector<char> keepRings(const MoilCali::RingCenter &r, double factor) {
    std::vector<char> keep(r.ringSpread.size(), 1);
    if (r.ringSpread.empty() || r.ringSpread.size() != r.ringRadius.size()) return keep;

    std::vector<double> s = r.ringSpread;
    std::sort(s.begin(), s.end());
    const double med = s[s.size() / 2];
    if (med <= 0) return keep;

    for (std::size_t k = 0; k < r.ringSpread.size(); ++k)
        if (r.ringSpread[k] > factor * med) keep[k] = 0;
    return keep;
}

// The engine's cost (mean over rings of spread/radius) restricted to `keep`.
// -1 when no kept ring is usable.
double trimmedCost(const MoilCali::RingCenter &r, const std::vector<char> &keep) {
    if (r.ringSpread.size() != r.ringRadius.size()) return -1.0;
    double total = 0.0;
    int used = 0;
    for (std::size_t k = 0; k < r.ringSpread.size(); ++k) {
        if (k < keep.size() && !keep[k]) continue;
        if (r.ringRadius[k] <= 1.0) continue;
        total += r.ringSpread[k] / r.ringRadius[k];
        ++used;
    }
    return used ? total / used : -1.0;
}

struct Verdict {
    bool evaluated = false;   // false when the point had no usable rings at all
    double offsetPx = -1, cost = -1, basinRatio = -1;
    int ringsUsed = 0, raysUsed = 0, raysTotal = 0;
    std::vector<double> ringRadius, ringSpread;
    std::vector<double> ringA1, ringB1, ringA2, ringB2;
    // Which rings the VALIDATION COST used, index-aligned with the arrays above.
    // Reported so a consumer of the harmonics knows which rings the cost trusted
    // -- the harmonics themselves are given for every detected ring, trimmed or
    // not, because a ring excluded from the cost still carries a real angular
    // measurement and a positioning loop may well want it.
    std::vector<char> ringKept;
    QString confidence = QStringLiteral("failed");
    QString reason;

    bool good() const { return confidence == QLatin1String("good"); }
    bool usable() const { return good() || confidence == QLatin1String("marginal"); }
};

// Gates A, B and C at one point. Five ring-cost evaluations: the candidate and
// four probes. This is the whole reason auto_center is one op -- driven from a
// client each of these would be a round trip.
Verdict judge(const cv::Mat &g, int K, cv::Point2d at, int expectedRings) {
    Verdict v;
    const MoilCali::RingCenter fit = MoilCali::evaluate_ring_center(g, K, at, kValidateRays);
    if (!fit.ok()) {
        v.reason = QStringLiteral("no usable rings at this point");
        return v;
    }

    v.evaluated = true;
    v.ringsUsed = fit.ringsUsed;
    v.raysUsed = fit.raysUsed;
    v.raysTotal = fit.raysTotal;
    v.ringRadius = fit.ringRadius;
    v.ringSpread = fit.ringSpread;
    v.ringA1 = fit.ringA1;
    v.ringB1 = fit.ringB1;
    v.ringA2 = fit.ringA2;
    v.ringB2 = fit.ringB2;
    // Untrimmed on purpose: the offset estimate is already a MEDIAN over rings, so
    // two contaminated rings out of eight cannot move it (measured: 0.218 px on
    // the bezel image, against a true error of 0.000 px). Only the cost, which is
    // a mean, needs the trimming.
    v.offsetPx = MoilCali::ring_center_offset_px(fit);

    const std::vector<char> keep = keepRings(fit, kSpreadOutlierFactor);
    v.ringKept = keep;
    v.cost = trimmedCost(fit, keep);

    double minProbe = -1;
    const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (const auto &d : dirs) {
        const MoilCali::RingCenter q = MoilCali::evaluate_ring_center(
            g, K, {at.x + d[0] * kProbePx, at.y + d[1] * kProbePx}, kValidateRays);
        if (!q.ok()) continue;
        const double cq = trimmedCost(q, keep);
        if (cq > 0 && (minProbe < 0 || cq < minProbe)) minProbe = cq;
    }
    if (v.cost > 0 && minProbe > 0) v.basinRatio = v.cost / minProbe;

    // rings_used counts rings DETECTED, before any trimming.
    //
    // The 5x-median trim (kSpreadOutlierFactor) applies to the COST SUM only --
    // it decides which rings are averaged into the number Gate B compares, and
    // nothing else. It must not reach this gate.
    //
    // The bezel case is why. There, all 8 rings are detected and 2 of them are
    // contaminated, so the cost is averaged over 6 while rings_used stays 8. Were
    // this gate to count post-trim it would see 6 against a want of
    // 0.8 * 8 = 6.4, and reject a centre that is exact to a third of a pixel --
    // punishing the fit for the very contamination the trimming just handled.
    // Detection and scoring are different questions: "did the pattern show up?"
    // and "which rings can be trusted to score it?".
    const int wantRings =
        std::max(kMinRings, static_cast<int>(std::lround(kGateRingFraction * expectedRings)));
    const bool coverage = v.ringsUsed >= wantRings &&
                          v.raysUsed >= static_cast<int>(kGateRayFraction * v.raysTotal);

    if (!coverage) {
        v.reason = QStringLiteral("coverage: %1 rings (want %2), %3/%4 rays")
                       .arg(v.ringsUsed).arg(wantRings).arg(v.raysUsed).arg(v.raysTotal);
        return v;
    }
    if (v.basinRatio < 0) {
        v.reason = QStringLiteral("no probe produced a usable cost");
        return v;
    }

    const bool aGood = v.offsetPx >= 0 && v.offsetPx <= kGateOffsetPx;
    const bool bGood = v.basinRatio <= kGateBasinRatio;
    if (aGood && bGood) {
        v.confidence = QStringLiteral("good");
        return v;
    }
    if ((v.offsetPx >= 0 && v.offsetPx <= kMarginalOffsetPx) &&
        v.basinRatio <= kMarginalBasinRatio) {
        v.confidence = QStringLiteral("marginal");
        v.reason = QStringLiteral("offset %1 px, basin ratio %2")
                       .arg(v.offsetPx, 0, 'f', 2).arg(v.basinRatio, 0, 'f', 3);
        return v;
    }
    v.reason = QStringLiteral("offset %1 px (want <= %2), basin ratio %3 (want <= %4)")
                   .arg(v.offsetPx, 0, 'f', 2).arg(kGateOffsetPx, 0, 'f', 1)
                   .arg(v.basinRatio, 0, 'f', 3).arg(kGateBasinRatio, 0, 'f', 1);
    return v;
}

QJsonObject stageRecord(const QString &method, cv::Point2d c, const Verdict &v) {
    QJsonObject o;
    o["method"] = method;
    o["cx"] = c.x;
    o["cy"] = c.y;
    o["offset_px"] = v.offsetPx;
    o["cost"] = v.cost;
    o["basin_ratio"] = v.basinRatio;
    o["rings_used"] = v.ringsUsed;
    o["rays_used"] = v.raysUsed;
    o["rays_total"] = v.raysTotal;
    o["confidence"] = v.confidence;
    if (!v.reason.isEmpty()) o["reason"] = v.reason;
    return o;
}

// Forces the process-wide raw-nodes flag off for as long as it is in scope, and
// puts it back on the way out.
//
// blur_gray() consults that flag: with raw nodes ON it skips the 3x3 box blur.
// The flag exists for NODE EXTRACTION, where the blur moves a crossing by a
// fraction of a pixel and can merge two rings near the edge -- a deliberate
// choice about a different measurement. Left unpinned it would reach in here and
// silently change where the centre lands, so the same capture would centre
// differently depending on a toggle that has nothing to do with centring, and
// nothing downstream would look wrong. Centre-finding is therefore always done
// on the blurred image, whatever the caller set.
//
// RAII rather than a save/restore pair because the cascade returns from several
// places. Safe despite the flag being process-wide: every detect op runs under
// computeMutex, so no other request can observe the window.
struct RawNodesPin {
    bool prev;
    RawNodesPin() : prev(MoilCali::raw_nodes_enabled()) { MoilCali::set_raw_nodes(false); }
    ~RawNodesPin() { MoilCali::set_raw_nodes(prev); }
    RawNodesPin(const RawNodesPin &) = delete;
    RawNodesPin &operator=(const RawNodesPin &) = delete;
};

// ---- where the ring count comes from ---------------------------------------

QString g_preparedDir;

// The prepared pattern belonging to a capture slot.
//
// PreparePatterns renders FOUR files (monitor_patterns.cpp:189-194):
// concentric_positive, concentric_negative, stripeline_positive,
// stripeline_negative -- and the negative of each is the SAME spec with the
// colour pair swapped, not a different geometry.
//
// Swapped colours can still change the ring COUNT, which is why the slot has to
// pick the file rather than both slots reading the positive one. renderConcentric
// draws onto a white canvas, so whether there is an intensity edge at the
// outermost boundary depends on the colour of the outermost layer: black against
// white is a boundary, white against white is not. Positive and negative can
// therefore differ by one crossing at the outer edge -- and a count that is one
// too high makes every ray fall short of it, so every ray is dropped and the
// centre is not found at all.
//
// Only the concentric pattern is relevant: it is the one with rings, and the one
// PreparePatterns sends to the TOP panel that the pos/neg shots photograph.
QString preparedPatternPath(const QString &slot) {
    if (g_preparedDir.isEmpty()) return {};
    const QString s = slot.trimmed().toLower();
    // Anything that is not explicitly the negative slot reads the positive file.
    // slotsFor() defaults a one-image op to "positive" when the caller names no
    // slot, so this matches what the images themselves will be.
    const QString name = (s == QLatin1String("negative")) ? QStringLiteral("concentric_negative")
                                                          : QStringLiteral("concentric_positive");
    return g_preparedDir + QLatin1Char('/') + name + QStringLiteral(".png");
}

}  // namespace

void setPreparedDir(const QString &dir) { g_preparedDir = dir; }
QString preparedDir() { return g_preparedDir; }

int detectImageCount(const QString &op) {
    if (op == detect::kNodes8Dir || op == detect::kHistogram8Dir) return 2;
    if (op == detect::kFisheyeEdge || op == detect::kRoi || op == detect::kRoiExact ||
        op == detect::kRingCenter || op == detect::kPatternRingRadii ||
        op == detect::kPatternCenter || op == detect::kAutoCenter)
        return 1;
    return -1;
}

QString runDetectOp(const QString &op, const std::vector<cv::Mat> &images,
                    const QString &paramsJson, QString *err) {
    const auto fail = [&](const QString &m) {
        if (err) *err = m;
        return QString();
    };

    const int want = detectImageCount(op);
    if (want < 0) return fail("unknown detect op: " + op);
    if (static_cast<int>(images.size()) < want)
        return fail(QString("op %1 needs %2 image(s), got %3")
                        .arg(op).arg(want).arg(images.size()));
    for (int i = 0; i < want; ++i)
        if (images[i].empty()) return fail(QString("image %1 is empty or undecodable").arg(i));

    bool ok = false;
    const QJsonObject p = parseObject(paramsJson, &ok);
    if (!ok) return fail("params is not a JSON object");

    // raw_nodes: no blur, no near-black crossing guard. Absent means false, so a
    // caller that does not ask for it gets the behaviour it always had.
    const CleaningScope cleaning(getBool(p, "noise_cleaning", false),
                                 getBool(p, "raw_nodes", false));

    if (op == detect::kNodes8Dir) {
        const cv::Point posC(getInt(p, "pos_cx", 0), getInt(p, "pos_cy", 0));
        const cv::Point negC(getInt(p, "neg_cx", 0), getInt(p, "neg_cy", 0));
        const cv::Mat posGray = MoilCali::blur_gray(images[0]);
        const cv::Mat negGray = MoilCali::blur_gray(images[1]);
        QJsonObject out;
        out["nodes"] = encodeNodeDict(
            MoilCali::get_dict_8direction_intersecting_nodes(posGray, negGray, posC, negC));
        return dump(out);
    }

    if (op == detect::kHistogram8Dir) {
        // One call for the whole curve panel. Done per direction it would be 16
        // round trips to draw one graph, and the two images would be re-sent for
        // each of them.
        const cv::Point posC(getInt(p, "pos_cx", 0), getInt(p, "pos_cy", 0));
        const cv::Point negC(getInt(p, "neg_cx", 0), getInt(p, "neg_cy", 0));
        const cv::Mat posGray = MoilCali::blur_gray(images[0]);
        const cv::Mat negGray = MoilCali::blur_gray(images[1]);

        QStringList dirs;
        for (const QJsonValue &v : p.value("dirs").toArray()) dirs << v.toString();
        if (dirs.isEmpty())
            for (const char *d : kDirs8) dirs << QString::fromLatin1(d);

        QJsonObject posOut, negOut, nodesOut;
        for (const QString &d : dirs) {
            const std::string ds = d.toStdString();
            const std::vector<int> pc = MoilCali::get_pattern_histogram_gray(posGray, posC, ds);
            const std::vector<int> nc = MoilCali::get_pattern_histogram_gray(negGray, negC, ds);
            posOut[d] = fromIntVec(pc);
            negOut[d] = fromIntVec(nc);
            // Raw crossings of THIS pair of curves, with no diagonal sqrt(2)
            // scaling and no cross-direction reconciliation. The curve panel draws
            // its own x-axis scaling, and applying the 8-direction pipeline's
            // adjustments here would put the white lines somewhere the two plotted
            // curves do not actually cross.
            nodesOut[d] = fromDoubleVec(MoilCali::get_list_intersecting_nodes(pc, nc));
        }
        QJsonObject out;
        out["pos"] = posOut;
        out["neg"] = negOut;
        out["nodes"] = nodesOut;
        return dump(out);
    }

    if (op == detect::kFisheyeEdge) {
        const auto r = MoilCali::detect_fisheye_edge(images[0], getInt(p, "threshold", 0));
        QJsonObject out;
        out["cx"] = r.first.x;
        out["cy"] = r.first.y;
        out["radius"] = r.second;
        return dump(out);
    }

    if (op == detect::kRoi) {
        const cv::Point r = MoilCali::detect_roi(images[0], getInt(p, "org_x", 0),
                                                 getInt(p, "org_y", 0), getInt(p, "threshold", 0));
        QJsonObject out;
        out["x"] = r.x;
        out["y"] = r.y;
        return dump(out);
    }

    // NEW IN v2.1.0. Both of the ops below came out of the client's main window,
    // where they were private methods over a cv::Mat. They are here because the
    // client has no cv::Mat any more -- see client/CMakeLists.txt -- and the
    // algorithms are unchanged from the ones that ran there.
    if (op == detect::kRoiExact) {
        // Recurse detect_roi until the centre stops moving, or 20 times. Ports
        // ControllerMain::findExactPoint.
        //
        // ONE op rather than the client looping: each iteration is a full-frame
        // scan, and driving the recursion from a remote would be up to 21 round
        // trips carrying a 3040x3040 capture on the first of them.
        const int thr = getInt(p, "threshold", 0);
        int x = getInt(p, "org_x", 0), y = getInt(p, "org_y", 0);
        int iterations = 0;
        for (; iterations <= 20; ++iterations) {
            const cv::Point n = MoilCali::detect_roi(images[0], x, y, thr);
            if (n.x == x && n.y == y) break;
            x = n.x;
            y = n.y;
        }
        QJsonObject out;
        out["x"] = x;
        out["y"] = y;
        // Reported so a caller can tell "settled immediately" from "ran out of
        // iterations", which are the same answer with different trustworthiness.
        out["iterations"] = iterations;
        out["converged"] = iterations <= 20;
        return dump(out);
    }

    if (op == detect::kPatternCenter) {
        // The fit itself is patternCenterFit() above -- moved there unchanged so
        // auto_center runs the same arithmetic rather than a second copy of it.
        // What stays here is this op's own contract: the `blur` parameter, and the
        // rounding to integer pixels that every existing caller expects.
        cv::Mat gray = images[0];
        if (gray.channels() != 1) cv::cvtColor(gray, gray, cv::COLOR_BGR2GRAY);
        if (getBool(p, "blur", true)) gray = MoilCali::blur_gray(gray);

        QJsonObject out;
        out["cx"] = -1;
        out["cy"] = -1;
        const cv::Point2d c = patternCenterFit(gray);
        if (c.x < 0) return dump(out);

        out["cx"] = static_cast<int>(std::lround(c.x));
        out["cy"] = static_cast<int>(std::lround(c.y));
        return dump(out);
    }

    if (op == detect::kRingCenter) {
        const cv::Mat gray = MoilCali::blur_gray(images[0]);
        const cv::Point2d seed(getDouble(p, "seed_x", gray.cols / 2.0),
                               getDouble(p, "seed_y", gray.rows / 2.0));
        const MoilCali::RingCenter r =
            MoilCali::find_center_from_rings(gray, getInt(p, "expected_rings", 0), seed,
                                            getDouble(p, "search_radius", 150.0));
        QJsonObject out;
        out["ok"] = r.ok();
        out["cx"] = r.center.x;
        out["cy"] = r.center.y;
        out["cost"] = r.cost;
        out["rings_used"] = r.ringsUsed;
        out["rays_used"] = r.raysUsed;
        out["rays_total"] = r.raysTotal;
        out["ring_radius"] = fromDoubleVec(r.ringRadius);
        out["ring_spread"] = fromDoubleVec(r.ringSpread);
        // Per-ring angular harmonics of radius(theta), px. Additive only -- see
        // the RingCenter note in moilcali_algorithm.h for what they observe.
        out["ring_a1"] = fromDoubleVec(r.ringA1);
        out["ring_b1"] = fromDoubleVec(r.ringB1);
        out["ring_a2"] = fromDoubleVec(r.ringA2);
        out["ring_b2"] = fromDoubleVec(r.ringB2);
        return dump(out);
    }

    if (op == detect::kAutoCenter) {
        // Stage 1 answers in milliseconds and is right most of the time; what it
        // cannot do is say so. Stages 2 and 3 are the accurate search, run only
        // when stage 1's answer does not survive its own validation -- so a good
        // shot costs about what pattern_center always cost, and a bad one is told
        // apart from a good one instead of being written into the form as if it
        // were fine.
        const RawNodesPin pin;

        QElapsedTimer clock;
        clock.start();
        const int maxSeconds = getInt(p, "max_seconds", kDefaultMaxSeconds);
        const auto outOfTime = [&] {
            return maxSeconds > 0 && clock.elapsed() > qint64(maxSeconds) * 1000;
        };

        const bool escalate = getBool(p, "escalate", false);
        const cv::Mat gray = MoilCali::blur_gray(images[0]);
        if (gray.empty()) return fail("auto_center: image did not convert to grayscale");
        const cv::Point2d imageCentre(gray.cols / 2.0, gray.rows / 2.0);

        // ---- where the ring count comes from -------------------------------
        //
        // params.expected_rings  ->  the slot's prepared PNG  ->  modal count.
        //
        // The count is what makes the ring method robust: rays that find a
        // different number of edges (bezel, monitor gap, glare) get DROPPED
        // instead of being silently mis-indexed by one ring. So the order is by
        // how much each source knows.
        //
        //   params  -- the caller states it outright. Nothing to second-guess.
        //   PNG     -- measured off the picture that was actually on the glass,
        //              which is the ground truth for what the camera saw. Beats
        //              the modal count because it is not inferred from the very
        //              capture whose centre is in question.
        //   modal   -- the most common crossing count over 72 probe rays. Works,
        //              but moilcali_algorithm.h:128-129 says outright it is the
        //              less reliable option, and on a contaminated image it can
        //              settle on a count that includes a bezel edge.
        //
        // The PNG wins over the modal count when they disagree, but BOTH are
        // reported. A prepared pattern that is stale -- the operator changed the
        // pattern and did not press Update to Monitor -- looks exactly like a
        // correct one from here, and the only symptom would be a centre that
        // quietly fails to converge. Putting the two numbers side by side in the
        // result makes that visible instead of mysterious.
        const QString slot = p.contains("slot")
                                 ? p.value("slot").toString()
                                 : p.value("slots").toArray().isEmpty()
                                       ? QString()
                                       : p.value("slots").toArray().at(0).toString();

        // The PNG is read even when params already supplied the count, so the two
        // can be compared. That is the whole point of reporting both: a prepared
        // pattern the operator forgot to update looks identical to a correct one
        // from in here. One imread of a flat two-colour PNG against a cascade
        // measured in seconds is not a cost worth optimising away.
        const QString patternPath = preparedPatternPath(slot);
        int pngRings = 0;
        if (!patternPath.isEmpty()) {
            const cv::Mat pat = cv::imread(patternPath.toStdString(), cv::IMREAD_GRAYSCALE);
            if (!pat.empty())
                pngRings = static_cast<int>(MoilCali::measure_pattern_ring_radii(pat).size());
        }

        int expectedRings = getInt(p, "expected_rings", 0);
        QString ringsSource = QStringLiteral("params");
        if (expectedRings <= 0) {
            if (pngRings >= kMinRings) {
                expectedRings = pngRings;
                ringsSource = QStringLiteral("png");
            } else {
                // Left at 0, which is how find_center_from_rings and
                // evaluate_ring_center are told to derive it themselves.
                ringsSource = QStringLiteral("modal");
            }
        }

        QJsonArray stages;
        QString method = QStringLiteral("none");
        QString confidence = QStringLiteral("failed");
        cv::Point2d best(-1, -1);
        Verdict bestV;

        // ---- stage 1: the gradient fit, then judged --------------------------
        const cv::Point2d c1 = patternCenterFit(gray);
        Verdict v1;
        if (c1.x < 0) {
            v1.reason = QStringLiteral("gradient fit found no centre");
            stages.append(stageRecord(QStringLiteral("pattern_center"), c1, v1));
        } else {
            v1 = judge(gray, expectedRings, c1, expectedRings);
            stages.append(stageRecord(QStringLiteral("pattern_center"), c1, v1));
            best = c1;
            bestV = v1;
            method = QStringLiteral("pattern_center");
            confidence = v1.confidence;
        }

        // The automatic post-shot path stops here whatever the verdict, and hands
        // back stage 1's point with an honest label. Escalating on every shot
        // would put a multi-second search at the shutter and hold computeMutex --
        // and process-wide, that is every other client's ops too.
        const bool done = v1.good() || !escalate;

        // ---- stage 2: the ring search, seeded ------------------------------
        // Seeded from stage 1 when it produced anything at all: even a rejected
        // gradient fit is usually within a few px, and that shrinks the coarse
        // grid from 31x31 = 961 evaluations to 7x7 = 49.
        if (!done && !outOfTime()) {
            cv::Point2d seed = (c1.x >= 0) ? c1 : imageCentre;
            if (c1.x < 0) {
                // Only now is fisheye_edge worth its threshold: it finds the LENS
                // circle, whose centre is not the pattern centre, so it is a seed
                // of last resort rather than a stage of its own.
                const auto lens = MoilCali::detect_fisheye_edge(images[0], getInt(p, "threshold", 0));
                if (lens.second > 0) seed = cv::Point2d(lens.first.x, lens.first.y);
            }
            const MoilCali::RingCenter r2 =
                MoilCali::find_center_from_rings(gray, expectedRings, seed, kSeededSearchRadius);
            Verdict v2;
            if (!r2.ok()) {
                v2.reason = QStringLiteral("seeded ring search found no centre");
                stages.append(stageRecord(QStringLiteral("ring_center"), {-1, -1}, v2));
            } else {
                v2 = judge(gray, expectedRings, r2.center, expectedRings);
                stages.append(stageRecord(QStringLiteral("ring_center"), r2.center, v2));
                if (v2.usable() || !bestV.evaluated) {
                    best = r2.center;
                    bestV = v2;
                    method = QStringLiteral("ring_center");
                    confidence = v2.confidence;
                }
            }

            // ---- stage 3: the ring search, full radius ----------------------
            if (!v2.usable() && !outOfTime()) {
                const MoilCali::RingCenter r3 = MoilCali::find_center_from_rings(
                    gray, expectedRings, imageCentre, kFullSearchRadius);
                Verdict v3;
                if (!r3.ok()) {
                    v3.reason = QStringLiteral("full ring search found no centre");
                    stages.append(stageRecord(QStringLiteral("ring_center_full"), {-1, -1}, v3));
                } else {
                    v3 = judge(gray, expectedRings, r3.center, expectedRings);
                    stages.append(stageRecord(QStringLiteral("ring_center_full"), r3.center, v3));
                    if (v3.usable()) {
                        best = r3.center;
                        bestV = v3;
                        method = QStringLiteral("ring_center_full");
                        confidence = v3.confidence;
                    }
                }
            }
        }

        // On the escalating path a centre nothing could vouch for is NOT handed
        // back as a point: the client falls through to its roi_exact fallback, and
        // a wrong centre written into the form silently would be worse than no
        // answer. The automatic path keeps stage 1's point regardless, because
        // that is what it returned before this op existed and losing it would be a
        // regression dressed up as caution.
        const bool haveAnswer = escalate ? (best.x >= 0 && bestV.usable()) : (best.x >= 0);
        if (!haveAnswer) {
            best = cv::Point2d(-1, -1);
            method = QStringLiteral("none");
            confidence = QStringLiteral("failed");
            // And the metrics go with it. Left in, the result read "no centre"
            // and "offset 0.306 px, basin ratio 0.074" at the same time -- the
            // numbers of the best candidate the cascade REJECTED, sitting in
            // fields that describe the answer. Anyone reading the top level would
            // have seen a good-looking fit next to a refusal. The per-stage
            // numbers are still in stages[], which is where a rejected candidate
            // belongs.
            bestV = Verdict{};
        }

        QJsonObject out;
        out["ok"] = haveAnswer;
        out["cx"] = best.x;
        out["cy"] = best.y;
        out["method"] = method;
        out["confidence"] = confidence;
        out["offset_px"] = bestV.offsetPx;
        out["cost"] = bestV.cost;
        out["basin_ratio"] = bestV.basinRatio;
        out["expected_rings"] = expectedRings;
        // Which of the three sources actually supplied it, and what the prepared
        // pattern says regardless. expected_rings_png is 0 when no prepared
        // pattern was found -- either setPreparedDir was never called or the
        // operator has not pressed Update to Monitor. A png value that disagrees
        // with a params value is the stale-pattern signal.
        out["expected_rings_source"] = ringsSource;
        out["expected_rings_png"] = pngRings;
        out["pattern_path"] = patternPath;
        out["slot"] = slot;
        out["rings_used"] = bestV.ringsUsed;
        out["rays_used"] = bestV.raysUsed;
        out["rays_total"] = bestV.raysTotal;
        // Raw, never trimmed: the trimming is a decision about the VALIDATION
        // cost, and a caller reading these to work out whether a ring is
        // contaminated must see what was actually measured.
        out["ring_radius"] = fromDoubleVec(bestV.ringRadius);
        out["ring_spread"] = fromDoubleVec(bestV.ringSpread);
        // Per-ring angular harmonics of radius(theta), px, for EVERY detected
        // ring. The 2-lobed pair is the ellipticity a tilted camera produces and
        // a mis-centred one cannot, which is what makes pitch/yaw separable from
        // X/Y in a single image at all. ring_kept says which of these rings the
        // validation cost averaged, so a caller can see the trim without the
        // trim removing data from it.
        out["ring_a1"] = fromDoubleVec(bestV.ringA1);
        out["ring_b1"] = fromDoubleVec(bestV.ringB1);
        out["ring_a2"] = fromDoubleVec(bestV.ringA2);
        out["ring_b2"] = fromDoubleVec(bestV.ringB2);
        QJsonArray kept;
        for (char c : bestV.ringKept) kept.append(c != 0);
        out["ring_kept"] = kept;
        // What was ASKED for, not what happened -- stages[] says what happened, and
        // on a good shot escalate=true still runs one stage. Named in full because
        // "escalated" would read as the latter.
        out["escalate_requested"] = escalate;
        out["elapsed_ms"] = static_cast<double>(clock.elapsed());
        out["timed_out"] = outOfTime();
        out["stages"] = stages;
        return dump(out);
    }

    if (op == detect::kPatternRingRadii) {
        // Measured on the pattern as drawn, so it must NOT be blurred the way a
        // camera capture is -- the caller sends the generated PNG itself.
        cv::Mat gray = images[0];
        if (gray.channels() != 1) cv::cvtColor(gray, gray, cv::COLOR_BGR2GRAY);
        QJsonObject out;
        out["radii"] = fromDoubleVec(MoilCali::measure_pattern_ring_radii(gray));
        return dump(out);
    }

    return fail("unhandled detect op: " + op);
}

}  // namespace ComputeOps
