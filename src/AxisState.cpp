#include "AxisState.h"

namespace {

constexpr qint64 kStaleMs = 30000;
constexpr int kIdleNeeded = 2;
constexpr int kMotionNeeded = 2;
constexpr int kDropoutsAllowed = 1;

const QString kNoValue = QStringLiteral("—");

int holdThrough(int incoming, int previous, int &dropouts) {
    if (incoming != AxisState::Unreadable) {
        dropouts = 0;
        return incoming;
    }
    if (previous == AxisState::Unreadable) return AxisState::Unreadable;
    if (++dropouts > kDropoutsAllowed) return AxisState::Unreadable;
    return previous;
}

} // namespace

AxisState::AxisState(const QString &name, const QString &unit, QObject *parent)
    : QObject(parent), name_(name), unit_(unit), coordinate_(kNoValue) {}

void AxisState::resetToUnknown() {
    const bool positionWas = positionKnown_ || coordinate_ != kNoValue || hasZero_;
    const bool sensorsWere = sensorLow_ != Unreadable || sensorOrg_ != Unreadable ||
                             sensorHigh_ != Unreadable || sensorMoving_ != Unreadable;
    const bool lowBlockedWas = lowBlocked();
    const bool highBlockedWas = highBlocked();
    const bool staleWas = stale_;

    coordinate_ = kNoValue;
    rawCount_.clear();
    hasZero_ = false;
    positionKnown_ = false;

    sensorLow_ = Unreadable;
    sensorOrg_ = Unreadable;
    sensorHigh_ = Unreadable;
    sensorMoving_ = Unreadable;

    stale_ = true;
    idleStreak_ = 0;
    motionStreak_ = 0;
    lowDropouts_ = 0;
    orgDropouts_ = 0;
    highDropouts_ = 0;
    sawMotion_ = false;
    rigReplied_ = false;
    sampleClock_.invalidate();

    if (positionWas) emit positionChanged();
    if (sensorsWere) emit sensorsChanged();
    if (!staleWas || lowBlockedWas != lowBlocked() || highBlockedWas != highBlocked())
        emit interlockChanged();

    const bool busyWas = busy();
    moving_ = false;
    commandPending_ = false;
    activity_.clear();
    if (busyWas) emit activityChanged();
}

void AxisState::applySample(int low, int org, int high, int moving, const QString &coordinate,
                            const QString &raw, bool hasZero, bool positionValid) {
    const bool lowBlockedWas = lowBlocked();
    const bool highBlockedWas = highBlocked();
    const bool staleWas = stale_;
    const bool busyWas = busy();
    const bool awaitingWas = awaitingRig();

    const int heldLow = holdThrough(low, sensorLow_, lowDropouts_);
    const int heldOrg = holdThrough(org, sensorOrg_, orgDropouts_);
    const int heldHigh = holdThrough(high, sensorHigh_, highDropouts_);

    const bool sensorsChangedNow = heldLow != sensorLow_ || heldOrg != sensorOrg_ ||
                                   heldHigh != sensorHigh_ || moving != sensorMoving_;
    sensorLow_ = heldLow;
    sensorOrg_ = heldOrg;
    sensorHigh_ = heldHigh;
    sensorMoving_ = moving;

    if (positionValid) {
        const bool positionChangedNow =
            coordinate != coordinate_ || raw != rawCount_ || hasZero != hasZero_ || !positionKnown_;
        coordinate_ = coordinate;
        rawCount_ = raw;
        hasZero_ = hasZero;
        positionKnown_ = true;
        if (positionChangedNow) emit positionChanged();
    }

    if (moving == Clear) {
        ++idleStreak_;
        motionStreak_ = 0;
    } else if (moving == Triggered) {
        ++motionStreak_;
        idleStreak_ = 0;
    } else {
        idleStreak_ = 0;
        motionStreak_ = 0;
    }

    if (moving == Triggered && (commandPending_ || motionStreak_ >= kMotionNeeded)) {
        sawMotion_ = true;
        moving_ = true;
    } else if (moving_ && idleStreak_ >= kIdleNeeded) {
        moving_ = false;
    }

    stale_ = false;
    sampleClock_.restart();

    if (sensorsChangedNow) emit sensorsChanged();
    if (staleWas || lowBlockedWas != lowBlocked() || highBlockedWas != highBlocked())
        emit interlockChanged();

    qWarning("AXISDBG sample %s low=%d org=%d high=%d moving=%d posValid=%d "
             "idle=%d motion=%d pending=%d replied=%d moving_=%d",
             qPrintable(name_), sensorLow_, sensorOrg_, sensorHigh_, sensorMoving_,
             int(positionValid), idleStreak_, motionStreak_, int(commandPending_),
             int(rigReplied_), int(moving_));

    updateActivity(busyWas, awaitingWas);
}

void AxisState::applyLimitFeedback(int sensor, bool highSide, const QString &coordinate) {
    const bool lowBlockedWas = lowBlocked();
    const bool highBlockedWas = highBlocked();
    const bool staleWas = stale_;

    bool sensorsChangedNow = false;
    if (highSide && sensor != sensorHigh_) {
        sensorHigh_ = sensor;
        sensorsChangedNow = true;
    } else if (!highSide && sensor != sensorLow_) {
        sensorLow_ = sensor;
        sensorsChangedNow = true;
    }

    if (!coordinate.isEmpty() && coordinate != coordinate_) {
        coordinate_ = coordinate;
        positionKnown_ = true;
        emit positionChanged();
    }

    stale_ = false;
    sampleClock_.restart();

    if (sensorsChangedNow) emit sensorsChanged();
    if (staleWas || lowBlockedWas != lowBlocked() || highBlockedWas != highBlocked())
        emit interlockChanged();
}

void AxisState::setCommandPending(bool pending, const QString &activity) {
    if (commandPending_ == pending && activity_ == activity) return;

    commandPending_ = pending;
    activity_ = activity;

    if (pending) {
        sawMotion_ = false;
        rigReplied_ = false;
        idleStreak_ = 0;
        motionStreak_ = 0;
    }

    qWarning("AXISDBG pending %s -> %d (%s)", qPrintable(name_), int(pending),
             qPrintable(activity_));

    emit activityChanged();
}

void AxisState::markRigReplied() {
    if (!commandPending_ || rigReplied_) return;

    const bool busyWas = busy();
    const bool awaitingWas = awaitingRig();

    rigReplied_ = true;
    if (!sawMotion_) idleStreak_ = 0;

    qWarning("AXISDBG rigReplied %s sawMotion=%d idle=%d", qPrintable(name_),
             int(sawMotion_), idleStreak_);

    updateActivity(busyWas, awaitingWas);
}

void AxisState::updateActivity(bool busyWas, bool awaitingWas) {
    const QString activityWas = activity_;

    if (commandPending_ && rigReplied_ && !moving_ && idleStreak_ >= kIdleNeeded) {
        commandPending_ = false;
        activity_.clear();
    }

    if (busyWas != busy() || awaitingWas != awaitingRig() || activityWas != activity_)
        emit activityChanged();
}

void AxisState::refreshStaleness() {
    if (stale_) return;
    if (sampleClock_.isValid() && sampleClock_.elapsed() < kStaleMs) return;

    qWarning("AXISDBG stale %s after %lld ms, pending=%d", qPrintable(name_),
             sampleClock_.isValid() ? sampleClock_.elapsed() : -1, int(commandPending_));

    stale_ = true;
    idleStreak_ = 0;
    motionStreak_ = 0;
    lowDropouts_ = 0;
    orgDropouts_ = 0;
    highDropouts_ = 0;
    sawMotion_ = false;
    rigReplied_ = false;

    const bool busyWas = busy();
    moving_ = false;
    commandPending_ = false;
    activity_.clear();

    emit interlockChanged();
    if (busyWas) emit activityChanged();
}
