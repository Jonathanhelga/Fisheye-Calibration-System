#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

class QNetworkAccessManager;
class QNetworkReply;

// Owns the committed HTTP server config (host + three service ports) and the
// reachability verdict for each one. A singleton because the committed URL has
// more than one reader: the Axis, Monitor and Camera panels all need it, and an
// `id` is only visible inside the file that declares it.
//
// The statuses are a receipt, not a live monitor: they report what happened the
// last time probeAll() ran, which is why editing a field voids them to Unknown.
class ServerProbe : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString host READ host NOTIFY configChanged)
    Q_PROPERTY(int axisPort READ axisPort NOTIFY configChanged)
    Q_PROPERTY(int monitorPort READ monitorPort NOTIFY configChanged)
    Q_PROPERTY(int cameraPort READ cameraPort NOTIFY configChanged)

    // One signal covers all four. Re-evaluating four colour bindings is free,
    // and it saves hostStatus() from tracking dependencies on the other three.
    Q_PROPERTY(Status axisStatus READ axisStatus NOTIFY statusesChanged)
    Q_PROPERTY(Status monitorStatus READ monitorStatus NOTIFY statusesChanged)
    Q_PROPERTY(Status cameraStatus READ cameraStatus NOTIFY statusesChanged)
    Q_PROPERTY(Status hostStatus READ hostStatus NOTIFY statusesChanged)

public:
    // Values double as indices into port_/status_/gen_, so do not reorder.
    enum Service { Axis = 0, Monitor = 1, Camera = 2 };
    Q_ENUM(Service)

    enum Status { Unknown, Checking, Ok, Failed, Partial };
    Q_ENUM(Status)

    explicit ServerProbe(QObject *parent = nullptr);
    ~ServerProbe() override;

    QString host() const { return host_; }
    int axisPort() const { return port_[Axis]; }
    int monitorPort() const { return port_[Monitor]; }
    int cameraPort() const { return port_[Camera]; }

    Status axisStatus() const { return status_[Axis]; }
    Status monitorStatus() const { return status_[Monitor]; }
    Status cameraStatus() const { return status_[Camera]; }
    Status hostStatus() const;

    // Update/ Call all probes again.
    Q_INVOKABLE void probeAll(const QString &host, int axis, int monitor, int camera);

    // One port field was edited
    Q_INVOKABLE void markUnknown(Service service);

    // The host was edited, reset all color
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
    Status status_[3] = {Unknown, Unknown, Unknown};

    QNetworkReply *inFlight_[3] = {nullptr, nullptr, nullptr};

    quint64 gen_[3] = {0, 0, 0};
};
