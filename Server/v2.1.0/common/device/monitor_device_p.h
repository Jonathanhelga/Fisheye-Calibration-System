#pragma once

// Internals of MonitorDevice, shared by the four translation units that
// implement it. NOT part of the public interface -- monitor_device.h is, and
// nothing outside device/ should include this.
//
// The implementation was one 1200-line file. It is four now, split by the thing
// each part actually talks to:
//
//   monitor_screens.cpp     Windows and Qt: which panels exist, what device name
//                           each has, and what its real pixel grid is
//   monitor_brightness.cpp  DDC/CI over dxva2
//   monitor_patterns.cpp    what goes on the glass, live and prepared
//   monitor_device.cpp      construction, the direction mapping, and diagnostics
//
// That is also the order in which they fail: a pattern that does not appear is
// almost always an enumeration or mapping problem rather than a rendering one,
// and having them apart makes the first two readable without the other two.

#include <functional>

#include <QCoreApplication>
#include <QHash>
#include <QImage>
#include <QList>
#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QThread>
#include <QWidget>
#include <QtGlobal>

#include <opencv2/core.hpp>

#include "device_config.h"
#include "monitor_device.h"

// Deliberately no windows.h here. Only the brightness and enumeration units need
// it, and pulling it into a header that four files include would put its macros
// in front of every one of them -- which is the class of problem the NOMINMAX
// note in those files exists to describe.

namespace monitor_detail {

inline constexpr const char *const kDirections[] = {"top", "n", "w", "s", "e"};

// How long the display-numbering overlays stay up on their own. Long enough to
// walk round the rig and read five panels, short enough that a press that turns
// out to have been a mistake is over before it becomes a problem.
inline constexpr int kNumberingSeconds = 30;

// "1" / "display1" / "\\.\DISPLAY1" all mean the same panel. The spin boxes in
// the mapping dialog send bare numbers, Windows reports "\\.\DISPLAY1", and the
// Python normalised with lstrip("\\.\\"); this is the one place all three meet.
QString normaliseDisplay(const QString &raw);

// The device name ("DISPLAY5") for the screen Qt is describing, or "" when it
// cannot be determined -- the caller then falls back to Qt's own name rather
// than losing the panel. See the long note in monitor_screens.cpp for why
// QScreen::name() is not this.
QString deviceNameForScreen(QScreen *screen);

// Physical pixel grid, with any Windows display scaling undone. A pattern drawn
// at the scaled size and stretched onto the panel has the wrong physical
// dimensions, which is the failure ShowPatternSpec exists to remove.
QSize physicalSize(const QString &displayNum, QScreen *screen);

QImage matToImage(const cv::Mat &mat);

// A pattern on one screen.
//
// Frameless, always-on-top, no focus. Not showFullScreen(): that asks the window
// manager for the screen the window is currently on, and the point here is to
// choose the screen. setGeometry() to the screen's own rectangle does exactly
// what is asked on all five panels.
class PatternWindow : public QWidget {
public:
    PatternWindow()
        : QWidget(nullptr, Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint) {
        setAttribute(Qt::WA_ShowWithoutActivating);
        setAttribute(Qt::WA_OpaquePaintEvent);
        setCursor(Qt::BlankCursor);
        // The glass is measured against this; a stray palette gradient behind a
        // pattern that does not cover the whole panel would be measured too.
        QPalette pal = palette();
        pal.setColor(QPalette::Window, Qt::black);
        setPalette(pal);
        setAutoFillBackground(true);
    }

    // `image` is in PHYSICAL pixels. Tagging it with the screen's device pixel
    // ratio makes Qt blit it 1:1 instead of resampling it through the logical
    // coordinate system -- on a scaled desktop that resampling is the difference
    // between a ring that is 40 px wide and one that is 50.
    void showOn(QScreen *screen, QImage image) {
        screen_ = screen;
        image_ = std::move(image);
        image_.setDevicePixelRatio(screen->devicePixelRatio());

        setScreen(screen);
        setGeometry(screen->geometry());
        show();
        raise();
        update();
    }

    // Turns this into a transient identification overlay rather than a
    // calibration pattern, and the difference matters a great deal to whoever is
    // sitting at the machine.
    //
    // A pattern window is frameless, always-on-top, never focused, and hides the
    // cursor. On the glass every one of those is right. On the operator's own
    // desktop they combine into a window with no title bar to drag, no close
    // button, no keyboard focus for Alt+F4 to land on, and no visible pointer --
    // and show_display_number() puts one on EVERY screen, including the two the
    // operator works from and the mapping dialog they were about to type into.
    // Nothing then closed them: close_pattern_all() would have, but its button
    // was underneath. The machine had to be rescued through Task Manager.
    //
    // So an overlay gets its cursor back and dies on any click or key.
    void makeDismissible(std::function<void()> onDismiss) {
        onDismiss_ = std::move(onDismiss);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::StrongFocus);
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.fillRect(rect(), Qt::black);
        if (image_.isNull()) return;
        painter.drawImage(QPointF(0, 0), image_);
    }

    void mousePressEvent(QMouseEvent *event) override {
        if (onDismiss_) onDismiss_();
        else QWidget::mousePressEvent(event);
    }

    void keyPressEvent(QKeyEvent *event) override {
        if (onDismiss_) onDismiss_();
        else QWidget::keyPressEvent(event);
    }

private:
    QScreen *screen_ = nullptr;
    QImage image_;
    // Empty for a calibration pattern: those are dismissed by the app, never by
    // a stray click on the glass.
    std::function<void()> onDismiss_;
};

// What is known about one attached panel.
struct Panel {
    QString displayNum;      // "DISPLAY1" -- the Windows device name, always
    // What Qt calls the screen: the monitor model on Windows ("EV2730Q"), which
    // is what the operator reads off the panel and what earlier saved mappings
    // contain, so it stays accepted as a way to name a panel.
    QString model;
    QScreen *screen = nullptr;
    QSize physical;          // real pixel grid, Windows scaling undone

    // DDC/CI pacing, ported from monitor_module_autodetect. A panel that is sent
    // a second brightness command while it is still ramping to the first simply
    // ignores it, so the module refused the write and reported how long was left.
    double brightness = 100.0;
    bool readyToChange = true;
    double changeDelaySec = 0.0;
    qint64 lastChangedMs = 0;
};

}  // namespace monitor_detail

struct MonitorDevice::Impl {
    using Panel = monitor_detail::Panel;
    using PatternWindow = monitor_detail::PatternWindow;

    DeviceConfig cfg;

    QList<Panel> panels;
    QHash<QString, int> byDirection;  // direction -> index into panels
    QHash<QString, PatternWindow *> windows;

    // Which of those windows are the temporary numbering overlays. Kept apart
    // from the calibration patterns so dismissing the numbers cannot take a
    // pattern off the glass mid-measurement.
    QStringList numbering;

    // Takes the numbering overlays down. Safe to call from inside one of their
    // own event handlers: deleteLater, not delete, because a widget cannot be
    // destroyed while it is dispatching the click that asked for it.
    void closeNumbering() {
        for (const QString &key : std::as_const(numbering)) {
            const auto it = windows.find(key);
            if (it == windows.end()) continue;
            it.value()->hide();
            it.value()->deleteLater();
            windows.erase(it);
        }
        numbering.clear();
    }

    // Enumerate the attached screens. Called at construction and again whenever a
    // direction cannot be resolved, because a panel that was still waking up at
    // launch would otherwise stay missing for the session.
    void collectPanels();

    int indexOfDisplay(const QString &displayNum) const;

    Panel *panelFor(const QString &direction) {
        const auto it = byDirection.constFind(direction);
        if (it == byDirection.constEnd()) return nullptr;
        if (*it < 0 || *it >= panels.size()) return nullptr;
        return &panels[*it];
    }

    // Apply a mapping already held as five display numbers. Returns "" on success
    // or the reason it could not be applied.
    QString applyMapping(const QString &top, const QString &n, const QString &w,
                         const QString &s, const QString &e);

    PatternWindow *windowFor(const QString &direction) {
        PatternWindow *&w = windows[direction];
        if (!w) w = new PatternWindow;
        return w;
    }

    // What a worker thread is allowed to know about a panel: values, no pointers.
    // A QScreen* handed to another thread can be dangling by the time it is used
    // -- the panel it names may have been unplugged -- so the direction is
    // re-resolved on the GUI thread at the moment the window is actually touched.
    struct Snapshot {
        bool mapped = false;
        QString displayNum;
        QSize physical;
    };

    // Thread-safe, and deliberately NON-blocking.
    //
    // When the direction is unmapped it schedules a re-enumeration and reports
    // unmapped for THIS push, rather than waiting on the GUI thread for a rescan.
    // Waiting here would put the deadlock back: a worker blocked on the GUI thread
    // while the GUI thread is in ControllerMonitor's teardown waiting on that same
    // worker. The cost of not waiting is that the first push after a panel wakes
    // up fails and the next succeeds -- and even that is rare, because
    // QGuiApplication::screenAdded already triggers the rescan on its own.
    Snapshot snapshotFor(MonitorDevice *owner, const QString &direction);

    // Put an image on the panel a direction is mapped to, WITHOUT waiting for it.
    //
    // Queued, not blocking, and this is the whole reason the class is shaped this
    // way. ControllerMonitor's destructor runs on the GUI thread and waits for the
    // in-flight pushes to finish; if those pushes were themselves waiting on the
    // GUI thread, closing the window -- or quitting the app -- while a pattern was
    // being sent to five screens would hang forever. Under ROS this could not
    // happen, because the push went out over DDS and never needed the GUI thread.
    //
    // `owner` as the context object also makes it lifetime-safe: a queued call to
    // a destroyed QObject is dropped rather than run against freed windows.
    void presentAsync(MonitorDevice *owner, const QString &direction, const QImage &image);

    // Directions a request applies to: one, or all five.
    QStringList targets(const QString &direction) const {
        if (direction.compare(QLatin1String("all"), Qt::CaseInsensitive) == 0) {
            QStringList all;
            for (const char *d : monitor_detail::kDirections) all << QLatin1String(d);
            return all;
        }
        return {direction};
    }

    bool setBrightnessOne(const QString &direction, double brightness, QString *err);
    QString getBrightnessOne(const QString &direction, QString *err);

    // Guards `panels` and `byDirection`, which the GUI thread rebuilds when a
    // screen is plugged in or removed and worker threads read on every push.
    QMutex mutex;

    // Serialises rendering. Five directions render concurrently now that the work
    // is done on the caller's thread, and the pattern generator was only ever
    // exercised one call at a time -- when it ran on the rig, the node serialised
    // it. Cheap insurance: the renders were sequential before this change too, so
    // nothing is lost by keeping them so.
    QMutex renderMutex;

    // Run `f` on the GUI thread and wait.
    //
    // Only for the dialog-driven calls -- the mapping form and the diagnostics --
    // which are already ON the GUI thread and therefore take the direct path
    // without ever blocking. The pattern and brightness paths, which DO run on
    // worker threads, deliberately do not use this; see presentAsync().
    template <typename F>
    auto onGui(F &&f) -> decltype(f()) {
        using R = decltype(f());
        QObject *gui = qApp;
        if (QThread::currentThread() == gui->thread()) return f();
        R out{};
        QMetaObject::invokeMethod(gui, std::forward<F>(f), Qt::BlockingQueuedConnection, &out);
        return out;
    }
};
