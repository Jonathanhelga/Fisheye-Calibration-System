#include "HttpServerProbe.h"

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

HttpServerProbe::HttpServerProbe(QObject *parent) : QObject(parent) {}

HttpServerProbe::~HttpServerProbe() = default;

QString HttpServerProbe::urlFor(int port) const {
    return QStringLiteral("http://%1:%2/").arg(host_, QString::number(port));
}

ProbeStatus::Status HttpServerProbe::hostStatus() const {
    int ok = 0;
    int failed = 0;

    for (ProbeStatus::Status s : status_) {
        if (s == ProbeStatus::Checking) return ProbeStatus::Checking;
        if (s == ProbeStatus::Unknown) return ProbeStatus::Unknown;
        if (s == ProbeStatus::Ok) ++ok;
        else ++failed;
    }

    if (failed == 0) return ProbeStatus::Ok;
    return ok == 0 ? ProbeStatus::Failed : ProbeStatus::Partial;
}

void HttpServerProbe::markUnknown(Service service) {
    cancel(service);
    if (status_[service] == ProbeStatus::Unknown) return;
    status_[service] = ProbeStatus::Unknown;
    emit statusesChanged();
}

void HttpServerProbe::resetAll() {
    bool changed = false;
    for (int i = 0; i < 3; ++i) {
        cancel(static_cast<Service>(i));
        if (status_[i] != ProbeStatus::Unknown) {
            status_[i] = ProbeStatus::Unknown;
            changed = true;
        }
    }
    if (changed) emit statusesChanged();
}

void HttpServerProbe::probeAll(const QString &host, int axis, int monitor, int camera) {
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

void HttpServerProbe::cancel(Service service) {
    ++gen_[service];
    if (QNetworkReply *reply = inFlight_[service]) {
        inFlight_[service] = nullptr;
        reply->abort();
    }
}

void HttpServerProbe::probeOne(Service service) {
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
    status_[service] = ProbeStatus::Checking;

    connect(reply, &QNetworkReply::finished, this, [this, service, generation, reply] {
        reply->deleteLater();
        if (generation != gen_[service]) return;
        inFlight_[service] = nullptr;
        status_[service] = answeredHttp(reply) ? ProbeStatus::Ok : ProbeStatus::Failed;
        emit statusesChanged();
    });
}
