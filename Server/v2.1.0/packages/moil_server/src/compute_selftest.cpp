#include "compute_node.h"

#include <cmath>

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointF>
#include <QStandardPaths>
#include <QStringList>

#include <opencv2/imgproc.hpp>

#include "CaliCompute.h"
#include "CaliTableData.h"
#include "ComputeOps.h"

// Does the arithmetic still give the answers it gave?
//
// This is a DRIFT check, and the distinction is worth being exact about because
// the wrong claim here would be worse than no check at all. It does not know the
// true calibration of anything. What it knows is what THIS build computed the
// first time it ran: on a fresh install it records the answers and says so, and
// on every run afterwards it compares against that record.
//
// So a pass means "the numbers have not moved since the baseline was taken", not
// "the numbers are right". That is the property worth monitoring on a rig: the
// formulas were verified against hardware once, and what you want to be told
// about is the day a rebuild, a compiler change or an edit quietly shifts one.
//
// Where the baseline lives:
//   %LOCALAPPDATA%/MoilLab/FisheyeCalisys/compute_baseline.json
// Delete it to re-record. Do that deliberately -- re-recording after a change
// makes the change permanent and invisible, which is the one way to misuse this.
namespace {

constexpr double kTolerance = 1e-9;  // relative

QString baselinePath() {
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/compute_baseline.json");
}

QJsonObject loadBaseline() {
    QFile f(baselinePath());
    if (!f.open(QIODevice::ReadOnly)) return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}

void saveBaseline(const QJsonObject &o) {
    QFile f(baselinePath());
    if (!f.open(QIODevice::WriteOnly)) return;
    f.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
}

// A calibration table with fixed, made-up-but-plausible node data. The numbers
// are not measurements; they only have to be the SAME numbers every run, and to
// exercise the whole pipeline: raw ict per direction, several rounds, the side
// layers, and the scalar form fields the formulas read.
CaliTableData buildFixture() {
    CaliTableData t;
    const char *dirs[8] = {"n", "ne", "e", "se", "s", "sw", "w", "nw"};
    (void)dirs;

    for (int round = 1; round <= 3; ++round) {
        t.addRound(round);
        // Row 1 holds per-round metadata; layer L lives on row L+2. Column indices
        // follow CaliCompute's own mapping, so the fixture is written through the
        // same door the pipeline reads from.
        for (int layer = 0; layer < 12; ++layer) {
            const int row = layer + 2;
            for (int col = 0; col < 8; ++col) {
                // Deterministic and monotonic in both layer and round: a ring
                // pattern that grows outwards, which is the shape the formulas
                // expect and enough to make every derived column non-trivial.
                const double v = 40.0 + layer * 17.5 + round * 3.25 + col * 0.75;
                t.setCell(round, row, 8 + col, QString::number(v, 'f', 3));
            }
            t.setCell(round, row, 1, QString::number(layer));
        }
    }

    t.setField(QStringLiteral("lineedit_pixel_size_top"), QStringLiteral("0.2478"));
    t.setField(QStringLiteral("lineedit_pixel_size_side"), QStringLiteral("0.2724"));
    t.setField(QStringLiteral("lineedit_distance"), QStringLiteral("280"));
    t.setField(QStringLiteral("lineedit_dis_per_round"), QStringLiteral("20"));
    t.setField(QStringLiteral("lineedit_gap_top"), QStringLiteral("0"));
    t.setField(QStringLiteral("lineedit_gap_side"), QStringLiteral("0"));
    return t;
}

// A synthetic concentric pattern, rendered by the same renderer the glass uses.
QString fixturePatternJson() {
    return QStringLiteral(
        R"({"type":"concentric","width":256,"height":256,"crossline":false,"layers":[)"
        R"({"shape":"circle","radius":30,"rgb":[0,0,0]},)"
        R"({"shape":"circle","radius":60,"rgb":[255,255,255]},)"
        R"({"shape":"circle","radius":90,"rgb":[0,0,0]}]})");
}

// Compare one value against the baseline, recording it when there is none yet.
// `maxDev` accumulates the worst relative deviation seen in this suite.
void checkValue(QJsonObject &baseline, bool &fresh, const QString &key, double value,
                bool *passed, double *maxDev, QStringList *notes) {
    if (!baseline.contains(key)) {
        baseline[key] = value;
        fresh = true;
        return;
    }
    const double was = baseline.value(key).toDouble();
    const double scale = std::max(1.0, std::abs(was));
    const double dev = std::abs(value - was) / scale;
    if (dev > *maxDev) *maxDev = dev;
    if (dev > kTolerance) {
        *passed = false;
        notes->append(QStringLiteral("%1: was %2, now %3 (relative %4)")
                          .arg(key)
                          .arg(was, 0, 'g', 12)
                          .arg(value, 0, 'g', 12)
                          .arg(dev, 0, 'g', 3));
    }
}

ComputeSelfTest::SuiteResult runCali(QJsonObject &baseline, bool &fresh) {
    ComputeSelfTest::SuiteResult r;
    r.name = QStringLiteral("cali");
    r.passed = true;

    CaliTableData t = buildFixture();
    QString result, err;
    if (!ComputeOps::runCaliOp(ComputeOps::cali::kComputeAll, t, QStringLiteral("{}"), &result,
                               &err)) {
        r.passed = false;
        r.notes.append(QStringLiteral("compute_all failed: %1").arg(err));
        return r;
    }

    CaliCompute cc(&t);
    checkValue(baseline, fresh, "cali.max_ict", cc.maxIctAllRounds(), &r.passed, &r.maxDeviation,
               &r.notes);

    // The regression and the polynomial are the two results a client used to
    // compute for itself. They are checked here precisely because they moved
    // across the boundary in this version -- if the move changed a number, this
    // is where it shows.
    const QVector<double> poly = cc.alphaPolynomial(4);
    for (int i = 0; i < poly.size(); ++i)
        checkValue(baseline, fresh, QStringLiteral("cali.poly%1").arg(i), poly.at(i), &r.passed,
                   &r.maxDeviation, &r.notes);

    const QVector<QPointF> reg = cc.ihAlphaRegression(4);
    checkValue(baseline, fresh, "cali.reg_n", reg.size(), &r.passed, &r.maxDeviation, &r.notes);
    if (!reg.isEmpty()) {
        checkValue(baseline, fresh, "cali.reg_first_y", reg.first().y(), &r.passed,
                   &r.maxDeviation, &r.notes);
        checkValue(baseline, fresh, "cali.reg_last_y", reg.last().y(), &r.passed, &r.maxDeviation,
                   &r.notes);
    }

    // Aggregation at a fixed distance: one number that depends on every derived
    // column above it, so it is the cheapest way to notice a change anywhere in
    // the pipeline.
    checkValue(baseline, fresh, "cali.aggr_at_300", cc.aggregationAllRoundsByDistance(300.0),
               &r.passed, &r.maxDeviation, &r.notes);
    return r;
}

ComputeSelfTest::SuiteResult runPattern(QJsonObject &baseline, bool &fresh) {
    ComputeSelfTest::SuiteResult r;
    r.name = QStringLiteral("pattern");
    r.passed = true;

    QString err;
    const cv::Mat m = ComputeOps::renderPattern(fixturePatternJson(), 0, 0, &err);
    if (m.empty()) {
        r.passed = false;
        r.notes.append(QStringLiteral("renderPattern failed: %1").arg(err));
        return r;
    }
    checkValue(baseline, fresh, "pattern.cols", m.cols, &r.passed, &r.maxDeviation, &r.notes);
    checkValue(baseline, fresh, "pattern.rows", m.rows, &r.passed, &r.maxDeviation, &r.notes);
    // The mean of the rendered pixels: sensitive to a changed radius, a changed
    // colour or a changed anti-aliasing rule, and insensitive to nothing that
    // matters.
    checkValue(baseline, fresh, "pattern.mean", cv::mean(m)[0], &r.passed, &r.maxDeviation,
               &r.notes);
    return r;
}

ComputeSelfTest::SuiteResult runDetect(QJsonObject &baseline, bool &fresh) {
    ComputeSelfTest::SuiteResult r;
    r.name = QStringLiteral("detect");
    r.passed = true;

    // Detect the ring radii of the pattern this build just rendered. Two engines
    // in one check: if either the renderer or the detector moves, the radii move.
    QString err;
    const cv::Mat pattern = ComputeOps::renderPattern(fixturePatternJson(), 0, 0, &err);
    if (pattern.empty()) {
        r.passed = false;
        r.notes.append(QStringLiteral("could not render the fixture pattern: %1").arg(err));
        return r;
    }

    const QString out = ComputeOps::runDetectOp(ComputeOps::detect::kPatternRingRadii, {pattern},
                                                QStringLiteral("{}"), &err);
    if (out.isEmpty()) {
        r.passed = false;
        r.notes.append(QStringLiteral("pattern_ring_radii failed: %1").arg(err));
        return r;
    }
    const QJsonObject o = QJsonDocument::fromJson(out.toUtf8()).object();
    const QJsonArray radii = o.value(QStringLiteral("radii")).toArray();
    checkValue(baseline, fresh, "detect.ring_count", radii.size(), &r.passed, &r.maxDeviation,
               &r.notes);
    for (int i = 0; i < radii.size(); ++i)
        checkValue(baseline, fresh, QStringLiteral("detect.radius%1").arg(i),
                   radii.at(i).toDouble(), &r.passed, &r.maxDeviation, &r.notes);
    return r;
}

}  // namespace

QVector<ComputeSelfTest::SuiteResult> ComputeSelfTest::run(const QString &suite,
                                                           double *durationSeconds) {
    QElapsedTimer clock;
    clock.start();

    const QString want = suite.trimmed().isEmpty() ? QStringLiteral("all") : suite.trimmed();
    QJsonObject baseline = loadBaseline();
    bool fresh = false;

    QVector<SuiteResult> out;
    if (want == "all" || want == "cali") out.append(runCali(baseline, fresh));
    if (want == "all" || want == "pattern") out.append(runPattern(baseline, fresh));
    if (want == "all" || want == "detect") out.append(runDetect(baseline, fresh));

    if (fresh) {
        saveBaseline(baseline);
        for (SuiteResult &r : out)
            r.notes.prepend(QStringLiteral(
                "baseline recorded for the first time in %1 -- this run establishes the "
                "reference, it does not verify against one").arg(baselinePath()));
    }

    if (durationSeconds) *durationSeconds = clock.elapsed() / 1000.0;
    return out;
}
