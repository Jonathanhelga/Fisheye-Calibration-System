#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

class AxisState : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("AxisState instances are owned by AxisController")

    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(QString unit READ unit CONSTANT)

    Q_PROPERTY(QString coordinate READ coordinate NOTIFY positionChanged)
    Q_PROPERTY(QString rawCount READ rawCount NOTIFY positionChanged)
    Q_PROPERTY(bool hasZero READ hasZero NOTIFY positionChanged)
    Q_PROPERTY(bool positionKnown READ positionKnown NOTIFY positionChanged)

    Q_PROPERTY(int sensorLow READ sensorLow NOTIFY sensorsChanged)
    Q_PROPERTY(int sensorOrg READ sensorOrg NOTIFY sensorsChanged)
    Q_PROPERTY(int sensorHigh READ sensorHigh NOTIFY sensorsChanged)
    Q_PROPERTY(int sensorMoving READ sensorMoving NOTIFY sensorsChanged)

    Q_PROPERTY(bool moving READ moving NOTIFY activityChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY activityChanged)
    Q_PROPERTY(bool awaitingRig READ awaitingRig NOTIFY activityChanged)
    Q_PROPERTY(bool settling READ settling NOTIFY activityChanged)
    Q_PROPERTY(QString activity READ activity NOTIFY activityChanged)

    Q_PROPERTY(bool stale READ stale NOTIFY interlockChanged)
    Q_PROPERTY(bool lowBlocked READ lowBlocked NOTIFY interlockChanged)
    Q_PROPERTY(bool highBlocked READ highBlocked NOTIFY interlockChanged)

public:
    enum Tri { Unreadable = -1, Clear = 0, Triggered = 1 };
    Q_ENUM(Tri)

    QString name() const { return name_; }
    QString unit() const { return unit_; }

    QString coordinate() const { return coordinate_; }
    QString rawCount() const { return rawCount_; }
    bool hasZero() const { return hasZero_; }
    bool positionKnown() const { return positionKnown_; }

    int sensorLow() const { return sensorLow_; }
    int sensorOrg() const { return sensorOrg_; }
    int sensorHigh() const { return sensorHigh_; }
    int sensorMoving() const { return sensorMoving_; }

    bool moving() const { return moving_; }
    bool busy() const { return moving_ || commandPending_; }
    bool awaitingRig() const { return commandPending_ && !rigReplied_ && !moving_; }
    bool settling() const { return commandPending_ && rigReplied_ && !moving_; }
    QString activity() const { return activity_; }

    bool stale() const { return stale_; }
    bool lowBlocked() const { return stale_ || sensorLow_ != Clear; }
    bool highBlocked() const { return stale_ || sensorHigh_ != Clear; }

signals:
    void positionChanged();
    void sensorsChanged();
    void activityChanged();
    void interlockChanged();

private:
    friend class AxisController;

    AxisState(const QString &name, const QString &unit, QObject *parent);

    void applySample(int low, int org, int high, int moving, const QString &coordinate,
                     const QString &raw, bool hasZero, bool positionValid);
    void applyLimitFeedback(int sensor, bool highSide, const QString &coordinate);
    void setCommandPending(bool pending, const QString &activity);
    void markRigReplied();
    void refreshStaleness();
    void resetToUnknown();

    void updateActivity(bool busyWas, bool awaitingWas);

    const QString name_;
    const QString unit_;

    QString coordinate_;
    QString rawCount_;
    bool hasZero_ = false;
    bool positionKnown_ = false;

    int sensorLow_ = Unreadable;
    int sensorOrg_ = Unreadable;
    int sensorHigh_ = Unreadable;
    int sensorMoving_ = Unreadable;

    bool moving_ = false;
    bool commandPending_ = false;
    bool rigReplied_ = false;
    QString activity_;

    bool stale_ = true;
    int idleStreak_ = 0;
    int motionStreak_ = 0;
    int lowDropouts_ = 0;
    int orgDropouts_ = 0;
    int highDropouts_ = 0;
    bool sawMotion_ = false;

    QElapsedTimer sampleClock_;
};
