#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QStringList>

#include <memory>

#include "ProbeStatus.h"

// The rig's session: named capture slots, server-side analysis.
class SessionController : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(ProbeStatus::Status status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString sessionId READ sessionId NOTIFY sessionChanged)
    Q_PROPERTY(QString sessionName READ sessionName NOTIFY sessionChanged)
    Q_PROPERTY(bool hasSession READ hasSession NOTIFY sessionChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

    // Slots the server says are filled.
    Q_PROPERTY(QStringList capturedSlots READ capturedSlots NOTIFY sessionChanged)

public:
    explicit SessionController(QObject *parent = nullptr);
    ~SessionController() override;

    // Reachable from Camera and Compute controllers.
    static SessionController *instance();

    ProbeStatus::Status status() const { return status_; }
    QString sessionId() const { return sessionId_; }
    QString sessionName() const { return sessionName_; }
    bool hasSession() const { return !sessionId_.isEmpty(); }
    bool busy() const { return busy_ > 0; }
    QString lastError() const { return lastError_; }
    QStringList capturedSlots() const { return capturedSlots_; }

    Q_INVOKABLE void connectTo(int domainId);

    // Open by name, or create. Asynchronous.
    Q_INVOKABLE void openOrCreate(const QString &name);
    Q_INVOKABLE void closeSession();

    // Capture into a session slot on the rig.
    Q_INVOKABLE void capture(const QString &slot, double timeoutSeconds = 0.0);

    // paramsJson names slots; token echoes back.
    void runCompute(const QString &op, const QString &paramsJson, quint64 token);

signals:
    void statusChanged();
    void sessionChanged();
    void busyChanged();
    void lastErrorChanged();
    void notice(const QString &message);

    void computeFinished(quint64 token, bool ok, const QString &resultJson,
                         const QString &message);
    void computeProgress(quint64 token, int done, int total, const QString &stage);
    void captured(const QString &slot, bool ok, int width, int height, const QString &message);

private:
    Q_INVOKABLE void applyLink(int status, const QString &message, quint64 generation);
    Q_INVOKABLE void applySession(const QString &id, const QString &name, bool ok,
                                  const QString &message, quint64 generation);
    Q_INVOKABLE void applyCaptured(const QString &slot, bool ok, int width, int height,
                                   const QString &message, quint64 generation);
    Q_INVOKABLE void applyCompute(quint64 token, bool ok, const QString &resultJson,
                                  const QString &message, quint64 generation);
    Q_INVOKABLE void applyProgress(quint64 token, int done, int total, const QString &stage,
                                   quint64 generation);

    void stopWorker();
    void setStatus(ProbeStatus::Status status);
    void setLastError(const QString &message);
    void beginCall();
    void endCall();

    ProbeStatus::Status status_ = ProbeStatus::Unknown;
    QString sessionId_;
    QString sessionName_;
    QString lastError_;
    QStringList capturedSlots_;
    int busy_ = 0;
    int domainId_ = 42;

    struct Impl;
    std::unique_ptr<Impl> d_;
};
