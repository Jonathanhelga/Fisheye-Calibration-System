// Panel brightness, over DDC/CI.
//
// This is a Win32 call that has nothing to do with windows or the GUI thread, so
// it runs wherever the caller is. Only the panel bookkeeping it touches -- the
// ramp pacing -- needs the lock.

#include "monitor_device_p.h"

#include <cmath>
#include <iostream>

#include <QDateTime>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
// Order matters: both of these need windows.h first.
#include <highlevelmonitorconfigurationapi.h>
#include <physicalmonitorenumerationapi.h>
#endif

#ifdef _WIN32
namespace {

// Device name -> HMONITOR, which is what the brightness API takes and what Qt
// does not expose. Keyed by szDevice, which is why monitor_screens.cpp goes to
// the trouble of resolving the device name rather than using QScreen::name().
BOOL CALLBACK collectMonitor(HMONITOR handle, HDC, LPRECT, LPARAM param) {
    MONITORINFOEXW info{};
    info.cbSize = sizeof(info);
    if (GetMonitorInfoW(handle, &info)) {
        auto *map = reinterpret_cast<QHash<QString, HMONITOR> *>(param);
        map->insert(monitor_detail::normaliseDisplay(QString::fromWCharArray(info.szDevice)),
                    handle);
    }
    return TRUE;
}

QHash<QString, HMONITOR> monitorHandles() {
    QHash<QString, HMONITOR> map;
    EnumDisplayMonitors(nullptr, nullptr, collectMonitor, reinterpret_cast<LPARAM>(&map));
    return map;
}

}  // namespace
#endif

bool MonitorDevice::Impl::setBrightnessOne(const QString &direction, double brightness,
                                           QString *err) {
    Panel *panel = panelFor(direction);
    if (!panel) {
        *err = QStringLiteral("direction \"%1\" is not mapped to a display yet").arg(direction);
        return false;
    }

#ifdef _WIN32
    const QHash<QString, HMONITOR> handles = monitorHandles();
    const auto it = handles.constFind(panel->displayNum);
    if (it == handles.constEnd()) {
        *err = QStringLiteral("no monitor handle for %1").arg(panel->displayNum);
        return false;
    }

    PHYSICAL_MONITOR physical{};
    if (!GetPhysicalMonitorsFromHMONITOR(*it, 1, &physical)) {
        *err = QStringLiteral("GetPhysicalMonitorsFromHMONITOR failed for %1")
                   .arg(panel->displayNum);
        return false;
    }

    // Map the UI's 0..100 onto whatever range the panel reports. Screens in the
    // wild report 0..100, 0..200, 0..255 and 10..90; assuming one of them is how
    // "50%" ends up meaning half brightness on one panel and a quarter on the
    // next, which shows up in the calibration as a systematic difference between
    // directions.
    DWORD minimum = 0, current = 0, maximum = 0;
    DWORD actual;
    if (GetMonitorBrightness(physical.hPhysicalMonitor, &minimum, &current, &maximum)) {
        const double range = static_cast<double>(maximum) - static_cast<double>(minimum);
        actual = static_cast<DWORD>(std::lround(minimum + (brightness / 100.0) * range));
    } else {
        // The old 0..200 assumption, kept as the fallback it was.
        actual = static_cast<DWORD>(std::lround(brightness) * 2);
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (!panel->readyToChange) {
        const qint64 completeAt =
            panel->lastChangedMs + static_cast<qint64>(panel->changeDelaySec * 1000.0);
        if (now < completeAt) {
            // Still ramping. The panel would drop this command, so report the
            // wait instead of pretending it landed.
            *err = QStringLiteral("%1 is still ramping; %2 s left")
                       .arg(direction, QString::number((completeAt - now) / 1000.0, 'f', 2));
            DestroyPhysicalMonitor(physical.hPhysicalMonitor);
            return false;
        }
        panel->readyToChange = true;
    }

    const double difference = std::fabs(panel->brightness - brightness);
    panel->changeDelaySec = difference / 3.33;  // measured ramp rate, %/s

    const bool ok = SetMonitorBrightness(physical.hPhysicalMonitor, actual);
    DestroyPhysicalMonitor(physical.hPhysicalMonitor);

    if (!ok) {
        *err = QStringLiteral("SetMonitorBrightness failed for %1").arg(panel->displayNum);
        return false;
    }

    panel->brightness = brightness;
    if (difference > 0) {
        panel->readyToChange = false;
        panel->lastChangedMs = now;
    }
    return true;
#else
    // DDC/CI over dxva2 is a Windows API. The panels are wired to the Windows
    // rig; on any other platform the pattern still goes up and only the dimming
    // is unavailable, which is worth saying rather than silently succeeding.
    Q_UNUSED(brightness);
    *err = QStringLiteral("brightness control needs Windows (DDC/CI via dxva2)");
    return false;
#endif
}

QString MonitorDevice::Impl::getBrightnessOne(const QString &direction, QString *err) {
    Panel *panel = panelFor(direction);
    if (!panel) {
        *err = QStringLiteral("direction \"%1\" is not mapped to a display yet").arg(direction);
        return {};
    }

#ifdef _WIN32
    const QHash<QString, HMONITOR> handles = monitorHandles();
    const auto it = handles.constFind(panel->displayNum);
    if (it == handles.constEnd()) {
        *err = QStringLiteral("no monitor handle for %1").arg(panel->displayNum);
        return {};
    }

    PHYSICAL_MONITOR physical{};
    if (!GetPhysicalMonitorsFromHMONITOR(*it, 1, &physical)) {
        *err = QStringLiteral("GetPhysicalMonitorsFromHMONITOR failed for %1")
                   .arg(panel->displayNum);
        return {};
    }

    DWORD minimum = 0, current = 0, maximum = 0;
    const bool ok = GetMonitorBrightness(physical.hPhysicalMonitor, &minimum, &current, &maximum);
    DestroyPhysicalMonitor(physical.hPhysicalMonitor);

    if (!ok) {
        *err = QStringLiteral("GetMonitorBrightness failed for %1").arg(panel->displayNum);
        return {};
    }
    // Raw panel value, as the Python returned. It is the panel's own scale, not a
    // percentage -- callers that show it label it as the monitor reports it.
    return QString::number(current);
#else
    *err = QStringLiteral("brightness control needs Windows (DDC/CI via dxva2)");
    return {};
#endif
}

// ---------------------------------------------------------------------------

bool MonitorDevice::set_brightness(const QString &direction, double brightness) {
    QMutexLocker lock(&d_->mutex);
    bool all = true;
    for (const QString &target : d_->targets(direction.trimmed().toLower())) {
        QString err;
        if (d_->setBrightnessOne(target, brightness, &err)) continue;
        std::cerr << "[monitor] set_brightness: " << qUtf8Printable(err) << "\n";
        all = false;
    }
    return all;
}

bool MonitorDevice::set_brightness_all(double brightness) {
    return set_brightness(QStringLiteral("all"), brightness);
}

QString MonitorDevice::get_brightness(const QString &direction) {
    QMutexLocker lock(&d_->mutex);
    QString err;
    const QString value = d_->getBrightnessOne(direction.trimmed().toLower(), &err);
    if (!err.isEmpty()) std::cerr << "[monitor] get_brightness: " << qUtf8Printable(err) << "\n";
    return value;
}
