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

// Image analysis by name over /compute/detect.
class ComputeController : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(ProbeStatus::Status status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QString activity READ activity NOTIFY busyChanged)

    // Last centre answer per slot.
    Q_PROPERTY(QVariantMap centers READ centers NOTIFY centersChanged)

    // Histogram curves per polarity and direction.
    Q_PROPERTY(QVariantMap histogram READ histogram NOTIFY histogramChanged)
    Q_PROPERTY(bool hasHistogram READ hasHistogram NOTIFY histogramChanged)

    // Per-direction intersecting nodes from nodes_8dir.
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

    // slot is "positive" or "negative".
    Q_INVOKABLE void autoCenter(const QString &slot, int expectedRings, bool noiseCleaning);

    // Settle an operator's click via roi_exact.
    Q_INVOKABLE void refineCenter(const QString &slot, int x, int y, int threshold);

    // One call: both polarities, eight directions.
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

    // Gathers slot bytes, or names the missing one.
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

    // Requests still expected to answer.
    QSet<quint64> liveTokens_;
    quint64 nextToken_ = 0;

    struct Impl;
    std::unique_ptr<Impl> d_;
};
