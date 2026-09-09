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

    // The size of the picture that actually arrived, as decoded -- NOT what the
    // rig said it sent. frameSizes prefers the rig's own report, and the two do
    // differ: the capture reply carries both, and the mismatch is reported to the
    // operator as "(the rig reported %1x%2)".
    //
    // Anything working in image coordinates must use this one. The detect ops
    // decode the same bytes and measure those pixels, so a centre derived from a
    // reported size that disagrees would be off by exactly the difference --
    // silently, because both numbers look like a frame size.
    Q_PROPERTY(QVariantMap imageSizes READ imageSizes NOTIFY changed)
    Q_PROPERTY(QVariantMap foldUrls READ foldUrls NOTIFY changed)
    Q_PROPERTY(QVariantMap foldScores READ foldScores NOTIFY changed)
    Q_PROPERTY(QString lastError READ lastError NOTIFY changed)

    // The lens's field of view, in degrees. Not a capture property and nothing
    // here reads it -- it lives on this controller because it describes the
    // camera, and it is written into camera_parameters.json beside the fitted
    // coefficients by the Cali Result window's Parameter tab.
    //
    // Restored after the 2026-09-08 merge, which took the camera path from
    // v2.1_2026_New-UI-CPP-ROS wholesale. That branch never had this property,
    // so CaliResultParameterPanel's binding resolved to undefined and the tab
    // showed "undefined" as the FOV -- then wrote it into the saved parameters.
    // qmllint caught it; at run time it would have been a plausible-looking file
    // with one wrong field.
    Q_PROPERTY(int fov READ fov WRITE setFov NOTIFY changed)

    // The live preview, restored 2026-09-08 after the merge from
    // v2.1_2026_New-UI-CPP-ROS, which has no stream -- LiveCameraPanel's Start,
    // Stop and Snapshot were left emitting into nothing.
    //
    // `receiving` is not the same question as `streaming`: streaming says the
    // operator asked for frames, receiving says some have arrived. A rig that
    // publishes nothing leaves the first true and the second false, which is the
    // distinction the panel's status line is made of.
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

    // Read a picture off disk into a slot, so a capture taken earlier -- or on
    // another machine -- can be analysed without the rig.
    //
    // This is the ONLY way to get an image into the app without a camera, and it
    // is what makes offline work possible: the detect ops take the bytes in the
    // request, so a file loaded here reaches /compute/detect exactly as a fresh
    // capture would. A compute node needs no hardware, so the whole measurement
    // path runs against a server on this machine.
    //
    // Restored 2026-09-09. It existed before the merge from
    // v2.1_2026_New-UI-CPP-ROS, which removed the button and the backend
    // together -- consistently, so nothing dangled and nothing reported it.
    Q_INVOKABLE bool openImage(const QUrl &fileUrl, const QString &slot = QString());

    Q_INVOKABLE void startStream();
    Q_INVOKABLE void stopStream();

    // Keep the newest preview frame as the "single" capture. Labelled as a
    // preview on purpose: it came off a BEST_EFFORT topic and may predate the
    // pattern now on the glass -- fine for aiming, not a measurement.
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
