#pragma once

#include <QColor>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <memory>

#include "ProbeStatus.h"

class QQmlImageProviderBase;

class PatternController : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(ProbeStatus::Status status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QVariantMap previewUrls READ previewUrls NOTIFY previewChanged)

public:
    explicit PatternController(QObject *parent = nullptr);
    ~PatternController() override;

    static QQmlImageProviderBase *createImageProvider();

    ProbeStatus::Status status() const { return status_; }
    bool busy() const { return busy_; }
    QString lastError() const { return lastError_; }
    QVariantMap previewUrls() const { return previewUrls_; }

    Q_INVOKABLE void connectTo(int domainId);
    Q_INVOKABLE void renderPreview(const QString &patternType, const QString &specJson, int width,
                                   int height);
    Q_INVOKABLE bool savePreview(const QString &patternType, const QUrl &fileUrl);
    Q_INVOKABLE void showOnMonitor(const QString &direction, const QString &specJson);
    Q_INVOKABLE void showImageOnMonitor(const QString &direction, const QString &imagePath);
    Q_INVOKABLE void preparePatterns(const QString &specConcentric, const QString &specStripeline,
                                     const QColor &positive, const QColor &negative);
    Q_INVOKABLE void showPrepared(const QString &polarity);
    Q_INVOKABLE void refreshDirection(const QString &direction);
    Q_INVOKABLE void closeMonitor(const QString &direction);
    Q_INVOKABLE void setMonitorBrightness(const QString &direction, double brightness);
    Q_INVOKABLE void readMonitorBrightness(const QString &direction);
    Q_INVOKABLE void showDisplayNumbers();
    Q_INVOKABLE void applyDisplayDirection(int top, int north, int west, int south, int east);

signals:
    void statusChanged();
    void busyChanged();
    void lastErrorChanged();
    void errorRaised(const QString &message);
    void previewChanged();
    void patternShown(const QString &direction, int width, int height, const QString &imagePath);
    void monitorClosed(const QString &direction);
    void brightnessApplied(const QString &direction, double brightness);
    void brightnessRead(const QString &direction, double brightness);
    void displaySetupReplied(bool ok, const QString &message);
    void patternsPrepared(bool ok, const QStringList &prepared, const QString &directory,
                          const QString &message);
    void preparedShown(bool ok, const QString &polarity, const QStringList &shown,
                       const QString &message);

private:
    Q_INVOKABLE void applyLink(int status, const QString &message, quint64 generation);
    Q_INVOKABLE void applyPreview(const QString &patternType, bool ok, const QString &message,
                                  quint64 token, quint64 generation);
    Q_INVOKABLE void applyShow(const QString &direction, bool ok, const QString &message, int width,
                               int height, const QString &imagePath, quint64 token,
                               quint64 generation);
    Q_INVOKABLE void applyClose(const QString &direction, bool ok, const QString &message,
                                quint64 token, quint64 generation);
    Q_INVOKABLE void applyBrightness(const QString &direction, double brightness, bool ok,
                                     const QString &message, quint64 token, quint64 generation);
    Q_INVOKABLE void applyBrightnessRead(const QString &direction, double brightness, bool ok,
                                         const QString &message, quint64 generation);
    Q_INVOKABLE void applyDisplaySetup(bool ok, const QString &message, quint64 token,
                                       quint64 generation);
    Q_INVOKABLE void applyPrepare(bool ok, const QStringList &prepared, const QString &directory,
                                  const QString &message, quint64 token, quint64 generation);
    Q_INVOKABLE void applyShowPrepared(const QString &polarity, bool ok, const QStringList &shown,
                                       const QString &message, quint64 token, quint64 generation);

    void setStatus(ProbeStatus::Status status);
    void setLastError(const QString &message);
    void finishRender(const QString &patternType);
    void updateBusy();
    void stopWorker();

    ProbeStatus::Status status_ = ProbeStatus::Unknown;
    bool busy_ = false;
    QString lastError_;
    QVariantMap previewUrls_;
    QHash<QString, int> revisions_;
    QHash<QString, quint64> renderTokens_;
    QHash<QString, QString> lastSpecs_;
    QHash<QString, quint64> showTokens_;
    QHash<QString, quint64> closeTokens_;
    QHash<QString, quint64> brightnessTokens_;
    quint64 displaySetupToken_ = 0;
    quint64 prepareToken_ = 0;
    quint64 showPreparedToken_ = 0;
    QSet<QString> pending_;
    int domainId_ = 0;

    struct Impl;
    std::unique_ptr<Impl> d_;
};
