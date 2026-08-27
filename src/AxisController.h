#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

#include <memory>

#include "AxisState.h"

class QTimer;

struct AxisSample {
    QString axis;
    int low = AxisState::Unreadable;
    int org = AxisState::Unreadable;
    int high = AxisState::Unreadable;
    int moving = AxisState::Unreadable;
    QString coordinate;
    QString raw;
    bool hasZero = false;
    bool positionValid = false;
};
Q_DECLARE_METATYPE(AxisSample)

struct AxisCommandRequest {
    enum Kind { Move, Stop };

    Kind kind = Move;
    QString axis;
    QString direction;
    double distance = 0.0;
    QString speed;
};

class AxisController : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(AxisState *x READ x CONSTANT)
    Q_PROPERTY(AxisState *y READ y CONSTANT)
    Q_PROPERTY(AxisState *z READ z CONSTANT)
    Q_PROPERTY(AxisState *yaw READ yaw CONSTANT)
    Q_PROPERTY(AxisState *pitch READ pitch CONSTANT)
    Q_PROPERTY(QList<AxisState *> allAxes READ allAxes CONSTANT)

    Q_PROPERTY(ConnectionState connectionState READ connectionState NOTIFY connectionChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY connectionChanged)
    Q_PROPERTY(int domainId READ domainId NOTIFY connectionChanged)
    Q_PROPERTY(QString axisNamespace READ axisNamespace NOTIFY connectionChanged)

    Q_PROPERTY(bool moveAvailable READ moveAvailable NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool commandAvailable READ commandAvailable NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool sensorAvailable READ sensorAvailable NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool positionAvailable READ positionAvailable NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool limitMoveAvailable READ limitMoveAvailable NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool homeActionAvailable READ homeActionAvailable NOTIFY capabilitiesChanged)
    Q_PROPERTY(StateSource stateSource READ stateSource NOTIFY capabilitiesChanged)
    Q_PROPERTY(QString capabilityText READ capabilityText NOTIFY capabilitiesChanged)

    Q_PROPERTY(bool anyMoving READ anyMoving NOTIFY activityChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY activityChanged)
    Q_PROPERTY(bool homing READ homing NOTIFY activityChanged)
    Q_PROPERTY(bool dataFresh READ dataFresh NOTIFY activityChanged)
    Q_PROPERTY(QString activityText READ activityText NOTIFY activityChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY statusChanged)

public:
    enum ConnectionState { Disconnected, Connecting, Connected, Degraded, Failed };
    Q_ENUM(ConnectionState)

    enum StateSource { NoSource, WatchTopic, Polling };
    Q_ENUM(StateSource)

    enum Speed { Low, Mid, High };
    Q_ENUM(Speed)

    enum Side { LowSide, HighSide };
    Q_ENUM(Side)

    enum Group { GroupXY, GroupZ, GroupRotation };
    Q_ENUM(Group)

    explicit AxisController(QObject *parent = nullptr);
    ~AxisController() override;

    AxisState *x() const;
    AxisState *y() const;
    AxisState *z() const;
    AxisState *yaw() const;
    AxisState *pitch() const;
    QList<AxisState *> allAxes() const;

    ConnectionState connectionState() const { return connectionState_; }
    bool connected() const { return connectionState_ == Connected || connectionState_ == Degraded; }
    int domainId() const { return domainId_; }
    QString axisNamespace() const { return axisNamespace_; }

    bool moveAvailable() const { return moveAvailable_; }
    bool commandAvailable() const { return commandAvailable_; }
    bool sensorAvailable() const { return sensorAvailable_; }
    bool positionAvailable() const { return positionAvailable_; }
    bool limitMoveAvailable() const { return limitMoveAvailable_; }
    bool homeActionAvailable() const { return homeActionAvailable_; }
    StateSource stateSource() const { return stateSource_; }
    QString capabilityText() const { return capabilityText_; }

    bool anyMoving() const;
    bool busy() const;
    bool homing() const { return !homingGroup_.isEmpty(); }
    bool dataFresh() const;
    QString activityText() const { return activityText_; }
    QString lastError() const { return lastError_; }

    Q_INVOKABLE void connectTo(int domainId, const QString &axisNamespace, bool force = false);
    Q_INVOKABLE void disconnectFromRig();

    Q_INVOKABLE AxisState *axis(const QString &name) const;

    Q_INVOKABLE void jog(const QString &axis, Side side, double distance, Speed speed);
    Q_INVOKABLE void driveToLimit(const QString &axis, Side side, Speed speed);
    Q_INVOKABLE void homeGroup(Group group);
    Q_INVOKABLE void stopAxis(const QString &axis);
    Q_INVOKABLE void stopAll();

    Q_INVOKABLE QStringList axesInGroup(Group group) const;

signals:
    void connectionChanged();
    void capabilitiesChanged();
    void activityChanged();
    void statusChanged();

    void commandRejected(const QString &axis, const QString &reason);
    void commandFailed(const QString &axis, const QString &reason);
    void operationTimedOut(const QString &axis, const QString &what, int ms);

    void homeGroupFinished(Group group, bool ok, const QString &message);

private:
    Q_INVOKABLE void applyConnection(int state, const QString &message, quint64 generation);
    Q_INVOKABLE void applyCapabilities(bool move, bool command, bool sensor, bool position,
                                       int stateSource, const QString &text, quint64 generation);
    Q_INVOKABLE void applySample(const AxisSample &sample, quint64 generation);
    Q_INVOKABLE void applyCommandOutcome(const QString &axis, bool ok, bool clearPending,
                                         const QString &message, quint64 generation);

    void setConnectionState(ConnectionState state, const QString &message);
    void clearCapabilities();
    void recomputeActivity();
    void teardownSession();
    void stopWorker();
    void enqueueCommand(const AxisCommandRequest &request);
    bool guardCommand(const QString &axis, bool needsMove);
    bool guardStop(const QString &axis);

    AxisState *axisOrNull(const QString &name) const;

    ConnectionState connectionState_ = Disconnected;
    int domainId_ = -1;
    QString axisNamespace_;

    bool moveAvailable_ = false;
    bool commandAvailable_ = false;
    bool sensorAvailable_ = false;
    bool positionAvailable_ = false;
    bool limitMoveAvailable_ = false;
    bool homeActionAvailable_ = false;
    StateSource stateSource_ = NoSource;
    QString capabilityText_;

    QString activityText_;
    bool busyReported_ = false;
    bool movingReported_ = false;
    bool freshReported_ = false;
    QString lastError_;
    QString homingGroup_;

    QTimer *stalenessTimer_ = nullptr;

    struct Impl;
    std::unique_ptr<Impl> d_;
};
