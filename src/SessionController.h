#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QStringList>

#include <memory>

#include "ProbeStatus.h"

// The rig's session: where captures live, and where analysis happens.
//
// WHY THIS EXISTS, and why it is not just another service client.
//
// `/compute/detect` takes `sensor_msgs/CompressedImage[] images` -- the client
// uploads the pictures and the node analyses what it was handed. That is the path
// this app used until 2026-09-09, and it means every centre fit, every histogram
// and every node extraction re-sends two 3040x3040 frames the rig took itself and
// already has on disk. One Direction Diff is four such uploads.
//
// The session path does not work that way. A capture is written into a NAMED SLOT
// on the rig, and an op names the slot:
//
//     {"slots": ["positive", "negative"], "pos_cx": 1520, ... }
//
// Named, not sent. `/session/run_compute` resolves those names to the session's
// own files, so no image crosses the wire for analysis at all -- and the
// measurement outlives this client, because it lives in the session rather than
// in a QVariantMap that dies with the process.
//
// This is not a new idea here. The Qt Widgets client (`cpp/`, on
// v2.0_2026_main-cpp-ros) has done it this way since the server-calculation
// migration; `session_ros_client.cpp` is the reference this class was written
// from, down to the `slots` key and the command vocabulary. What is new is only
// that the QML client finally has it.
//
// ONE SESSION AT A TIME. The server keeps a current session
// (`ctx_.sessions->sessionId()`) and `RunCompute` answers from it, so this class
// holds a single id rather than a set. `openOrCreate` is the entry point: it
// opens a session of that name if one exists -- inheriting the shots already in
// it, so the curve panels work without re-shooting -- and creates one otherwise.
//
// Threading follows the house pattern exactly: its own `rclcpp::Context`, node
// and executor on a worker thread, a `generation` counter through every callback
// so a reconnect invalidates in-flight replies, and every result routed back to
// the GUI thread through a private `Q_INVOKABLE apply*`. Touching a Q_PROPERTY
// from the executor thread deadlocks on Win32 -- see AxisController.
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

    // Which slots the SERVER says are filled. A tally kept on this side drifts
    // the moment a session is reopened or another client shoots into it, so this
    // is only ever a copy of the server's answer.
    Q_PROPERTY(QStringList capturedSlots READ capturedSlots NOTIFY sessionChanged)

public:
    explicit SessionController(QObject *parent = nullptr);
    ~SessionController() override;

    // Reachable from CameraController and ComputeController, which need the
    // session id and the compute path but must not own them. Same precedent as
    // MonitorController::instance().
    static SessionController *instance();

    ProbeStatus::Status status() const { return status_; }
    QString sessionId() const { return sessionId_; }
    QString sessionName() const { return sessionName_; }
    bool hasSession() const { return !sessionId_.isEmpty(); }
    bool busy() const { return busy_ > 0; }
    QString lastError() const { return lastError_; }
    QStringList capturedSlots() const { return capturedSlots_; }

    Q_INVOKABLE void connectTo(int domainId);

    // Open a session of this name, or create one if there is none. Asynchronous;
    // watch sessionChanged.
    Q_INVOKABLE void openOrCreate(const QString &name);
    Q_INVOKABLE void closeSession();

    // Capture into a session slot. The frame is written on the RIG; nothing comes
    // back but the size. Use SessionController for the measurement path and
    // CameraController's own capture only for a preview.
    Q_INVOKABLE void capture(const QString &slot, double timeoutSeconds = 0.0);

    // The op, named not sent. `paramsJson` must carry a "slots" array naming the
    // capture slots the op should resolve. `token` is the caller's, echoed back
    // on computeFinished so several ops can be in flight without being confused
    // for one another -- which they are: a centre fit and a histogram run
    // concurrently on the Centering panel.
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
