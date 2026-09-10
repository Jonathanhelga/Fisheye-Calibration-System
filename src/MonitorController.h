#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <memory>

#include "ProbeStatus.h"

// The five calibration screens: brightness, content, mapping.
class MonitorController : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(ProbeStatus::Status status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

    // One entry per screen the server sees.
    Q_PROPERTY(QVariantList screens READ screens NOTIFY screensChanged)
    Q_PROPERTY(bool mappingComplete READ mappingComplete NOTIFY screensChanged)
    Q_PROPERTY(QString screensSummary READ screensSummary NOTIFY screensChanged)

    // True once both polarities are prepared.
    Q_PROPERTY(bool preparedReady READ preparedReady NOTIFY preparedChanged)

public:
    explicit MonitorController(QObject *parent = nullptr);
    ~MonitorController() override;

    // C++ accessor: CameraController swaps polarity mid-pair.
    static MonitorController *instance();

    ProbeStatus::Status status() const { return status_; }
    bool busy() const { return busy_ > 0; }
    QString lastError() const { return lastError_; }
    QVariantList screens() const { return screens_; }
    bool mappingComplete() const { return mappingComplete_; }
    QString screensSummary() const { return screensSummary_; }
    bool preparedReady() const { return preparedReady_; }

    Q_INVOKABLE void connectTo(int domainId);
    Q_INVOKABLE void reconnect();

    Q_INVOKABLE void setBrightness(const QString &direction, double brightness);
    Q_INVOKABLE void readBrightness(const QString &direction);

    // Push a picture. Accepts a URL or path.
    Q_INVOKABLE void showImage(const QString &direction, const QUrl &fileUrl);
    Q_INVOKABLE void showImagePath(const QString &direction, const QString &path);

    Q_INVOKABLE void closePattern(const QString &direction);

    Q_INVOKABLE void describeScreens();
    Q_INVOKABLE void setDisplayDirection(const QString &top, const QString &north,
                                         const QString &west, const QString &south,
                                         const QString &east);
    Q_INVOKABLE void showDisplayNumbers();

    // Render both polarities and keep them.
    Q_INVOKABLE void preparePatterns(const QString &specConcentric, const QString &specStripeline,
                                     const QVariantList &positiveRgb,
                                     const QVariantList &negativeRgb);
    Q_INVOKABLE void showPrepared(const QString &polarity);

signals:
    void statusChanged();
    void busyChanged();
    void lastErrorChanged();
    void errorRaised(const QString &message);
    void notice(const QString &message);

    void screensChanged();
    void preparedChanged();

    void brightnessRead(const QString &direction, double brightness);
    void imageShown(const QString &direction);
    void patternClosed(const QString &direction);
    void directionsApplied();

    // Emitted once polarity is actually on the glass.
    void preparedShown(const QString &polarity, const QStringList &shown);
    void preparedFailed(const QString &polarity, const QString &message);

private:
    Q_INVOKABLE void applyLink(int status, const QString &message, quint64 generation);
    Q_INVOKABLE void applySimple(int kind, const QString &direction, bool ok,
                                 const QString &message, quint64 generation);
    Q_INVOKABLE void applyBrightness(const QString &direction, bool ok, double brightness,
                                     const QString &message, quint64 generation);
    Q_INVOKABLE void applyScreens(const QVariantList &screens, bool complete,
                                  const QString &message, quint64 generation);
    Q_INVOKABLE void applyPrepared(const QString &polarity, bool ok, const QStringList &shown,
                                   const QString &message, quint64 generation);
    Q_INVOKABLE void applyPrepareDone(bool ok, const QString &directory, const QString &message,
                                      quint64 generation);

    void setStatus(ProbeStatus::Status status);
    void setLastError(const QString &message);
    void beginCall();
    void endCall();
    void stopWorker();
    bool ready();

    ProbeStatus::Status status_ = ProbeStatus::Unknown;
    int busy_ = 0;
    QString lastError_;
    QVariantList screens_;
    QString screensSummary_;
    bool mappingComplete_ = false;
    bool preparedReady_ = false;
    int domainId_ = 0;

    struct Impl;
    std::unique_ptr<Impl> d_;
};
