#include "AxisController.h"

#include "AxisState.h"

#include <QCoreApplication>
#include <QTimer>

#include <atomic>

namespace {

constexpr int kStalenessTickMs = 500;

QString normaliseNamespace(const QString &value) {
    QString ns = value.trimmed();
    while (ns.endsWith('/')) ns.chop(1);
    if (ns.isEmpty()) return QStringLiteral("/axis");
    return ns.startsWith('/') ? ns : '/' + ns;
}

QString sideWord(const QString &axis, AxisController::Side side) {
    const bool high = side == AxisController::HighSide;
    if (axis == QLatin1String("x")) return high ? QStringLiteral("right") : QStringLiteral("left");
    if (axis == QLatin1String("y")) return high ? QStringLiteral("up") : QStringLiteral("down");
    if (axis == QLatin1String("z")) return high ? QStringLiteral("forward") : QStringLiteral("backward");
    if (axis == QLatin1String("yaw")) return high ? QStringLiteral("right") : QStringLiteral("left");
    if (axis == QLatin1String("pitch")) return high ? QStringLiteral("up") : QStringLiteral("down");
    return high ? QStringLiteral("high") : QStringLiteral("low");
}

QString axisLabel(const QString &axis) {
    if (axis == QLatin1String("yaw")) return QStringLiteral("Yaw");
    if (axis == QLatin1String("pitch")) return QStringLiteral("Pitch");
    return axis.toUpper();
}

} // namespace

struct AxisController::Impl {
    std::atomic<quint64> generation{0};

    AxisState *x = nullptr;
    AxisState *y = nullptr;
    AxisState *z = nullptr;
    AxisState *yaw = nullptr;
    AxisState *pitch = nullptr;

    QList<AxisState *> all;
};

AxisController::AxisController(QObject *parent) : QObject(parent), d_(new Impl) {
    d_->x = new AxisState(QStringLiteral("x"), QStringLiteral("mm"), this);
    d_->y = new AxisState(QStringLiteral("y"), QStringLiteral("mm"), this);
    d_->z = new AxisState(QStringLiteral("z"), QStringLiteral("mm"), this);
    d_->yaw = new AxisState(QStringLiteral("yaw"), QStringLiteral("°"), this);
    d_->pitch = new AxisState(QStringLiteral("pitch"), QStringLiteral("°"), this);
    d_->all = {d_->x, d_->y, d_->z, d_->yaw, d_->pitch};

    for (AxisState *state : std::as_const(d_->all)) {
        connect(state, &AxisState::activityChanged, this, &AxisController::recomputeActivity);
        connect(state, &AxisState::interlockChanged, this, &AxisController::recomputeActivity);
    }

    stalenessTimer_ = new QTimer(this);
    stalenessTimer_->setInterval(kStalenessTickMs);
    connect(stalenessTimer_, &QTimer::timeout, this, [this] {
        for (AxisState *state : std::as_const(d_->all)) state->refreshStaleness();
    });

    connect(qApp, &QCoreApplication::aboutToQuit, this, [this] { teardownSession(); });
}

AxisController::~AxisController() { teardownSession(); }

AxisState *AxisController::x() const { return d_->x; }
AxisState *AxisController::y() const { return d_->y; }
AxisState *AxisController::z() const { return d_->z; }
AxisState *AxisController::yaw() const { return d_->yaw; }
AxisState *AxisController::pitch() const { return d_->pitch; }
QList<AxisState *> AxisController::allAxes() const { return d_->all; }

AxisState *AxisController::axisOrNull(const QString &name) const {
    const QString key = name.trimmed().toLower();
    for (AxisState *state : std::as_const(d_->all)) {
        if (state->name() == key) return state;
    }
    return nullptr;
}

AxisState *AxisController::axis(const QString &name) const { return axisOrNull(name); }

bool AxisController::anyMoving() const {
    for (AxisState *state : std::as_const(d_->all)) {
        if (state->moving()) return true;
    }
    return false;
}

bool AxisController::busy() const {
    if (homing()) return true;
    for (AxisState *state : std::as_const(d_->all)) {
        if (state->busy()) return true;
    }
    return false;
}

bool AxisController::dataFresh() const {
    if (!connected()) return false;
    for (AxisState *state : std::as_const(d_->all)) {
        if (state->stale()) return false;
    }
    return true;
}

QStringList AxisController::axesInGroup(Group group) const {
    switch (group) {
    case GroupXY:
        return {QStringLiteral("x"), QStringLiteral("y")};
    case GroupZ:
        return {QStringLiteral("z")};
    case GroupRotation:
        return {QStringLiteral("yaw"), QStringLiteral("pitch")};
    }
    return {};
}

void AxisController::setConnectionState(ConnectionState state, const QString &message) {
    const bool changed = connectionState_ != state;
    connectionState_ = state;

    if (message != lastError_) {
        lastError_ = message;
        emit statusChanged();
    }

    if (state == Connected || state == Degraded) {
        stalenessTimer_->start();
    } else {
        stalenessTimer_->stop();
        for (AxisState *axis : std::as_const(d_->all)) axis->resetToUnknown();
    }

    if (changed) {
        emit connectionChanged();
        recomputeActivity();
    }
}

void AxisController::clearCapabilities() {
    moveAvailable_ = false;
    commandAvailable_ = false;
    sensorAvailable_ = false;
    positionAvailable_ = false;
    limitMoveAvailable_ = false;
    homeActionAvailable_ = false;
    stateSource_ = NoSource;
    capabilityText_.clear();
    emit capabilitiesChanged();
}

void AxisController::applyConnection(int state, const QString &message, quint64 generation) {
    if (generation != d_->generation.load()) return;
    setConnectionState(static_cast<ConnectionState>(state), message);
}

void AxisController::connectTo(int domainId, const QString &axisNamespace, bool force) {
    const QString ns = normaliseNamespace(axisNamespace);

    if (!force && connectionState_ == Connected && domainId == domainId_ && ns == axisNamespace_) {
        emit connectionChanged();
        return;
    }

    teardownSession();

    domainId_ = domainId;
    axisNamespace_ = ns;
    const quint64 generation = d_->generation.load();

    setConnectionState(Connecting, QString());
    emit connectionChanged();

#ifdef FISHEYE_ROS_ENABLED
    applyConnection(Failed,
                    QStringLiteral("axis ROS session is not implemented yet "
                                   "(namespace %1, domain %2)")
                        .arg(ns)
                        .arg(domainId),
                    generation);
#else
    Q_UNUSED(generation)
    applyConnection(Failed,
                    QStringLiteral("this build has no ROS 2 support "
                                   "(configure with -DFISHEYE_ENABLE_ROS=ON on Linux)"),
                    d_->generation.load());
#endif
}

void AxisController::disconnectFromRig() {
    teardownSession();
    setConnectionState(Disconnected, QString());
}

void AxisController::teardownSession() {
    ++d_->generation;
    stalenessTimer_->stop();

    for (AxisState *state : std::as_const(d_->all)) state->resetToUnknown();

    homingGroup_.clear();
    clearCapabilities();
    recomputeActivity();
}

bool AxisController::guardCommand(const QString &axis, bool needsMove) {
    if (!connected()) {
        emit commandRejected(axis, tr("not connected to the axis node"));
        return false;
    }
    if (needsMove && !moveAvailable_) {
        emit commandRejected(axis, tr("the axis node is not serving %1/move").arg(axisNamespace_));
        return false;
    }
    AxisState *state = axisOrNull(axis);
    if (!state) {
        emit commandRejected(axis, tr("unknown axis"));
        return false;
    }
    if (state->busy()) {
        emit commandRejected(axis, tr("%1 is already moving").arg(axisLabel(axis)));
        return false;
    }
    if (busy()) {
        emit commandRejected(axis, tr("another axis is moving"));
        return false;
    }
    return true;
}

void AxisController::jog(const QString &axis, Side side, double distance, Speed speed) {
    Q_UNUSED(speed)

    const QString key = axis.trimmed().toLower();
    if (distance <= 0.0) {
        emit commandRejected(key, tr("step distance must be greater than zero"));
        return;
    }
    if (!guardCommand(key, true)) return;

    AxisState *state = axisOrNull(key);
    if (state->stale()) {
        emit commandRejected(key, tr("%1 state is stale, position is unknown").arg(axisLabel(key)));
        return;
    }

    const bool high = side == HighSide;
    if (high ? state->highBlocked() : state->lowBlocked()) {
        emit commandRejected(key, tr("%1 %2 is blocked by its limit sensor")
                                      .arg(axisLabel(key), sideWord(key, side)));
        return;
    }

    emit commandRejected(key, tr("axis motion is not wired to the rig yet"));
}

void AxisController::driveToLimit(const QString &axis, Side side, Speed speed) {
    Q_UNUSED(speed)

    const QString key = axis.trimmed().toLower();
    if (!guardCommand(key, false)) return;
    if (!limitMoveAvailable_) {
        emit commandRejected(key, tr("the rig is not serving %1/limit_move").arg(axisNamespace_));
        return;
    }

    AxisState *state = axisOrNull(key);
    if (state->stale()) {
        emit commandRejected(key, tr("%1 state is stale, position is unknown").arg(axisLabel(key)));
        return;
    }

    const bool high = side == HighSide;
    if (high ? state->highBlocked() : state->lowBlocked()) {
        emit commandRejected(key, tr("%1 is already on its %2 limit")
                                      .arg(axisLabel(key), sideWord(key, side)));
        return;
    }

    emit commandRejected(key, tr("drive-to-limit is not wired to the rig yet"));
}

void AxisController::homeGroup(Group group) {
    if (!connected()) {
        emit commandRejected(QString(), tr("not connected to the axis node"));
        return;
    }
    if (busy()) {
        emit commandRejected(QString(), tr("an axis is already moving"));
        return;
    }
    if (!dataFresh()) {
        emit commandRejected(QString(), tr("axis state is stale, position is unknown"));
        return;
    }
    emit homeGroupFinished(group, false, tr("homing is not wired to the rig yet"));
}

bool AxisController::guardStop(const QString &axis) {
    if (!connected()) {
        emit commandRejected(axis, tr("not connected to the axis node"));
        return false;
    }
    if (!commandAvailable_) {
        emit commandRejected(axis,
                             tr("the axis node is not serving %1/command").arg(axisNamespace_));
        return false;
    }
    return true;
}

void AxisController::stopAxis(const QString &axis) {
    const QString key = axis.trimmed().toLower();
    if (!guardStop(key)) return;
    emit commandRejected(key, tr("stop is not wired to the rig yet"));
}

void AxisController::stopAll() {
    if (!guardStop(QString())) return;
    emit commandRejected(QString(), tr("stop is not wired to the rig yet"));
}

void AxisController::recomputeActivity() {
    QString text;
    for (AxisState *state : std::as_const(d_->all)) {
        if (!state->busy()) continue;
        text = tr("Moving %1").arg(axisLabel(state->name()));
        break;
    }
    if (homing()) text = tr("Homing %1").arg(homingGroup_);

    if (text == activityText_ && busy() == busyReported_ && anyMoving() == movingReported_ &&
        dataFresh() == freshReported_)
        return;

    activityText_ = text;
    busyReported_ = busy();
    movingReported_ = anyMoving();
    freshReported_ = dataFresh();
    emit activityChanged();
}
