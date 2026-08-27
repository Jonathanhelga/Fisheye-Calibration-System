#include "ServerProbe.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>
#include <QVariant>

namespace {

constexpr int kTimeoutMs = 2000;

bool answeredHttp(QNetworkReply *reply) {
    const QVariant code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    return code.isValid() && code.toInt() > 0;
}

} // namespace

ServerProbe::ServerProbe(QObject *parent) : QObject(parent) {}

ServerProbe::~ServerProbe() = default;

QString ServerProbe::urlFor(int port) const {
    return QStringLiteral("http://%1:%2/").arg(host_, QString::number(port));
}

ServerProbe::Status ServerProbe::hostStatus() const {
    int ok = 0;
    int failed = 0;

    for (Status s : status_) {
        if (s == Checking) return Checking;
        if (s == Unknown) return Unknown;
        if (s == Ok) ++ok;
        else ++failed;
    }

    if (failed == 0) return Ok;
    return ok == 0 ? Failed : Partial;
}

void ServerProbe::markUnknown(Service service) {
    cancel(service);
    if (status_[service] == Unknown) return;
    status_[service] = Unknown;
    emit statusesChanged();
}

void ServerProbe::resetAll() {
    bool changed = false;
    for (int i = 0; i < 3; ++i) {
        cancel(static_cast<Service>(i));
        if (status_[i] != Unknown) {
            status_[i] = Unknown;
            changed = true;
        }
    }
    if (changed) emit statusesChanged();
}

void ServerProbe::probeAll(const QString &host, int axis, int monitor, int camera) {
    host_ = host;
    port_[Axis] = axis;
    port_[Monitor] = monitor;
    port_[Camera] = camera;
    emit configChanged();

    probeOne(Axis);
    probeOne(Monitor);
    probeOne(Camera);

    emit statusesChanged();
}

void ServerProbe::cancel(Service service) {
    ++gen_[service];
    if (QNetworkReply *reply = inFlight_[service]) {
        inFlight_[service] = nullptr;
        reply->abort();
    }
}

void ServerProbe::probeOne(Service service) {
    cancel(service);

    if (!nam_) {
        nam_ = new QNetworkAccessManager(this);
        nam_->setRedirectPolicy(QNetworkRequest::ManualRedirectPolicy);
    }

    QNetworkRequest request{QUrl(urlFor(port_[service]))};
    request.setTransferTimeout(kTimeoutMs);

    const quint64 generation = gen_[service];

    QNetworkReply *reply = nam_->get(request);
    inFlight_[service] = reply;
    status_[service] = Checking;

    connect(reply, &QNetworkReply::finished, this, [this, service, generation, reply] {
        reply->deleteLater();
        if (generation != gen_[service]) return;
        inFlight_[service] = nullptr;
        status_[service] = answeredHttp(reply) ? Ok : Failed;
        emit statusesChanged();
    });
}
