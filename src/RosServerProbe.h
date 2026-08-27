#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

#include <memory>

class RosServerProbe : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(Status axisStatus READ axisStatus NOTIFY statusesChanged)
    Q_PROPERTY(Status monitorStatus READ monitorStatus NOTIFY statusesChanged)
    Q_PROPERTY(Status cameraStatus READ cameraStatus NOTIFY statusesChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY statusesChanged)

public:
    enum Service { Axis = 0, Monitor = 1, Camera = 2 };
    Q_ENUM(Service)

    enum Status { Unknown, Checking, Ok, Failed, Partial };
    Q_ENUM(Status)

    explicit RosServerProbe(QObject *parent = nullptr);
    ~RosServerProbe() override;

    Status axisStatus() const { return status_[Axis]; }
    Status monitorStatus() const { return status_[Monitor]; }
    Status cameraStatus() const { return status_[Camera]; }
    QString lastError() const { return lastError_; }

    Q_INVOKABLE void probeAll(int domainId, const QString &axisNamespace,
                              const QString &monitorNamespace, const QString &cameraTopic);

    Q_INVOKABLE void resetAll();

signals:
    void statusesChanged();

private:
    Q_INVOKABLE void applyResult(int service, int status, const QString &error);

    void stopWorker();

    Status status_[3] = {Unknown, Unknown, Unknown};
    QString lastError_;

    struct Impl;
    std::unique_ptr<Impl> d_;
};
