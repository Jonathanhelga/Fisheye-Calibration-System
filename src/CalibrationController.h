#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <memory>

#include "ProbeStatus.h"

// The Cali Result window's table and cached answers.
class CalibrationController : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(ProbeStatus::Status status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString activity READ activity NOTIFY busyChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

    Q_PROPERTY(QString folder READ folder WRITE setFolder NOTIFY folderChanged)
    Q_PROPERTY(ProbeStatus::Status loadStatus READ loadStatus NOTIFY tableChanged)
    Q_PROPERTY(QString loadSummary READ loadSummary NOTIFY tableChanged)

    // 11 rounds of row objects.
    Q_PROPERTY(QVariantList rounds READ rounds NOTIFY tableChanged)
    Q_PROPERTY(QVariantList sideLayers READ sideLayers NOTIFY tableChanged)
    Q_PROPERTY(QVariantList roundEnabled READ roundEnabled NOTIFY tableChanged)
    Q_PROPERTY(QVariantMap fields READ fields NOTIFY tableChanged)
    Q_PROPERTY(int tableVersion READ tableVersion NOTIFY tableChanged)

    // Six coefficient slots as text.
    Q_PROPERTY(QVariantList coefficients READ coefficients NOTIFY seriesChanged)

    // Plot series as lists of {x, y}.
    Q_PROPERTY(QVariantList alphaRounds READ alphaRounds NOTIFY seriesChanged)
    Q_PROPERTY(QVariantList alphaFit READ alphaFit NOTIFY seriesChanged)
    Q_PROPERTY(QVariantList zflRounds READ zflRounds NOTIFY seriesChanged)
    Q_PROPERTY(QVariantList roundColors READ roundColors NOTIFY seriesChanged)
    Q_PROPERTY(double maxIct READ maxIct NOTIFY seriesChanged)
    Q_PROPERTY(bool hasRawIct READ hasRawIct NOTIFY seriesChanged)
    Q_PROPERTY(bool seriesFresh READ seriesFresh NOTIFY seriesChanged)

    // Scalar answers from the ops.
    Q_PROPERTY(QString aggregationText READ aggregationText NOTIFY resultChanged)
    Q_PROPERTY(QString noiseText READ noiseText NOTIFY resultChanged)

    // ---- distance searches (CaliJob) ----
    Q_PROPERTY(bool searchRunning READ searchRunning NOTIFY searchChanged)
    Q_PROPERTY(QString searchStage READ searchStage NOTIFY searchChanged)
    Q_PROPERTY(int searchDone READ searchDone NOTIFY searchChanged)
    Q_PROPERTY(int searchTotal READ searchTotal NOTIFY searchChanged)
    Q_PROPERTY(QString searchSummary READ searchSummary NOTIFY searchChanged)
    Q_PROPERTY(double bestDistance READ bestDistance NOTIFY searchChanged)
    Q_PROPERTY(double bestAggregation READ bestAggregation NOTIFY searchChanged)
    Q_PROPERTY(QVariantList searchSamples READ searchSamples NOTIFY searchChanged)

    // ---- extra plot series ----
    Q_PROPERTY(QVariantList globalIctAlpha READ globalIctAlpha NOTIFY seriesChanged)
    Q_PROPERTY(QVariantList roundPoints READ roundPoints NOTIFY roundPointsChanged)
    Q_PROPERTY(int roundPointsRound READ roundPointsRound NOTIFY roundPointsChanged)

    // Distances live in the table's fields.
    Q_PROPERTY(double baseDistance READ baseDistance WRITE setBaseDistance NOTIFY tableChanged)
    Q_PROPERTY(double distanceStep READ distanceStep WRITE setDistanceStep NOTIFY tableChanged)
    // Per round: {formula, own}; blank when absent.
    Q_PROPERTY(QVariantList roundDistances READ roundDistances NOTIFY tableChanged)
    // Off: own distances are kept but unused.
    Q_PROPERTY(bool manualRoundDistance READ manualRoundDistance WRITE setManualRoundDistance
                   NOTIFY optionsChanged)
    Q_PROPERTY(int regressionDegree READ regressionDegree WRITE setRegressionDegree
                   NOTIFY optionsChanged)

public:
    explicit CalibrationController(QObject *parent = nullptr);
    ~CalibrationController() override;

    ProbeStatus::Status status() const { return status_; }
    bool busy() const { return busy_ > 0; }
    QString activity() const { return activity_; }
    QString lastError() const { return lastError_; }

    QString folder() const { return folder_; }
    void setFolder(const QString &path);
    ProbeStatus::Status loadStatus() const { return loadStatus_; }
    QString loadSummary() const { return loadSummary_; }

    QVariantList rounds() const { return rounds_; }
    QVariantList sideLayers() const { return sideLayers_; }
    QVariantList roundEnabled() const { return roundEnabled_; }
    QVariantMap fields() const { return fields_; }
    int tableVersion() const { return tableVersion_; }

    QVariantList coefficients() const { return coefficients_; }
    QVariantList alphaRounds() const { return alphaRounds_; }
    QVariantList alphaFit() const { return alphaFit_; }
    QVariantList zflRounds() const { return zflRounds_; }
    QVariantList roundColors() const { return roundColors_; }
    double maxIct() const { return maxIct_; }
    bool hasRawIct() const { return hasRawIct_; }
    bool seriesFresh() const { return seriesVersion_ == tableVersion_; }

    QString aggregationText() const { return aggregationText_; }
    QString noiseText() const { return noiseText_; }

    bool searchRunning() const { return searchRunning_; }
    QString searchStage() const { return searchStage_; }
    int searchDone() const { return searchDone_; }
    int searchTotal() const { return searchTotal_; }
    QString searchSummary() const { return searchSummary_; }
    double bestDistance() const { return bestDistance_; }
    double bestAggregation() const { return bestAggregation_; }
    QVariantList searchSamples() const { return searchSamples_; }

    QVariantList globalIctAlpha() const { return globalIctAlpha_; }
    QVariantList roundPoints() const { return roundPoints_; }
    int roundPointsRound() const { return roundPointsRound_; }

    double baseDistance() const;
    void setBaseDistance(double distance);
    double distanceStep() const;
    void setDistanceStep(double step);
    QVariantList roundDistances() const;
    bool manualRoundDistance() const { return manualRoundDistance_; }
    void setManualRoundDistance(bool on);
    int regressionDegree() const { return degree_; }
    void setRegressionDegree(int degree);

    Q_INVOKABLE void connectTo(int domainId);

    // ---- the table ----
    Q_INVOKABLE void setPct(int round, int layer, const QString &value);
    Q_INVOKABLE void setIct(int round, int layer, int direction, const QString &value);
    Q_INVOKABLE void setSideLayer(int round, int layer);
    Q_INVOKABLE void setRoundEnabled(int round, bool enabled);
    Q_INVOKABLE void setField(const QString &name, const QString &value);
    // Blank returns the round to the formula.
    Q_INVOKABLE void setRoundDistance(int round, const QString &value);
    Q_INVOKABLE void clearTable(int round);
    Q_INVOKABLE void clearAllTables();

    // ---- Excel ----
    Q_INVOKABLE void loadExcel(int round, const QUrl &fileUrl);
    Q_INVOKABLE void loadAllExcel(const QUrl &folderUrl);
    Q_INVOKABLE void saveExcel(int round, const QUrl &fileUrl);

    // ---- the pipeline ----
    Q_INVOKABLE void computeAll();
    Q_INVOKABLE void calculateRound(int round);
    Q_INVOKABLE void aggregationForRound(int round);

    // Every enabled round scored at one distance.
    Q_INVOKABLE void aggregationAllRounds(bool useRange, double xLo, double xHi);

    Q_INVOKABLE void cleanNoise(int round);

    // Fill a round's ICT columns from crossings.
    Q_INVOKABLE void updateFromCapture(int round, const QVariantList &pct, const QVariantMap &nodes);

    // ---- the graphs ----
    Q_INVOKABLE void updateSeries();

    // One round's ZFL curve, ignoring its enabled flag.
    Q_INVOKABLE void fetchRoundPoints(int round);

    // ---- the distance searches ----
    // Ternary search over one round.
    Q_INVOKABLE void findMinForRound(int round, double distMin, double distMax);
    // Coarse sweep then refine, every enabled round.
    Q_INVOKABLE void findMinInWindow(bool useWindow, double xLo, double xHi);
    // 282 probes; hence an action, not service.
    Q_INVOKABLE void findDistanceForTarget(double target, bool useRange, double xLo, double xHi);

    // Unwind the search; partial table comes back.
    Q_INVOKABLE void cancelSearch();

    // Drop in-flight replies; the op still finishes.
    Q_INVOKABLE void stop();

signals:
    void statusChanged();
    void busyChanged();
    void lastErrorChanged();
    void errorRaised(const QString &message);
    void notice(const QString &message);

    void tableChanged();
    void seriesChanged();
    void resultChanged();
    void folderChanged();
    void optionsChanged();
    void searchChanged();
    void roundPointsChanged();

private:
    Q_INVOKABLE void applyLink(int status, const QString &message, quint64 generation);
    Q_INVOKABLE void applyCaliOp(const QString &op, int round, bool ok, const QString &tableJson,
                                 const QString &resultJson, const QString &message, quint64 token,
                                 quint64 generation);
    Q_INVOKABLE void applySeries(const QString &kind, bool ok, const QString &seriesJson,
                                 const QString &message, int version, quint64 token,
                                 quint64 generation);
    Q_INVOKABLE void applyXlsxRead(int round, bool ok, const QString &gridJson,
                                   const QString &message, const QString &name, quint64 token,
                                   quint64 generation);
    Q_INVOKABLE void applyXlsxWrite(bool ok, const QByteArray &data, const QString &message,
                                    const QString &path, quint64 token, quint64 generation);

    Q_INVOKABLE void applySearchAccepted(bool accepted, const QString &message, quint64 generation);
    Q_INVOKABLE void applySearchFeedback(int done, int total, const QString &stage,
                                         quint64 generation);
    Q_INVOKABLE void applySearchResult(bool ok, bool cancelled, const QString &tableJson,
                                       const QString &resultJson, const QString &message,
                                       quint64 generation);

    void startSearch(const QString &op, const QJsonObject &extra, int round);

    void setStatus(ProbeStatus::Status status);
    void setLastError(const QString &message);
    void beginCall(const QString &what);
    void endCall();
    void stopWorker();
    bool ready();

    void rebuildModel();

    // One round's rows, to avoid rebuilding eleven.
    void rebuildRound(int round);

    // round < 0 rebuilds everything.
    void bumpVersion(int round = -1);
    QString tableJson() const;
    QString paramsJson(int round, const QJsonObject &extra = {}) const;

    bool sendCaliOp(const QString &op, int round, const QJsonObject &extra, const QString &activity);
    bool sendSeries(const QString &kind, const QJsonObject &extra);

    // Noise bands are removed one at a time.
    void sendNextBand();

    void setCell(int round, int row, int col, const QString &text);
    QString cell(int round, int row, int col) const;
    double numberField(const QString &name, double fallback) const;
    bool roundHasRawIct(int round) const;
    void ensureRound(int round);

    ProbeStatus::Status status_ = ProbeStatus::Unknown;
    int busy_ = 0;
    QString activity_;
    QString lastError_;

    QString folder_;
    ProbeStatus::Status loadStatus_ = ProbeStatus::Unknown;
    QString loadSummary_;

    // The wire format, single source of truth.
    QJsonObject table_;

    QVariantList rounds_;
    QVariantList sideLayers_;
    QVariantList roundEnabled_;
    QVariantMap fields_;
    int tableVersion_ = 0;

    QVariantList coefficients_;
    QVariantList alphaRounds_;
    QVariantList alphaFit_;
    QVariantList zflRounds_;
    QVariantList roundColors_;
    double maxIct_ = 0;
    bool hasRawIct_ = false;
    int seriesVersion_ = -1;
    int seriesPending_ = 0;

    QString aggregationText_;
    QString noiseText_;

    // Noise-band queue; removals must not overlap.
    QList<QJsonObject> pendingBands_;
    int pendingBandRound_ = 0;
    int bandsFound_ = 0;
    int bandsRemoved_ = 0;

    bool manualRoundDistance_ = false;
    int degree_ = 4;

    // Which rounds a load-all is still awaiting.
    int pendingLoads_ = 0;
    int loadedOk_ = 0;
    QStringList loadFailures_;

    bool searchRunning_ = false;
    QString searchStage_;
    QString searchSummary_;
    int searchDone_ = 0;
    int searchTotal_ = 0;
    double bestDistance_ = 0;
    double bestAggregation_ = 0;
    QVariantList searchSamples_;

    QVariantList globalIctAlpha_;
    QVariantList roundPoints_;
    int roundPointsRound_ = 0;

    QSet<quint64> liveTokens_;
    quint64 nextToken_ = 0;

    struct Impl;
    std::unique_ptr<Impl> d_;
};
