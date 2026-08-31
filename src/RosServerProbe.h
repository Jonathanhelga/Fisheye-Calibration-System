#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

#include "ProbeStatus.h"

#include <memory>

class RosServerProbe : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(ProbeStatus::Status axisStatus READ axisStatus NOTIFY statusesChanged)
    Q_PROPERTY(ProbeStatus::Status monitorStatus READ monitorStatus NOTIFY statusesChanged)
    Q_PROPERTY(ProbeStatus::Status cameraStatus READ cameraStatus NOTIFY statusesChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY statusesChanged)

public:
    enum Service { Axis = 0, Monitor = 1, Camera = 2 };
    Q_ENUM(Service)

    explicit RosServerProbe(QObject *parent = nullptr);
    ~RosServerProbe() override;

    ProbeStatus::Status axisStatus() const { return status_[Axis]; }
    ProbeStatus::Status monitorStatus() const { return status_[Monitor]; }
    ProbeStatus::Status cameraStatus() const { return status_[Camera]; }
    QString lastError() const { return lastError_; }

    Q_INVOKABLE void probeAll(int domainId, const QString &axisNamespace,
                              const QString &monitorNamespace, const QString &cameraTopic);

    Q_INVOKABLE void resetAll();

signals:
    void statusesChanged();

private:
    Q_INVOKABLE void applyResult(int service, int status, const QString &error);

    void stopWorker();

    ProbeStatus::Status status_[3] = {ProbeStatus::Unknown, ProbeStatus::Unknown,
                                      ProbeStatus::Unknown};
    QString lastError_;

    struct Impl;
    std::unique_ptr<Impl> d_;
};
