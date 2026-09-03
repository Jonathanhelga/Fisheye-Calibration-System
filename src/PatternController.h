#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

#include <memory>

#include "ProbeStatus.h"

class QQmlImageProviderBase;

class PatternController : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(ProbeStatus::Status status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QString previewUrl READ previewUrl NOTIFY previewChanged)
    Q_PROPERTY(QString previewLabel READ previewLabel NOTIFY previewChanged)

public:
    explicit PatternController(QObject *parent = nullptr);
    ~PatternController() override;

    static QQmlImageProviderBase *createImageProvider();

    ProbeStatus::Status status() const { return status_; }
    bool busy() const { return busy_; }
    QString lastError() const { return lastError_; }
    QString previewUrl() const;
    QString previewLabel() const { return previewLabel_; }

    Q_INVOKABLE void connectTo(int domainId);
    Q_INVOKABLE void renderPreview(const QString &specJson, int width, int height);

signals:
    void statusChanged();
    void busyChanged();
    void lastErrorChanged();
    void previewChanged();

private:
    Q_INVOKABLE void applyLink(int status, const QString &message, quint64 generation);
    Q_INVOKABLE void applyPreview(bool ok, int width, int height, const QString &message,
                                  quint64 token, quint64 generation);

    void setStatus(ProbeStatus::Status status);
    void setBusy(bool busy);
    void setLastError(const QString &message);
    void stopWorker();

    ProbeStatus::Status status_ = ProbeStatus::Unknown;
    bool busy_ = false;
    int revision_ = 0;
    QString previewLabel_;
    QString lastError_;
    quint64 renderToken_ = 0;

    struct Impl;
    std::unique_ptr<Impl> d_;
};
