#include "ComputeOps.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>

#include "CaliCompute.h"
#include "CaliTableData.h"
#include "ComputeCleaning.h"
#include "ComputeJson.h"

namespace ComputeOps {
namespace {

using namespace ComputeOps::json;

// round_enabled: the client's per-round checkboxes. Sent every call rather than
// held as node state -- the node has no session, and a stale copy would quietly
// include or drop a whole round from every aggregation.
void applyRoundFlags(CaliCompute &c, const QJsonObject &p) {
    const QJsonValue v = p.value("round_enabled");
    if (!v.isArray()) return;
    const QJsonArray a = v.toArray();
    for (int r = 1; r <= 10 && r - 1 < a.size(); ++r) c.setRoundEnabled(r, a.at(r - 1).toBool(true));
}

QJsonObject encodeMinAggr(const CaliCompute::MinAggr &m) {
    QJsonObject o;
    o["best_distance"] = m.bestDistance;
    // 1e300 is the sentinel for "no data"; it survives JSON as inf on some
    // parsers, so say so with a flag instead of making the client recognise it.
    o["found"] = m.bestAggr < 1e299;
    o["best_aggr"] = (m.bestAggr < 1e299) ? m.bestAggr : 0.0;
    QJsonArray samples;
    for (const QPointF &pt : m.samples) {
        QJsonArray pair;
        pair.append(pt.x());
        pair.append(pt.y());
        samples.append(pair);
    }
    o["samples"] = samples;
    return o;
}

double finiteOrZero(double v, bool *found) {
    *found = v < 1e299;
    return *found ? v : 0.0;
}

}  // namespace

bool runCaliOp(const QString &op, CaliTableData &table, const QString &paramsJson,
               QString *resultJson, QString *err, const ProgressFn &progress) {
    const auto fail = [&](const QString &m) {
        if (err) *err = m;
        return false;
    };
    const auto done = [&](const QJsonObject &o) {
        if (resultJson) *resultJson = dump(o);
        return true;
    };

    bool ok = false;
    const QJsonObject p = parseObject(paramsJson, &ok);
    if (!ok) return fail("params is not a JSON object");

    const int round = getInt(p, "round", 0);
    if (round < 0 || round > 10) return fail(QString("round out of range: %1").arg(round));

    // updateTableFromCapture and the detection-derived steps read the cleaning
    // flag, so it is scoped for every op, not just the obviously image-related ones.
    const CleaningScope cleaning(getBool(p, "noise_cleaning", false));

    CaliCompute c(&table);
    applyRoundFlags(c, p);
    // Per request, like round_enabled: the node holds no session.
    c.setUseRoundDistances(getBool(p, "use_round_distances", false));

    // Only the distance searches report progress; naming the stage here rather than
    // inside CaliCompute keeps the label a presentation concern of the boundary,
    // where it belongs, instead of embedding UI wording in the maths.
    if (progress) {
        const QString stage = (op == cali::kFindMinAggregationInWindow ||
                               op == cali::kFindDistanceForTargetAggregation)
                                  ? QStringLiteral("distance sweep")
                                  : QStringLiteral("search");
        c.setProgress([&progress, stage](int done, int total) {
            return progress(done, total, stage);
        });
    }

    if (op == cali::kComputeAll) {
        c.computeAll();
        return done({});
    }

    // Answers with the round's aggregation, so Aggr Round needs no second op.
    if (op == cali::kCalculateResult) {
        c.calculateResult(round);
        bool found = false;
        const double v = finiteOrZero(c.roundAggregation(round), &found);
        QJsonObject out;
        out["found"] = found;
        out["value"] = v;
        return done(out);
    }

    if (op == cali::kCalculateResultSingleRound) {
        c.calculateResultSingleRound(round, getDouble(p, "distance", 200.0));
        return done({});
    }

    if (op == cali::kCalculateResultWithBaseDistance) {
        c.calculateResultWithBaseDistance(round, getDouble(p, "base_distance", 250.0));
        return done({});
    }

    if (op == cali::kAggregationByDistance) {
        bool found = false;
        const double v = finiteOrZero(c.aggregationByDistance(round, getDouble(p, "distance", 250.0)),
                                      &found);
        QJsonObject out;
        out["found"] = found;
        out["value"] = v;
        return done(out);
    }

    if (op == cali::kAggregationAllRoundsByDistance) {
        bool found = false;
        const double v = finiteOrZero(
            c.aggregationAllRoundsByDistance(getDouble(p, "base_distance", 250.0),
                                             getBool(p, "use_range", false),
                                             getDouble(p, "x_lo", 0), getDouble(p, "x_hi", 0)),
            &found);
        QJsonObject out;
        out["found"] = found;
        out["value"] = v;
        return done(out);
    }

    if (op == cali::kFindMinAggrSingleRound) {
        return done(encodeMinAggr(c.findMinAggrSingleRound(
            round, getDouble(p, "dist_min", 1), getDouble(p, "dist_max", 500),
            getInt(p, "max_iter", 30), getDouble(p, "tol", 1))));
    }

    if (op == cali::kFindMinAggregationInWindow) {
        return done(encodeMinAggr(c.findMinAggregationInWindow(
            getBool(p, "use_window", false), getDouble(p, "x_lo", 0), getDouble(p, "x_hi", 0))));
    }

    if (op == cali::kFindDistanceForTargetAggregation) {
        if (!p.contains("target")) return fail("find_distance_for_target_aggregation needs target");
        return done(encodeMinAggr(c.findDistanceForTargetAggregation(
            getDouble(p, "target", 0), getBool(p, "use_range", true), getDouble(p, "x_lo", 0),
            getDouble(p, "x_hi", 0))));
    }

    if (op == cali::kAutoDetectNoiseBands) {
        QJsonArray bands;
        for (const CaliCompute::NoiseBand &b : c.autoDetectNoiseBands(round)) {
            QJsonArray dirs;
            for (const QString &d : b.dirs) dirs.append(d);
            QJsonObject o;
            o["group"] = b.group;
            o["dirs"] = dirs;
            o["lo"] = b.lo;
            o["hi"] = b.hi;
            o["found"] = b.found;
            bands.append(o);
        }
        QJsonObject out;
        out["bands"] = bands;
        return done(out);
    }

    if (op == cali::kRemoveNodesInBand) {
        QStringList dirs;
        for (const QJsonValue &v : p.value("dirs").toArray()) dirs << v.toString();
        if (dirs.isEmpty()) return fail("remove_nodes_in_band needs a non-empty dirs array");
        QJsonObject out;
        out["removed"] = c.removeNodesInBand(round, dirs, getDouble(p, "lo", 0),
                                             getDouble(p, "hi", 0));
        return done(out);
    }

    if (op == cali::kUpdateTableFromCapture) {
        QVector<QString> pct;
        for (const QJsonValue &v : p.value("pct").toArray())
            // The PCT list is text in the generator too, and "" is not "0": a blank
            // entry falls back to "0" inside updateTableFromCapture, whereas a
            // number would already have decided that.
            pct.append(v.isString() ? v.toString() : QString::number(v.toDouble()));

        std::map<std::string, std::vector<double>> ict8;
        const QJsonObject nodes = p.value("ict8").toObject();
        for (auto it = nodes.constBegin(); it != nodes.constEnd(); ++it)
            ict8[it.key().toStdString()] = toDoubleVec(it.value());

        c.updateTableFromCapture(round, pct, ict8);
        return done({});
    }

    return fail("unknown cali op: " + op);
}

}  // namespace ComputeOps
