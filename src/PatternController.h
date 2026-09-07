#pragma once

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
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
    Q_INVOKABLE void refreshDirection(const QString &direction);
    Q_INVOKABLE void closeMonitor(const QString &direction);
    Q_INVOKABLE void setMonitorBrightness(const QString &direction, double brightness);

signals:
    void statusChanged();
    void busyChanged();
    void lastErrorChanged();
    void errorRaised(const QString &message);
    void previewChanged();
    void patternShown(const QString &direction, int width, int height);
    void monitorClosed(const QString &direction);
    void brightnessApplied(const QString &direction, double brightness);

private:
    Q_INVOKABLE void applyLink(int status, const QString &message, quint64 generation);
    Q_INVOKABLE void applyPreview(const QString &patternType, bool ok, const QString &message,
                                  quint64 token, quint64 generation);
    Q_INVOKABLE void applyShow(const QString &direction, bool ok, const QString &message, int width,
                               int height, quint64 token, quint64 generation);
    Q_INVOKABLE void applyClose(const QString &direction, bool ok, const QString &message,
                                quint64 token, quint64 generation);
    Q_INVOKABLE void applyBrightness(const QString &direction, double brightness, bool ok,
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
    QSet<QString> pending_;
    int domainId_ = 0;

    struct Impl;
    std::unique_ptr<Impl> d_;
};
