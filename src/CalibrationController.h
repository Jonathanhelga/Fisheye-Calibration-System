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

// The Cali Result window's data: eleven round tables, the form's scalar fields,
// and everything derived from them.
//
// THE CLIENT COMPUTES NOTHING. Not the derived columns, not the aggregation, not
// the regression, not the six polynomial coefficients -- CaliSeries.srv says why
// at length, and the short version is that a boundary with an arithmetic
// exception in it is not a boundary. What this class holds is the table (the
// operator's working copy, which travels whole with every op) and a CACHE of
// answers the server gave it. There is no code here that could produce those
// numbers, only code that remembers the ones it was given.
//
// The cache is keyed to tableVersion(), bumped on every edit and every op. A
// redraw, a tab change or a resize reads the cache; the wire is touched only when
// the table actually changed.
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

    // The display model: 11 rounds, each a list of row objects shaped exactly
    // like CaliResultDataPanel.emptyRow().
    Q_PROPERTY(QVariantList rounds READ rounds NOTIFY tableChanged)
    Q_PROPERTY(QVariantList sideLayers READ sideLayers NOTIFY tableChanged)
    Q_PROPERTY(QVariantList roundEnabled READ roundEnabled NOTIFY tableChanged)
    Q_PROPERTY(QVariantMap fields READ fields NOTIFY tableChanged)
    Q_PROPERTY(int tableVersion READ tableVersion NOTIFY tableChanged)

    // The parameter box: six coefficient slots as text, straight from
    // alpha_polynomial. Never fitted here.
    Q_PROPERTY(QVariantList coefficients READ coefficients NOTIFY seriesChanged)

    // Plot series, as lists of {x, y}. Empty until updateSeries() has answered.
    Q_PROPERTY(QVariantList alphaRounds READ alphaRounds NOTIFY seriesChanged)
    Q_PROPERTY(QVariantList alphaFit READ alphaFit NOTIFY seriesChanged)
    Q_PROPERTY(QVariantList zflRounds READ zflRounds NOTIFY seriesChanged)
    Q_PROPERTY(QVariantList roundColors READ roundColors NOTIFY seriesChanged)
    Q_PROPERTY(double maxIct READ maxIct NOTIFY seriesChanged)
    Q_PROPERTY(bool hasRawIct READ hasRawIct NOTIFY seriesChanged)
    Q_PROPERTY(bool seriesFresh READ seriesFresh NOTIFY seriesChanged)

    // Scalar answers the ops hand back, shown next to the buttons that asked.
    Q_PROPERTY(QString aggregationText READ aggregationText NOTIFY resultChanged)
    Q_PROPERTY(QString noiseText READ noiseText NOTIFY resultChanged)

    // ---- the distance searches (CaliJob) -----------------------------------
    // These run for minutes and CAN be cancelled, unlike every service above.
    Q_PROPERTY(bool searchRunning READ searchRunning NOTIFY searchChanged)
    Q_PROPERTY(QString searchStage READ searchStage NOTIFY searchChanged)
    Q_PROPERTY(int searchDone READ searchDone NOTIFY searchChanged)
    Q_PROPERTY(int searchTotal READ searchTotal NOTIFY searchChanged)
    Q_PROPERTY(QString searchSummary READ searchSummary NOTIFY searchChanged)
    Q_PROPERTY(double bestDistance READ bestDistance NOTIFY searchChanged)
    Q_PROPERTY(double bestAggregation READ bestAggregation NOTIFY searchChanged)
    Q_PROPERTY(QVariantList searchSamples READ searchSamples NOTIFY searchChanged)

    // ---- extra plot series --------------------------------------------------
    Q_PROPERTY(QVariantList globalIctAlpha READ globalIctAlpha NOTIFY seriesChanged)
    Q_PROPERTY(QVariantList roundPoints READ roundPoints NOTIFY roundPointsChanged)
    Q_PROPERTY(int roundPointsRound READ roundPointsRound NOTIFY roundPointsChanged)

    Q_PROPERTY(bool singleDistance READ singleDistance WRITE setSingleDistance NOTIFY optionsChanged)
    Q_PROPERTY(double baseDistance READ baseDistance WRITE setBaseDistance NOTIFY optionsChanged)
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

    bool singleDistance() const { return singleDistance_; }
    void setSingleDistance(bool on);
    double baseDistance() const { return baseDistance_; }
    void setBaseDistance(double distance);
    int regressionDegree() const { return degree_; }
    void setRegressionDegree(int degree);

    Q_INVOKABLE void connectTo(int domainId);

    // ---- the table ---------------------------------------------------------
    Q_INVOKABLE void setPct(int round, int layer, const QString &value);
    Q_INVOKABLE void setIct(int round, int layer, int direction, const QString &value);
    Q_INVOKABLE void setSideLayer(int round, int layer);
    Q_INVOKABLE void setRoundEnabled(int round, bool enabled);
    Q_INVOKABLE void setField(const QString &name, const QString &value);
    Q_INVOKABLE void clearTable(int round);
    Q_INVOKABLE void clearAllTables();

    // ---- Excel -------------------------------------------------------------
    Q_INVOKABLE void loadExcel(int round, const QUrl &fileUrl);
    Q_INVOKABLE void loadAllExcel(const QUrl &folderUrl);
    Q_INVOKABLE void saveExcel(int round, const QUrl &fileUrl);

    // ---- the pipeline ------------------------------------------------------
    Q_INVOKABLE void computeAll();
    Q_INVOKABLE void calculateRound(int round);
    Q_INVOKABLE void aggregationForRound(int round);

    // Every ENABLED round scored together at one distance -- the number you judge
    // a whole run on, as opposed to how tight a single round is with itself.
    Q_INVOKABLE void aggregationAllRounds(bool useRange, double xLo, double xHi);

    Q_INVOKABLE void cleanNoise(int round);

    // Fill a round's ICT columns from the crossings ComputeController just found.
    Q_INVOKABLE void updateFromCapture(int round, const QVariantList &pct, const QVariantMap &nodes);

    // ---- the graphs --------------------------------------------------------
    Q_INVOKABLE void updateSeries();

    // One round's ZFL curve on its own, IGNORING its enabled flag -- which is
    // exactly what you need when deciding whether to disable it.
    Q_INVOKABLE void fetchRoundPoints(int round);

    // ---- the distance searches ---------------------------------------------
    // Ternary search over one round.
    Q_INVOKABLE void findMinForRound(int round, double distMin, double distMax);
    // Coarse sweep then refine, over every enabled round.
    Q_INVOKABLE void findMinInWindow(bool useWindow, double xLo, double xHi);
    // 282 probes, each recomputing all eleven rounds. The reason this is an
    // action rather than a service.
    Q_INVOKABLE void findDistanceForTarget(double target, bool useRange, double xLo, double xHi);

    // Asks the rig to unwind the search. The table comes back at whatever the
    // last probe wrote -- a cancelled search produced a partial answer, it did
    // not fail.
    Q_INVOKABLE void cancelSearch();

    // Drops whatever is in flight so the window unblocks. The op itself is a
    // plain service with no cancel, so it finishes on the rig regardless -- say
    // so rather than pretend it stopped.
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

    // One round's display rows. Typing in a cell must not rebuild all eleven
    // tables: that is ~24k QVariant constructions per keystroke in a table the
    // operator types into all day.
    void rebuildRound(int round);

    // round < 0 rebuilds everything, for the ops that replace the whole table.
    void bumpVersion(int round = -1);
    QString tableJson() const;
    QString paramsJson(int round, const QJsonObject &extra = {}) const;

    bool sendCaliOp(const QString &op, int round, const QJsonObject &extra, const QString &activity);
    bool sendSeries(const QString &kind, const QJsonObject &extra);

    // Noise bands are removed strictly one at a time -- see applyCaliOp.
    void sendNextBand();

    void setCell(int round, int row, int col, const QString &text);
    QString cell(int round, int row, int col) const;
    void ensureRound(int round);

    ProbeStatus::Status status_ = ProbeStatus::Unknown;
    int busy_ = 0;
    QString activity_;
    QString lastError_;

    QString folder_;
    ProbeStatus::Status loadStatus_ = ProbeStatus::Unknown;
    QString loadSummary_;

    // The wire format itself, kept as the single source of truth so the table
    // that travels is exactly the table that is displayed.
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

    // The noise-band queue. Each removal returns the whole table, so they cannot
    // overlap without silently undoing one another.
    QList<QJsonObject> pendingBands_;
    int pendingBandRound_ = 0;
    int bandsFound_ = 0;
    int bandsRemoved_ = 0;

    bool singleDistance_ = false;
    double baseDistance_ = 250.0;
    int degree_ = 4;

    // Pending load-all bookkeeping: which rounds are still expected, so the
    // summary can say "8 of 10 loaded" instead of only reporting the last one.
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
