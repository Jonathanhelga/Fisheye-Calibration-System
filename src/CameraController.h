#pragma once

#include <QHash>
#include <QObject>
#include <QString>
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
    Q_PROPERTY(QString lastError READ lastError NOTIFY changed)

public:
    explicit CameraController(QObject *parent = nullptr);
    ~CameraController() override;

    static QQmlImageProviderBase *createImageProvider();

    ProbeStatus::Status status() const { return status_; }
    bool busy() const { return busy_; }
    QVariantMap frameUrls() const { return frameUrls_; }
    QVariantMap frameLabels() const { return frameLabels_; }
    QString lastError() const { return lastError_; }

    Q_INVOKABLE void connectTo(int domainId);
    Q_INVOKABLE void capture(const QString &slot = QString());

signals:
    void changed();
    void captured(const QString &slot, bool ok, const QString &message);

private:
    Q_INVOKABLE void applyLink(int status, const QString &message, quint64 generation);
    Q_INVOKABLE void applyCapture(const QString &slot, bool ok, int width, int height,
                                  const QString &message, quint64 token, quint64 generation);

    void stopWorker();

    ProbeStatus::Status status_ = ProbeStatus::Unknown;
    bool busy_ = false;
    QHash<QString, int> revisions_;
    QVariantMap frameUrls_;
    QVariantMap frameLabels_;
    QString lastError_;
    quint64 captureToken_ = 0;

    struct Impl;
    std::unique_ptr<Impl> d_;
};
