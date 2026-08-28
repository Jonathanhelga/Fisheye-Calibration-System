#pragma once

#include <QString>

class QWidget;
class QPushButton;

// A "HELP" button that opens the window's Sphinx doc page (<page>.html) in the
// system default browser.
namespace Help {

// Full URL of the docs page for `page` (falls back to the docs landing page).
QString url(const QString &page);

// Open the docs page for `page` in the user's browser. Tries the Qt desktop
// integration first, then platform openers, and on WSL hands the URL to the
// Windows host (a WSL guest normally has no browser at all). Returns false only
// when every strategy failed; the user is then shown the URL to copy.
bool open(const QString &page, QWidget *parent = nullptr);

// Open an arbitrary URL with the same strategy chain open() uses. Any window
// that hands a link to the browser should go through this rather than
// QDesktopServices::openUrl directly: under WSL that call only warns "Unable to
// detect a web browser" and the link is lost, because a WSL distro has no
// browser and no xdg-open. Returns false only when every strategy failed; the
// URL is then copied to the clipboard and shown to the user.
bool openLink(const QString &link, QWidget *parent = nullptr);

// Create a ready-wired HELP button (text, style, click -> open page). The
// caller owns placement (e.g. into a menu-bar corner). `page` is the base name
// of the built HTML page, without extension (e.g. "main_window", "database").
QPushButton *makeButton(QWidget *parent, const QString &page);

// Attach a HELP button pinned to the top-right corner of `window` as a floating
// overlay (for windows without a menu bar to host it).
void attach(QWidget *window, const QString &page);

}  // namespace Help
