#pragma once

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QPair>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <memory>

#include "ProbeStatus.h"

// Image analysis, addressed by name, over /compute/detect.
//
// NOTHING HERE COMPUTES. Every op runs on the rig; this class marshals the
// captures out of ImageStore, sends the ORIGINAL compressed bytes, and turns the
// result JSON into something QML can bind to. That boundary is the one described
// in ComputeOps.h and it is not negotiable here: there is no OpenCV in this
// target and no second implementation of a centre fit to drift from the engine's.
//
// A CENTRE THAT IS NOT TRUSTWORTHY IS REPORTED AS NO CENTRE. auto_center answers
// with ok=false or (-1,-1) when its cascade cannot validate a fit, and that
// answer is passed straight through to centerRefused() -- it is never rounded up
// into a plausible-looking coordinate. The rig moves five axes off these numbers
// (see doc/auto_center_design.md), so a wrong centre is worse than none.
class ComputeController : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(ProbeStatus::Status status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QString activity READ activity NOTIFY busyChanged)

    // The last answer per slot: {"positive": {ok, x, y, method, confidence,
    // offsetPx, cost}, "negative": {...}}. Bound by the Centering panel.
    Q_PROPERTY(QVariantMap centers READ centers NOTIFY centersChanged)

    // {"pos": {dir: [counts]}, "neg": {dir: [counts]}, "nodes": {dir: [x]}}.
    // Empty until a Direction Diff has run; the histogram panels draw nothing
    // rather than inventing a curve when it is.
    Q_PROPERTY(QVariantMap histogram READ histogram NOTIFY histogramChanged)
    Q_PROPERTY(bool hasHistogram READ hasHistogram NOTIFY histogramChanged)

    // Per-direction intersecting-node lists from nodes_8dir, which is what a
    // round's ICT columns are built from.
    Q_PROPERTY(QVariantMap nodes READ nodes NOTIFY nodesChanged)
    Q_PROPERTY(bool hasNodes READ hasNodes NOTIFY nodesChanged)

public:
    explicit ComputeController(QObject *parent = nullptr);
    ~ComputeController() override;

    ProbeStatus::Status status() const { return status_; }
    bool busy() const { return busy_ > 0; }
    QString lastError() const { return lastError_; }
    QString activity() const { return activity_; }
    QVariantMap centers() const { return centers_; }
    QVariantMap histogram() const { return histogram_; }
    bool hasHistogram() const { return !histogram_.isEmpty(); }
    QVariantMap nodes() const { return nodes_; }
    bool hasNodes() const { return !nodes_.isEmpty(); }

    Q_INVOKABLE void connectTo(int domainId);

    // slot is "positive" or "negative" -- both an ImageStore key and the name
    // auto_center uses to find the prepared PNG it should count rings from.
    Q_INVOKABLE void autoCenter(const QString &slot, int expectedRings, bool noiseCleaning);

    // Settle an operator's click. roi_exact recurses detect_roi until the point
    // stops moving; a click that does not converge keeps the raw click and says
    // so rather than silently drifting somewhere else.
    Q_INVOKABLE void refineCenter(const QString &slot, int x, int y, int threshold);

    // One call for the whole curve panel, both polarities, all eight directions.
    Q_INVOKABLE void histogram8Dir(int posCx, int posCy, int negCx, int negCy, bool noiseCleaning);

    Q_INVOKABLE void nodes8Dir(int posCx, int posCy, int negCx, int negCy, bool noiseCleaning);

    Q_INVOKABLE void clearResults();

signals:
    void statusChanged();
    void busyChanged();
    void lastErrorChanged();
    void errorRaised(const QString &message);
    void notice(const QString &message);

    void centersChanged();
    void histogramChanged();
    void nodesChanged();

    void centerFound(const QString &slot, int x, int y, const QString &method,
                     const QString &confidence);
    void centerRefused(const QString &slot, const QString &reason);

private:
    Q_INVOKABLE void applyLink(int status, const QString &message, quint64 generation);
    Q_INVOKABLE void applyResult(const QString &op, const QString &slot, bool ok,
                                 const QString &resultJson, const QString &message, quint64 token,
                                 quint64 generation);

    void setStatus(ProbeStatus::Status status);
    void setLastError(const QString &message);
    void beginCall(const QString &what);
    void endCall();
    void stopWorker();

    // Collects the compressed bytes for the named slots out of ImageStore, or
    // reports exactly which one is missing. Returns false without sending
    // anything.
    //
    // The parameter is `slotNames`, not `slots`, because `slots` is a Qt keyword
    // macro that expands to nothing -- naming it that way silently strips the
    // parameter name and leaves the range-for with no range.
    bool gather(const QStringList &slotNames, QList<QPair<QByteArray, QString>> *out);

    bool sendDetect(const QString &op, const QString &slot, const QStringList &imageSlots,
                    const QString &paramsJson, const QString &activity);

    ProbeStatus::Status status_ = ProbeStatus::Unknown;
    int busy_ = 0;
    QString lastError_;
    QString activity_;
    QVariantMap centers_;
    QVariantMap histogram_;
    QVariantMap nodes_;

    // Tokens of the requests still expected to answer. Several ops can be in
    // flight (the two histogram panels and a centre fit are independent), so a
    // single "latest token" would drop the older reply AND leak its busy count,
    // leaving the panel disabled forever. Membership is the receipt.
    QSet<quint64> liveTokens_;
    quint64 nextToken_ = 0;

    struct Impl;
    std::unique_ptr<Impl> d_;
};
