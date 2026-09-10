#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <memory>

#include "ProbeStatus.h"

class QQmlImageProviderBase;

class CameraController : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(ProbeStatus::Status status READ status NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QVariantMap frameUrls READ frameUrls NOTIFY changed)
    Q_PROPERTY(QVariantMap frameLabels READ frameLabels NOTIFY changed)
    Q_PROPERTY(QVariantMap frameSizes READ frameSizes NOTIFY changed)

    // The decoded size, not the rig's report.
    Q_PROPERTY(QVariantMap imageSizes READ imageSizes NOTIFY changed)
    Q_PROPERTY(QVariantMap foldUrls READ foldUrls NOTIFY changed)
    Q_PROPERTY(QVariantMap foldScores READ foldScores NOTIFY changed)
    Q_PROPERTY(QString lastError READ lastError NOTIFY changed)

    // Lens FOV in degrees. Saved to camera_parameters.json.
    Q_PROPERTY(int fov READ fov WRITE setFov NOTIFY changed)

    // Live preview state. receiving is not streaming.
    Q_PROPERTY(QString liveUrl READ liveUrl NOTIFY changed)
    Q_PROPERTY(bool streaming READ streaming NOTIFY changed)
    Q_PROPERTY(bool receiving READ receiving NOTIFY changed)
    Q_PROPERTY(qreal fps READ fps NOTIFY changed)

public:
    explicit CameraController(QObject *parent = nullptr);
    ~CameraController() override;

    static QQmlImageProviderBase *createImageProvider();

    ProbeStatus::Status status() const { return status_; }
    bool busy() const { return busy_; }
    QVariantMap frameUrls() const { return frameUrls_; }
    QVariantMap frameLabels() const { return frameLabels_; }
    QVariantMap frameSizes() const { return frameSizes_; }
    QVariantMap imageSizes() const { return imageSizes_; }
    QVariantMap foldUrls() const { return foldUrls_; }
    QVariantMap foldScores() const { return foldScores_; }
    QString lastError() const { return lastError_; }
    int fov() const { return fov_; }
    void setFov(int degrees);

    QString liveUrl() const;
    bool streaming() const { return streaming_; }
    bool receiving() const { return streaming_ && liveRevision_ > 0; }
    qreal fps() const { return fps_; }

    Q_INVOKABLE void connectTo(int domainId);
    Q_INVOKABLE void capture(const QString &slot = QString());
    Q_INVOKABLE void foldCheck(const QString &slot, int cx, int cy, int radius, int gain = 4);

    // Read a picture off disk into a slot.
    Q_INVOKABLE bool openImage(const QUrl &fileUrl, const QString &slot = QString());

    Q_INVOKABLE void startStream();
    Q_INVOKABLE void stopStream();

    // Keep the newest preview frame. Aiming only.
    Q_INVOKABLE void snapshot();

signals:
    void changed();
    void captured(const QString &slot, bool ok, const QString &message);

private:
    Q_INVOKABLE void applyLink(int status, const QString &message, quint64 generation);
    Q_INVOKABLE void applyCapture(const QString &slot, bool ok, int width, int height,
                                  int frameWidth, int frameHeight, const QString &message,
                                  quint64 token, quint64 generation);
    Q_INVOKABLE void applyLiveFrame(int width, int height, quint64 generation);

    void stopWorker();

    ProbeStatus::Status status_ = ProbeStatus::Unknown;
    bool busy_ = false;
    QHash<QString, int> revisions_;
    QVariantMap frameUrls_;
    QVariantMap frameLabels_;
    QVariantMap frameSizes_;
    QVariantMap imageSizes_;
    QVariantMap foldUrls_;
    QVariantMap foldScores_;
    QString lastError_;
    int fov_ = 180;
    bool streaming_ = false;
    int liveRevision_ = 0;
    qreal fps_ = 0;
    qint64 lastFrameMs_ = 0;
    quint64 captureToken_ = 0;

    struct Impl;
    std::unique_ptr<Impl> d_;
};
