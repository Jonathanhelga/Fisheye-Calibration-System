#pragma once

#include <QObject>
#include <QString>
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
    Q_PROPERTY(QString frameUrl READ frameUrl NOTIFY changed)
    Q_PROPERTY(QString frameLabel READ frameLabel NOTIFY changed)
    Q_PROPERTY(QString lastError READ lastError NOTIFY changed)

public:
    explicit CameraController(QObject *parent = nullptr);
    ~CameraController() override;

    static QQmlImageProviderBase *createImageProvider();

    ProbeStatus::Status status() const { return status_; }
    bool busy() const { return busy_; }
    QString frameUrl() const;
    QString frameLabel() const { return frameLabel_; }
    QString lastError() const { return lastError_; }

    Q_INVOKABLE void connectTo(int domainId);
    Q_INVOKABLE void capture();

signals:
    void changed();

private:
    Q_INVOKABLE void applyLink(int status, const QString &message, quint64 generation);
    Q_INVOKABLE void applyCapture(bool ok, int width, int height, const QString &message,
                                  quint64 token, quint64 generation);

    void stopWorker();

    ProbeStatus::Status status_ = ProbeStatus::Unknown;
    bool busy_ = false;
    int revision_ = 0;
    QString frameLabel_;
    QString lastError_;
    quint64 captureToken_ = 0;

    struct Impl;
    std::unique_ptr<Impl> d_;
};
