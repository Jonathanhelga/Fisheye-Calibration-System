#include "Help.h"

#include <QClipboard>
#include <QDesktopServices>
#include <QEvent>
#include <QFile>
#include <QGuiApplication>
#include <QHash>
#include <QLayout>
#include <QMainWindow>
#include <QMenuBar>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QStandardPaths>
#include <QUrl>

#include <utility>

namespace {

constexpr int kMargin = 8;

// objectName of the HELP button, so a stylesheet can target it.
const QString kHelpButtonName = QStringLiteral("helpButton");

// Base of the published documentation site (GitHub Pages). Bump the version
// segment here when the docs site publishes a new version.
const QString kDocsBase =
    QStringLiteral("https://perseverance-tech-tw.github.io/moilcalib_documentation/docs/v2.0");

// Window key (used by the call sites) -> path of the page on the docs site.
// Keys that are not listed fall back to the docs landing page.
const QHash<QString, QString> &pageRoutes() {
    static const QHash<QString, QString> routes = {
        {QStringLiteral("main_window"), QStringLiteral("system-overview/main-window")},
        {QStringLiteral("pattern_generator"), QStringLiteral("calibration/pct-pattern-generator")},
        {QStringLiteral("monitor_viewer"), QStringLiteral("calibration/monitor-viewer")},
        {QStringLiteral("captured_image"), QStringLiteral("calibration/camera-calibration")},
        {QStringLiteral("cali_result"), QStringLiteral("calibration/cali-result")},
        {QStringLiteral("curve_color"), QStringLiteral("system-overview/main-window")},
        {QStringLiteral("center_setup"), QStringLiteral("verification/setup-center")},
        {QStringLiteral("measure3d"), QStringLiteral("verification/3d-verification")},
        {QStringLiteral("database"), QStringLiteral("database/database-overview")},
    };
    return routes;
}

// Launch `exe args...` detached. `extraDirs` are searched after PATH, for
// interpreters that live outside it (e.g. the Windows tools seen from WSL).
// Returns false when the program is missing or refuses to start.
bool startDetached(const QString &exe, const QStringList &args,
                   const QStringList &extraDirs = {}) {
    QString path = QStandardPaths::findExecutable(exe);
    if (path.isEmpty() && !extraDirs.isEmpty())
        path = QStandardPaths::findExecutable(exe, extraDirs);
    if (path.isEmpty()) return false;
    return QProcess::startDetached(path, args);
}

#if defined(Q_OS_LINUX)

bool underWsl() {
    static const bool yes = [] {
        if (qEnvironmentVariableIsSet("WSL_DISTRO_NAME") ||
            qEnvironmentVariableIsSet("WSL_INTEROP") ||
            qEnvironmentVariableIsSet("WSLENV"))
            return true;
        QFile version(QStringLiteral("/proc/version"));
        return version.open(QIODevice::ReadOnly) &&
               QString::fromLatin1(version.readAll())
                   .contains(QLatin1String("microsoft"), Qt::CaseInsensitive);
    }();
    return yes;
}

// Hand the URL to the Windows host. A WSL distro usually ships no browser and
// no xdg-open, so this - not the Linux side - is where the docs must open.
bool openViaWindowsHost(const QString &link) {
    // The Windows tools are on PATH only when interop keeps the Windows PATH
    // appended; look them up directly as well.
    static const QStringList winDirs = {
        QStringLiteral("/mnt/c/Windows"),
        QStringLiteral("/mnt/c/Windows/System32"),
    };

    // wslu's browser shim: the cleanest option when it is installed.
    if (startDetached(QStringLiteral("wslview"), {link})) return true;

    // explorer.exe hands the URL to the Windows default browser. It exits with
    // a non-zero code even on success, which is why we start it detached.
    if (startDetached(QStringLiteral("explorer.exe"), {link}, winDirs)) return true;

    if (startDetached(QStringLiteral("powershell.exe"),
                      {QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive"),
                       QStringLiteral("-Command"),
                       QStringLiteral("Start-Process '%1'").arg(QString(link).replace(
                           QLatin1Char('\''), QLatin1String("''")))},
                      winDirs))
        return true;

    // cmd.exe's `start` treats '&' as a command separator, and its first quoted
    // argument as a window title - hence the empty "" placeholder.
    QString escaped = link;
    escaped.replace(QLatin1Char('&'), QLatin1String("^&"));
    return startDetached(QStringLiteral("cmd.exe"),
                         {QStringLiteral("/c"), QStringLiteral("start"),
                          QStringLiteral(""), escaped},
                         winDirs);
}

#endif  // Q_OS_LINUX

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)

// Desktop-portal openers first, then the browsers themselves, so a machine with
// a browser but no working xdg-open still opens the docs.
bool openViaUnixTools(const QString &link) {
    static const std::pair<const char *, const char *> openers[] = {
        {"xdg-open", nullptr},   {"gio", "open"},
        {"gvfs-open", nullptr},  {"kde-open", nullptr},
        {"kde-open5", nullptr},  {"gnome-open", nullptr},
        {"sensible-browser", nullptr}, {"x-www-browser", nullptr},
        {"firefox", nullptr},    {"chromium", nullptr},
        {"chromium-browser", nullptr}, {"google-chrome", nullptr},
        {"google-chrome-stable", nullptr}, {"microsoft-edge", nullptr},
        {"brave-browser", nullptr},
    };

    // $BROWSER wins when the user has set it.
    const QString browser = qEnvironmentVariable("BROWSER");
    if (!browser.isEmpty() && startDetached(browser, {link})) return true;

    for (const auto &[exe, verb] : openers) {
        QStringList args;
        if (verb) args << QLatin1String(verb);
        args << link;
        if (startDetached(QLatin1String(exe), args)) return true;
    }
    return false;
}

#endif  // Q_OS_UNIX && !Q_OS_MACOS

// Last resort: nothing could be launched, so put the URL where the user can
// still get at it.
void showManualFallback(QWidget *parent, const QString &link, const QString &title) {
    if (auto *clipboard = QGuiApplication::clipboard())
        clipboard->setText(link);

    QMessageBox box(parent);
    box.setIcon(QMessageBox::Information);
    box.setWindowTitle(title);
    box.setText(QObject::tr("No web browser could be launched on this machine.\n"
                            "The address has been copied to the clipboard:"));
    box.setInformativeText(link);
    box.setTextInteractionFlags(Qt::TextSelectableByMouse);
    box.exec();
}

// Every way we know of to get a URL in front of the user, in order. Shared by
// the HELP button and by any window opening a link (e.g. the Database window's
// SharePoint files), so a fix here reaches all of them.
bool openWithFallbacks(const QString &link, QWidget *parent, const QString &title) {
#if defined(Q_OS_LINUX)
    // Do this before QDesktopServices: under WSL the Qt path only warns
    // "Unable to detect a web browser" and there is nothing to fall back to.
    if (underWsl() && openViaWindowsHost(link)) return true;
#endif

    if (QDesktopServices::openUrl(QUrl(link))) return true;

#if defined(Q_OS_MACOS)
    if (startDetached(QStringLiteral("open"), {link})) return true;
#elif defined(Q_OS_UNIX)
    if (openViaUnixTools(link)) return true;
#endif

    showManualFallback(parent, link, title);
    return false;
}

// Keeps a Help button pinned to the top-right corner of its host window as the
// window is shown/resized (below the menu bar when there is one).
class HelpButtonFilter : public QObject {
public:
    HelpButtonFilter(QPushButton *btn, QWidget *host)
        : QObject(host), btn_(btn), host_(host) {}

    void reposition() {
        int top = kMargin;
        if (auto *mw = qobject_cast<QMainWindow *>(host_))
            if (mw->menuBar() && mw->menuBar()->isVisible())
                top += mw->menuBar()->height();
        btn_->move(host_->width() - btn_->width() - kMargin, top);
        btn_->raise();
    }

protected:
    bool eventFilter(QObject *o, QEvent *e) override {
        if (o == host_ && (e->type() == QEvent::Resize || e->type() == QEvent::Show))
            reposition();
        return QObject::eventFilter(o, e);
    }

private:
    QPushButton *btn_;
    QWidget *host_;
};

// Is `w` placed by a layout, at any nesting depth below its parent? A form that
// puts its HELP button in a layout (mainwindow_main.ui) has already said where
// it goes. One that pins it by geometry instead (center_setup.ui) has not: uic
// creates such a free-floating child *before* the widgets it overlays, which
// leaves it at the bottom of the sibling stack -- visible through their
// transparent backgrounds, but with every click going to whatever sits on top.
bool managedByLayout(QWidget *w) {
    QWidget *parent = w->parentWidget();
    if (!parent || !parent->layout()) return false;
    QList<QLayout *> todo{parent->layout()};
    while (!todo.isEmpty()) {
        QLayout *l = todo.takeFirst();
        for (int i = 0; i < l->count(); ++i) {
            QLayoutItem *item = l->itemAt(i);
            if (item->widget() == w) return true;
            if (item->layout()) todo.append(item->layout());
        }
    }
    return false;
}

}  // namespace

namespace Help {

QString url(const QString &page) {
    const QString path = pageRoutes().value(page, QStringLiteral("intro"));
    return kDocsBase + QLatin1Char('/') + path;
}

bool open(const QString &page, QWidget *parent) {
    return openWithFallbacks(url(page), parent, QObject::tr("Online help"));
}

bool openLink(const QString &link, QWidget *parent) {
    return openWithFallbacks(link, parent, QObject::tr("Open link"));
}

QPushButton *makeButton(QWidget *parent, const QString &page) {
    // A form may already declare its own HELP button (center_setup.ui does), in
    // which case reuse it so the window does not end up with two. Every other
    // window has no such widget and gets a freshly created one.
    auto *btn = parent->findChild<QPushButton *>(kHelpButtonName);
    if (!btn) {
        btn = new QPushButton(parent);
        btn->setObjectName(kHelpButtonName);
    }
    if (btn->text().isEmpty()) btn->setText(QObject::tr("HELP"));
    if (btn->toolTip().isEmpty())
        btn->setToolTip(QObject::tr("Open the online help for this window"));
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFocusPolicy(Qt::NoFocus);

    QObject::connect(btn, &QPushButton::clicked, parent,
                     [page, btn] { open(page, btn->window()); });
    return btn;
}

void attach(QWidget *window, const QString &page) {
    if (!window) return;

    // A button that came from the .ui is wired up, never reparented. Whether it
    // also needs placing depends on how the form put it there: a layout-managed
    // one is already where it belongs, while a geometry-pinned one is a plain
    // overlay child that nothing raises and nothing moves when the window is
    // resized -- so it gets the same filter a generated button would get.
    if (window->findChild<QPushButton *>(kHelpButtonName)) {
        auto *btn = makeButton(window, page);
        if (!managedByLayout(btn)) {
            btn->adjustSize();  // the form's width predates the theme's padding
            auto *filter = new HelpButtonFilter(btn, window);
            window->installEventFilter(filter);
            filter->reposition();  // raises it, and pins it to the top-right corner
        }
        btn->show();
        return;
    }

    // If the window already has a menu bar (e.g. File / Control), dock the HELP
    // button into its top-right corner so it sits on the top bar instead of
    // floating over the content.
    if (auto *mw = qobject_cast<QMainWindow *>(window))
        if (auto *mb = qobject_cast<QMenuBar *>(mw->menuWidget()))
            if (!mb->cornerWidget(Qt::TopRightCorner)) {
                auto *btn = makeButton(window, page);
                mb->setCornerWidget(btn, Qt::TopRightCorner);
                btn->show();
                return;
            }

    // Otherwise float it in the top-right corner as an overlay.
    auto *btn = makeButton(window, page);
    btn->adjustSize();
    auto *filter = new HelpButtonFilter(btn, window);
    window->installEventFilter(filter);
    filter->reposition();
    btn->show();
}

}  // namespace Help
