#pragma once

#include <vector>

#include <QByteArray>
#include <QString>
#include <QVector>

#include <opencv2/core.hpp>

class CaliTableData;

// The calibration maths.
//
// This is what is left of ComputeRosClient once the transport is removed, and
// removing it took almost nothing away: that class already ended at these exact
// ComputeOps functions, and already called them in-process whenever the rig was
// not answering or MOILCALI_COMPUTE_LOCAL was set. What it added was encode,
// send, decode, and a five-second discovery wait before the first click.
//
// The class is kept rather than having callers use ComputeOps:: directly because
// detectEncoded() is a real service to them -- the captures are read from
// image_cali/*.png as file bytes, and something has to decode them.
class Compute {
public:
    Compute() = default;

    Compute(const Compute &) = delete;
    Compute &operator=(const Compute &) = delete;

    // The maths is compiled into this binary, so it is always available and never
    // remote. Both are kept so the status line and the callers that branched on
    // them did not have to be rewritten.
    bool available() const { return true; }
    bool lastCallWasRemote() const { return false; }

    // ---- image analysis ----------------------------------------------------
    // `images` in the order the op documents (pos then neg for the two-image ops).
    // Returns the op's result JSON, or an empty string with *err set.
    QString detect(const QString &op, const std::vector<cv::Mat> &images, const QString &params,
                   QString *err);

    // For callers holding the encoded file bytes, which is the usual case.
    QString detectEncoded(const QString &op, const QVector<QByteArray> &pngs,
                          const QString &params, QString *err);

    // ---- calibration pipeline ---------------------------------------------
    // Mutates `table` in place.
    bool cali(const QString &op, CaliTableData &table, const QString &params, QString *result,
              QString *err);

    // ---- pattern rendering -------------------------------------------------
    // width/height of 0 means "the size in the JSON".
    cv::Mat renderPattern(const QString &patternJson, int width, int height, QString *err);
};
