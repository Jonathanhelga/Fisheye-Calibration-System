// Which panels are attached, what each is called, and how many pixels it really
// has. Everything above this in MonitorDevice trusts the answers here, so when a
// pattern lands on the wrong screen or at the wrong size, this is the file.

#include "monitor_device_p.h"

#include <QDateTime>
#include <QGuiApplication>
#include <QList>
#include <QRect>

#include <opencv2/imgproc.hpp>

#ifdef _WIN32
// NOMINMAX before windows.h, or it defines min/max as MACROS and every
// std::max( in this file -- and in the Qt and OpenCV headers above -- becomes a
// syntax error. WIN32_LEAN_AND_MEAN keeps winsock and the rest of the shell API
// out of a translation unit that wants four display functions.
//
// Both are also set on the moil_common target in CMakeLists.txt; they are
// repeated here so this file cannot be built wrongly by a target that forgets
// them.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace monitor_detail {

QString normaliseDisplay(const QString &raw) {
    QString s = raw.trimmed();
    while (!s.isEmpty() && (s.at(0) == QLatin1Char('\\') || s.at(0) == QLatin1Char('.')))
        s.remove(0, 1);
    if (s.isEmpty()) return s;

    bool numeric = false;
    const int n = s.toInt(&numeric);
    if (numeric) return QStringLiteral("DISPLAY%1").arg(n);
    return s.toUpper();
}

#ifdef _WIN32

namespace {

// This exists because QScreen::name() is NOT the device name on Windows: Qt 6
// returns the monitor's model, and with five identical-ish panels that reads
// "EV2730Q", "EV2785 (1)", "EV2785 (2)"... Everything else in MonitorDevice is
// keyed by the device name -- monitorHandles() in monitor_brightness.cpp stores
// handles under szDevice, and physicalSize below builds "\\.\<name>" for
// EnumDisplaySettings -- so taking the name from Qt silently broke all three of
// them at once:
//
//   * the mapping never resolved. normaliseDisplay("5") is "DISPLAY5", the panel
//     was keyed "EV2730Q", so --map and the Setup Monitor Direction dialog (whose
//     spin boxes send bare numbers) both reported "display is not attached".
//   * brightness was permanently unavailable -- "no monitor handle for EV2730Q".
//   * physicalSize's EnumDisplaySettings always failed on "\\.\EV2730Q" and fell
//     back to geometry * devicePixelRatio, which happens to be right at 150%
//     scaling and is wrong at fractional scaling. That fallback being silent is
//     why the wrong pattern size ShowPatternSpec exists to prevent could come
//     back without anything saying so.
//
// Matched on the top-left corner: EnumDisplayMonitors and QScreen::geometry()
// both report virtual-desktop coordinates, so the corners coincide exactly.
struct MonitorRect {
    QString device;
    QRect rect;
};

BOOL CALLBACK collectMonitorRect(HMONITOR handle, HDC, LPRECT, LPARAM param) {
    Q_UNUSED(handle);
    MONITORINFOEXW info{};
    info.cbSize = sizeof(info);
    if (GetMonitorInfoW(handle, &info)) {
        auto *list = reinterpret_cast<QList<MonitorRect> *>(param);
        list->append({normaliseDisplay(QString::fromWCharArray(info.szDevice)),
                      QRect(info.rcMonitor.left, info.rcMonitor.top,
                            info.rcMonitor.right - info.rcMonitor.left,
                            info.rcMonitor.bottom - info.rcMonitor.top)});
    }
    return TRUE;
}

}  // namespace

QString deviceNameForScreen(QScreen *screen) {
    QList<MonitorRect> rects;
    EnumDisplayMonitors(nullptr, nullptr, collectMonitorRect, reinterpret_cast<LPARAM>(&rects));
    const QPoint corner = screen->geometry().topLeft();
    for (const MonitorRect &m : rects)
        if (m.rect.topLeft() == corner) return m.device;
    return {};  // caller falls back to Qt's name rather than losing the panel
}

QSize physicalSize(const QString &displayNum, QScreen *screen) {
    // The same thing the Python got from GetDeviceCaps(HORZRES/VERTRES).
    DEVMODEW mode{};
    mode.dmSize = sizeof(mode);
    const QString device = QStringLiteral("\\\\.\\") + displayNum;
    if (EnumDisplaySettingsW(reinterpret_cast<LPCWSTR>(device.utf16()), ENUM_CURRENT_SETTINGS,
                             &mode))
        return QSize(static_cast<int>(mode.dmPelsWidth), static_cast<int>(mode.dmPelsHeight));

    return screen->geometry().size() * screen->devicePixelRatio();
}

#else

QSize physicalSize(const QString &, QScreen *screen) {
    // Off Windows there is no EnumDisplaySettings; Qt's own ratio is the best
    // available answer and is correct whenever the desktop is not fractionally
    // scaled.
    return screen->geometry().size() * screen->devicePixelRatio();
}

// No \\.\DISPLAYn namespace off Windows, so Qt's name is the only name there is.
QString deviceNameForScreen(QScreen *) { return {}; }

#endif

QImage matToImage(const cv::Mat &mat) {
    if (mat.empty()) return {};
    if (mat.type() == CV_8UC3) {
        // Deep copy: the QImage outlives the Mat it was wrapped around.
        return QImage(mat.data, mat.cols, mat.rows, static_cast<int>(mat.step),
                      QImage::Format_BGR888)
            .copy();
    }
    if (mat.type() == CV_8UC1) {
        return QImage(mat.data, mat.cols, mat.rows, static_cast<int>(mat.step),
                      QImage::Format_Grayscale8)
            .copy();
    }
    cv::Mat bgr;
    cv::cvtColor(mat, bgr, cv::COLOR_BGRA2BGR);
    return matToImage(bgr);
}

}  // namespace monitor_detail

// ---------------------------------------------------------------------------
// The Impl methods that are about enumeration and about which panel is which.

void MonitorDevice::Impl::collectPanels() {
    panels.clear();
    for (QScreen *screen : QGuiApplication::screens()) {
        Panel p;
        p.model = monitor_detail::normaliseDisplay(screen->name());
        // The device name is what the handle map and EnumDisplaySettings are
        // keyed by; Qt's name is only a fallback for when the corner match
        // finds nothing, which is better than dropping the panel entirely.
        p.displayNum = monitor_detail::deviceNameForScreen(screen);
        if (p.displayNum.isEmpty()) p.displayNum = p.model;
        if (p.displayNum.isEmpty())
            p.displayNum = QStringLiteral("DISPLAY%1").arg(panels.size() + 1);
        p.screen = screen;
        p.physical = monitor_detail::physicalSize(p.displayNum, screen);
        p.lastChangedMs = QDateTime::currentMSecsSinceEpoch();
        panels.append(p);
    }
}

int MonitorDevice::Impl::indexOfDisplay(const QString &displayNum) const {
    const QString want = monitor_detail::normaliseDisplay(displayNum);
    for (int i = 0; i < panels.size(); ++i)
        if (panels.at(i).displayNum == want) return i;
    // A panel can also be named by its model. Mappings saved before the device
    // name was resolved correctly hold those, and re-mapping five panels by hand
    // is not a reasonable thing to ask on an upgrade.
    for (int i = 0; i < panels.size(); ++i)
        if (!panels.at(i).model.isEmpty() && panels.at(i).model == want) return i;
    return -1;
}

QString MonitorDevice::Impl::applyMapping(const QString &top, const QString &n, const QString &w,
                                          const QString &s, const QString &e) {
    const QString wanted[] = {top, n, w, s, e};
    QHash<QString, int> resolved;

    for (int i = 0; i < 5; ++i) {
        if (wanted[i].trimmed().isEmpty())
            return QStringLiteral("no display given for \"%1\"")
                .arg(QLatin1String(monitor_detail::kDirections[i]));
        const int index = indexOfDisplay(wanted[i]);
        if (index < 0)
            return QStringLiteral("display \"%1\" (for \"%2\") is not attached")
                .arg(wanted[i], QLatin1String(monitor_detail::kDirections[i]));
        resolved.insert(QLatin1String(monitor_detail::kDirections[i]), index);
    }

    byDirection = resolved;
    return QString();
}

MonitorDevice::Impl::Snapshot MonitorDevice::Impl::snapshotFor(MonitorDevice *owner,
                                                               const QString &direction) {
    QMutexLocker lock(&mutex);
    if (const Panel *panel = panelFor(direction))
        return {true, panel->displayNum, panel->physical};
    if (cfg.displayTop.isEmpty()) return {};
    lock.unlock();

    QMetaObject::invokeMethod(owner, [this] {
        QMutexLocker rescanLock(&mutex);
        collectPanels();
        applyMapping(cfg.displayTop, cfg.displayN, cfg.displayW, cfg.displayS, cfg.displayE);
    });
    return {};
}

void MonitorDevice::Impl::presentAsync(MonitorDevice *owner, const QString &direction,
                                       const QImage &image) {
    QMetaObject::invokeMethod(owner, [this, direction, image] {
        QScreen *screen = nullptr;
        {
            QMutexLocker lock(&mutex);
            // Re-resolved here, on the GUI thread, at the moment of use.
            if (const Panel *panel = panelFor(direction)) screen = panel->screen;
        }
        if (!screen) return;  // unplugged between the render and now
        windowFor(direction)->showOn(screen, image);
    });
}
