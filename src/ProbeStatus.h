#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>

namespace ProbeStatus {
Q_NAMESPACE
QML_ELEMENT

enum Status {
    Unknown,
    Checking,
    Ok,
    Failed,
    Partial,
};
Q_ENUM_NS(Status)

} // namespace ProbeStatus
