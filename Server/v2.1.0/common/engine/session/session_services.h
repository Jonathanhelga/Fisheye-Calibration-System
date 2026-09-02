#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <QByteArray>
#include <QString>
#include <opencv2/core.hpp>

#include "ComputeOps.h"
#include "session_registry.h"

// The session service handlers, as plain functions over a SessionRegistry.
//
// They are here rather than inside the node class so they can be driven by a test
// without a ROS graph: everything below takes and returns strings and bytes, and
// none of it mentions rclcpp. What the node adds is the transport and the
// threading.
namespace SessionOps {

// Result of a handler: what the service response is filled from.
struct Reply {
    bool success = false;
    QString sessionId;
    QString result = QStringLiteral("{}");
    QString message;
    quint64 version = 0;
};

// SessionCommand. `nowSeconds` is supplied by the caller so time is the node's
// concern, not this layer's. `event` is called once per state change, with the kind
// and detail that SessionEvent carries.
using EventFn = std::function<void(const QString &sessionId, quint64 version, const QString &kind,
                                   const QString &detail)>;

Reply command(SessionRegistry &reg, const QString &command, const QString &sessionId,
              const QString &payload, qint64 nowSeconds, const EventFn &event);

// SessionEdit. Parses the edits JSON and applies it as one undo step.
Reply edit(SessionRegistry &reg, const QString &sessionId, const QString &edits,
           const EventFn &event);

// SessionState. `changed` is false when the caller's mirror is already current.
Reply state(SessionRegistry &reg, const QString &sessionId, quint64 sinceVersion, bool *changed,
            QString *stateJson);

// SessionCapture, once the node has actually obtained a frame -- getting one from
// the camera topic is the node's job, storing it is this one's.
Reply putCapture(SessionRegistry &reg, const QString &sessionId, const QString &slot,
                 const QByteArray &encoded, int *width, int *height, const EventFn &event);

// SessionImage. Fills `out` with the bytes to send and reports the ORIGINAL size,
// which is not the size of `out` when maxSide shrank it.
Reply image(SessionRegistry &reg, const QString &sessionId, const QString &slot, int maxSide,
            QByteArray *out, int *width, int *height);

// RunCompute. Resolves the op's images from the session's capture slots, runs it,
// and writes any table changes back into the session.
Reply runCompute(SessionRegistry &reg, const QString &sessionId, const QString &op,
                 const QString &params, const ComputeOps::ProgressFn &progress,
                 const EventFn &event);

}  // namespace SessionOps
