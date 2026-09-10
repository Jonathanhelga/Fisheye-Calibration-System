#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

#include "ProbeStatus.h"

class QNetworkAccessManager;
class QNetworkReply;

// Committed HTTP config plus last probe verdicts.
class HttpServerProbe : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString host READ host NOTIFY configChanged)
    Q_PROPERTY(int axisPort READ axisPort NOTIFY configChanged)
    Q_PROPERTY(int monitorPort READ monitorPort NOTIFY configChanged)
    Q_PROPERTY(int cameraPort READ cameraPort NOTIFY configChanged)

    // One signal covers all four values.
    Q_PROPERTY(ProbeStatus::Status axisStatus READ axisStatus NOTIFY statusesChanged)
    Q_PROPERTY(ProbeStatus::Status monitorStatus READ monitorStatus NOTIFY statusesChanged)
    Q_PROPERTY(ProbeStatus::Status cameraStatus READ cameraStatus NOTIFY statusesChanged)
    Q_PROPERTY(ProbeStatus::Status hostStatus READ hostStatus NOTIFY statusesChanged)

public:
    // Values index port_/status_/gen_; do not reorder.
    enum Service { Axis = 0, Monitor = 1, Camera = 2 };
    Q_ENUM(Service)

    explicit HttpServerProbe(QObject *parent = nullptr);
    ~HttpServerProbe() override;

    QString host() const { return host_; }
    int axisPort() const { return port_[Axis]; }
    int monitorPort() const { return port_[Monitor]; }
    int cameraPort() const { return port_[Camera]; }

    ProbeStatus::Status axisStatus() const { return status_[Axis]; }
    ProbeStatus::Status monitorStatus() const { return status_[Monitor]; }
    ProbeStatus::Status cameraStatus() const { return status_[Camera]; }
    ProbeStatus::Status hostStatus() const;

    // Re-run every probe.
    Q_INVOKABLE void probeAll(const QString &host, int axis, int monitor, int camera);

    // One port field was edited.
    Q_INVOKABLE void markUnknown(Service service);

    // Host edited; reset every status.
    Q_INVOKABLE void resetAll();

    Q_INVOKABLE QString urlFor(int port) const;

signals:
    void configChanged();
    void statusesChanged();

private:
    void probeOne(Service service);
    void cancel(Service service);

    QNetworkAccessManager *nam_ = nullptr;

    QString host_;
    int port_[3] = {0, 0, 0};
    ProbeStatus::Status status_[3] = {ProbeStatus::Unknown, ProbeStatus::Unknown,
                                      ProbeStatus::Unknown};

    QNetworkReply *inFlight_[3] = {nullptr, nullptr, nullptr};

    quint64 gen_[3] = {0, 0, 0};
};
