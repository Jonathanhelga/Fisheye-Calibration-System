#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

#include <memory>

#include "ProbeStatus.h"

class QQmlImageProviderBase;

// The rig's camera: one-shot captures into named slots, and the live preview.
//
// SLOTS, not a single current image. A calibration measurement is a PAIR -- the
// same scene under the positive and the negative pattern -- and every detect op
// that matters takes both. Holding one "last frame" would mean the positive shot
// is gone the moment the negative is taken, which is precisely when it is needed.
// The frames themselves live in ImageStore so ComputeController can send the
// original compressed bytes without this class knowing anything about analysis.
//
// Slot names: "single" (the plain Capture button), "positive", "negative",
// "live" (the newest streamed frame).
//
// A CAPTURE IS A MEASUREMENT AND A PREVIEW FRAME IS NOT. Captures go through the
// /camera/capture SERVICE, which answers with a frame grabbed after the request;
// the live preview is a BEST_EFFORT topic subscription whose dropped frames cost
// nothing. Snapshot copies a preview frame into the "single" slot and is labelled
// as such -- it is not a substitute for a capture, because the device holds
// frames from before the pattern on the glass changed.
class CameraController : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(ProbeStatus::Status status READ status NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString lastError READ lastError NOTIFY changed)

    // image:// URLs, one per slot. Empty until that slot has a frame.
    Q_PROPERTY(QString frameUrl READ frameUrl NOTIFY changed)
    Q_PROPERTY(QString positiveUrl READ positiveUrl NOTIFY changed)
    Q_PROPERTY(QString negativeUrl READ negativeUrl NOTIFY changed)
    Q_PROPERTY(QString liveUrl READ liveUrl NOTIFY liveChanged)

    Q_PROPERTY(QString frameLabel READ frameLabel NOTIFY changed)
    Q_PROPERTY(QString positiveTime READ positiveTime NOTIFY changed)
    Q_PROPERTY(QString negativeTime READ negativeTime NOTIFY changed)
    Q_PROPERTY(bool hasPositive READ hasPositive NOTIFY changed)
    Q_PROPERTY(bool hasNegative READ hasNegative NOTIFY changed)
    Q_PROPERTY(bool hasPair READ hasPair NOTIFY changed)

    // Which slot a capture is currently filling, "" when idle. The panel shows
    // "Capturing positive shot..." off this.
    Q_PROPERTY(QString pendingSlot READ pendingSlot NOTIFY changed)
    Q_PROPERTY(bool pairing READ pairing NOTIFY changed)

    Q_PROPERTY(bool streaming READ streaming NOTIFY liveChanged)
    Q_PROPERTY(qreal fps READ fps NOTIFY liveChanged)
    Q_PROPERTY(bool receiving READ receiving NOTIFY liveChanged)

    // The camera's field of view in degrees. Held here rather than in the panel
    // because it is a property of the rig that the calibration result needs, not
    // a control's local state.
    Q_PROPERTY(int fov READ fov WRITE setFov NOTIFY fovChanged)

public:
    explicit CameraController(QObject *parent = nullptr);
    ~CameraController() override;

    static QQmlImageProviderBase *createImageProvider();

    ProbeStatus::Status status() const { return status_; }
    bool busy() const { return busy_; }
    QString lastError() const { return lastError_; }

    QString frameUrl() const;
    QString positiveUrl() const;
    QString negativeUrl() const;
    QString liveUrl() const;

    QString frameLabel() const { return frameLabel_; }
    QString positiveTime() const { return positiveTime_; }
    QString negativeTime() const { return negativeTime_; }
    bool hasPositive() const;
    bool hasNegative() const;
    bool hasPair() const { return hasPositive() && hasNegative(); }

    QString pendingSlot() const { return pendingSlot_; }
    bool pairing() const { return pairStage_ != 0; }

    bool streaming() const { return streaming_; }
    qreal fps() const { return fps_; }
    bool receiving() const { return streaming_ && liveRevision_ > 0; }

    int fov() const { return fov_; }
    void setFov(int degrees);

    Q_INVOKABLE void connectTo(int domainId);

    // slot: "" for the plain Capture button, or "positive" / "negative".
    Q_INVOKABLE void capture(const QString &slot = QString());

    // Positive then negative, swapping the prepared polarity on the glass in
    // between. Needs MonitorController and a prepared pattern; says so rather
    // than shooting the same picture twice if either is missing.
    Q_INVOKABLE void capturePair();

    // Read a picture off disk into a slot, so an existing capture can be
    // re-analysed without the rig.
    Q_INVOKABLE bool openImage(const QUrl &fileUrl, const QString &slot = QString());

    Q_INVOKABLE void startStream();
    Q_INVOKABLE void stopStream();
    Q_INVOKABLE void snapshot();

    Q_INVOKABLE void clearSlot(const QString &slot);

signals:
    void changed();
    void liveChanged();
    void fovChanged();
    void errorRaised(const QString &message);
    void notice(const QString &message);

    void captured(const QString &slot, int width, int height);
    void pairComplete();
    void pairFailed(const QString &reason);

private:
    Q_INVOKABLE void applyLink(int status, const QString &message, quint64 generation);
    Q_INVOKABLE void applyCapture(const QString &slot, bool ok, int width, int height,
                                  const QString &message, quint64 token, quint64 generation);
    Q_INVOKABLE void applyLiveFrame(int width, int height, quint64 generation);

    void stopWorker();

    // Sets lastError AND announces it. Both matter: the panel shows the text
    // inline, and the window raises a toast, so an error written only to the
    // member would be visible in one place and invisible in the other.
    void setError(const QString &message);

    void abortPair(const QString &reason);
    void advancePair(const QString &justCaptured);
    void hookMonitor();

    ProbeStatus::Status status_ = ProbeStatus::Unknown;
    bool busy_ = false;
    QString lastError_;
    QString frameLabel_;
    QString positiveTime_;
    QString negativeTime_;
    QString pendingSlot_;

    int singleRevision_ = 0;
    int positiveRevision_ = 0;
    int negativeRevision_ = 0;
    int liveRevision_ = 0;

    bool streaming_ = false;
    qreal fps_ = 0;
    qint64 lastFrameMs_ = 0;

    int fov_ = 180;

    // 0 idle, 1 showing positive, 2 capturing positive,
    // 3 showing negative, 4 capturing negative.
    int pairStage_ = 0;
    bool monitorHooked_ = false;

    quint64 captureToken_ = 0;

    struct Impl;
    std::unique_ptr<Impl> d_;
};
