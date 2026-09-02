// The five calibration screens, driven by THIS process.
//
// This file is construction, the direction mapping, and the two diagnostics an
// operator reaches for when a pattern does not appear. The rest of the class is
// beside it -- see monitor_device_p.h for what is where and why.

#include "monitor_device_p.h"

#include <algorithm>
#include <iostream>

#include <QColor>
#include <QFont>
#include <QGuiApplication>
#include <QTimer>

using monitor_detail::kNumberingSeconds;
using monitor_detail::normaliseDisplay;
using monitor_detail::Panel;
using monitor_detail::PatternWindow;

MonitorDevice::MonitorDevice(DeviceConfig cfg, QObject *parent)
    : QObject(parent), d_(new Impl) {
    d_->cfg = std::move(cfg);
    {
        QMutexLocker lock(&d_->mutex);
        d_->collectPanels();
    }

    // Reuse the mapping the operator set last time, exactly as the monitor node
    // reloaded its saved one. Without this every launch starts with the patterns
    // refused until someone reopens the dialog.
    if (!d_->cfg.displayTop.isEmpty()) {
        QMutexLocker lock(&d_->mutex);
        const QString err = d_->applyMapping(d_->cfg.displayTop, d_->cfg.displayN,
                                             d_->cfg.displayW, d_->cfg.displayS,
                                             d_->cfg.displayE);
        if (!err.isEmpty())
            std::cerr << "[monitor] saved display mapping unusable: " << qUtf8Printable(err)
                      << "\n";
    } else {
        std::cerr << "[monitor] no saved display mapping -- directions are UNSET, so "
                     "patterns are refused until one is applied\n";
    }

    // A panel plugged in, unplugged or re-numbered while the app runs invalidates
    // both the enumeration and the mapping, and the mapping is by display number
    // so it can be reapplied.
    const auto rescan = [this] {
        QMutexLocker lock(&d_->mutex);
        d_->collectPanels();
        d_->applyMapping(d_->cfg.displayTop, d_->cfg.displayN, d_->cfg.displayW,
                         d_->cfg.displayS, d_->cfg.displayE);
    };
    connect(qApp, &QGuiApplication::screenAdded, this, rescan);
    connect(qApp, &QGuiApplication::screenRemoved, this, [rescan](QScreen *) { rescan(); });

    std::cerr << "[monitor] " << d_->panels.size() << " screen(s) detected\n";
}

MonitorDevice::~MonitorDevice() {
    for (PatternWindow *w : std::as_const(d_->windows)) delete w;
    d_->windows.clear();
}

// ------------------------------------------------------------------ mapping

QString MonitorDevice::show_display_number() {
    return d_->onGui([this]() -> QString {
        QMutexLocker lock(&d_->mutex);
        d_->collectPanels();
        if (d_->panels.isEmpty()) return QString();

        // Anything still up from a previous press is replaced, not stacked.
        d_->closeNumbering();

        // Which screens may be covered.
        //
        // Not all of them, which is what this used to do. The operator's own
        // monitors are screens like any other, so they got a full-screen overlay
        // too -- and the whole point of pressing this button is that the operator
        // is sitting there, reading numbers, about to type them into a dialog
        // that was also underneath.
        //
        // Once a mapping exists the app knows exactly which five screens are the
        // rig, so nothing else is touched. Before one exists it cannot know, so
        // it covers everything except the primary screen: that is where the
        // taskbar and the mapping dialog are, and leaving one screen alone is the
        // difference between an overlay and a machine that appears to have hung.
        const QStringList mapped{d_->cfg.displayTop, d_->cfg.displayN, d_->cfg.displayW,
                                 d_->cfg.displayS, d_->cfg.displayE};
        QStringList wanted;
        for (const QString &m : mapped)
            if (!m.trimmed().isEmpty()) wanted << normaliseDisplay(m);

        const QScreen *primary = QGuiApplication::primaryScreen();
        QStringList skipped;

        QStringList shown;
        for (Panel &panel : d_->panels) {
            const bool isRigPanel = wanted.contains(panel.displayNum);
            const bool isPrimary = primary && panel.screen == primary;
            if (wanted.isEmpty() ? isPrimary : !isRigPanel) {
                skipped << panel.displayNum;
                continue;
            }
            QImage image(panel.physical, QImage::Format_RGB888);
            image.fill(Qt::white);

            QPainter painter(&image);
            painter.setRenderHint(QPainter::TextAntialiasing);
            painter.setPen(Qt::black);

            // A tenth of the panel height, as the Python sized it -- readable
            // from across the room on a 1440p panel and on a 4K one alike.
            QFont font = painter.font();
            font.setPixelSize(std::max(16, panel.physical.height() / 10));
            font.setBold(true);
            painter.setFont(font);
            painter.drawText(image.rect(), Qt::AlignCenter, panel.displayNum);

            // The way out, written on the thing that is in the way. Without it
            // the overlay looks exactly like a machine that has locked up, and
            // the operator's next move is the power button.
            QFont hint = painter.font();
            hint.setPixelSize(std::max(12, panel.physical.height() / 36));
            hint.setBold(false);
            painter.setFont(hint);
            painter.setPen(QColor(90, 90, 90));
            painter.drawText(image.rect().adjusted(0, 0, 0, -panel.physical.height() / 12),
                             Qt::AlignHCenter | Qt::AlignBottom,
                             QObject::tr("click anywhere to close  --  closes itself in %1 s")
                                 .arg(kNumberingSeconds));
            painter.end();

            // Keyed by display number, not by direction: this runs BEFORE a
            // mapping exists, which is what it is for.
            PatternWindow *&window = d_->windows[panel.displayNum];
            if (!window) window = new PatternWindow;
            window->showOn(panel.screen, image);
            window->makeDismissible([this] { d_->closeNumbering(); });
            d_->numbering << panel.displayNum;

            shown << QStringLiteral("%1 (%2x%3)")
                         .arg(panel.displayNum)
                         .arg(panel.physical.width())
                         .arg(panel.physical.height());
        }

        // The backstop, for the case a click cannot reach them: the numbers go
        // away on their own. A press that covers every screen with no way back
        // is not a feature an operator can be asked to be careful around.
        QTimer::singleShot(kNumberingSeconds * 1000, this, [this] {
            QMutexLocker lock(&d_->mutex);
            d_->closeNumbering();
        });

        // The skipped ones are named rather than passed over in silence: an
        // operator looking for a number that never appeared needs to be told the
        // screen was left alone on purpose, not that it went missing.
        QString answer = shown.join(QStringLiteral(", "));
        if (!skipped.isEmpty())
            answer += QStringLiteral("  [left alone: %1]").arg(skipped.join(QStringLiteral(", ")));
        return answer;
    });
}

QString MonitorDevice::set_display_direction(const QString &top, const QString &n,
                                             const QString &w, const QString &s,
                                             const QString &e) {
    return d_->onGui([this, top, n, w, s, e]() -> QString {
        QMutexLocker lock(&d_->mutex);
        d_->collectPanels();
        const QString err = d_->applyMapping(top, n, w, s, e);
        if (!err.isEmpty()) return err;

        d_->cfg.displayTop = normaliseDisplay(top);
        d_->cfg.displayN = normaliseDisplay(n);
        d_->cfg.displayW = normaliseDisplay(w);
        d_->cfg.displayS = normaliseDisplay(s);
        d_->cfg.displayE = normaliseDisplay(e);
        if (!d_->cfg.save())
            return QStringLiteral("mapping applied, but could not be saved to %1")
                .arg(DeviceConfig::path());

        // The numbering windows have done their job and would otherwise sit on
        // top of the patterns that are about to be pushed.
        for (auto it = d_->windows.begin(); it != d_->windows.end();) {
            if (it.key().startsWith(QLatin1String("DISPLAY"))) {
                delete it.value();
                it = d_->windows.erase(it);
            } else {
                ++it;
            }
        }

        return QStringLiteral("top=%1 n=%2 w=%3 s=%4 e=%5")
            .arg(d_->cfg.displayTop, d_->cfg.displayN, d_->cfg.displayW, d_->cfg.displayS,
                 d_->cfg.displayE);
    });
}

QString MonitorDevice::get_display_direction(const QString &direction) {
    return d_->onGui([this, direction]() -> QString {
        QMutexLocker lock(&d_->mutex);
        const Panel *panel = d_->panelFor(direction.trimmed().toLower());
        return panel ? panel->displayNum : QString();
    });
}

// -------------------------------------------------------------- diagnostics

QStringList MonitorDevice::describeScreens() {
    return d_->onGui([this]() -> QStringList {
        QMutexLocker lock(&d_->mutex);
        QStringList out;
        for (const Panel &panel : d_->panels) {
            // Every direction on this panel, not just one. On a real rig that is
            // always a single direction, and seeing two is the point: it means
            // the mapping form has the same display number in two boxes, which
            // otherwise shows up much later as two directions that never differ.
            QStringList mapped;
            for (auto it = d_->byDirection.constBegin(); it != d_->byDirection.constEnd(); ++it)
                if (*it >= 0 && *it < d_->panels.size() &&
                    d_->panels.at(*it).displayNum == panel.displayNum)
                    mapped << it.key();
            mapped.sort();
            const QString mappedTo =
                mapped.isEmpty() ? QStringLiteral("-") : mapped.join(QLatin1Char('+'));

            // Both names: the number is what the mapping wants, the model is what
            // is printed on the bezel. Showing only one means the operator has to
            // work out the other before they can fill the dialog in.
            const QString label =
                (panel.model.isEmpty() || panel.model == panel.displayNum)
                    ? panel.displayNum
                    : QStringLiteral("%1 (%2)").arg(panel.displayNum, panel.model);

            out << QStringLiteral("%1 %2x%3 at (%4,%5) -> %6")
                       .arg(label)
                       .arg(panel.physical.width())
                       .arg(panel.physical.height())
                       .arg(panel.screen->geometry().x())
                       .arg(panel.screen->geometry().y())
                       .arg(mappedTo);
        }
        return out;
    });
}

// ------------------------------------------------------------------ naming

void MonitorDevice::setUrl(const QString &) {
    // There is no address any more; the screens are this machine's screens.
}

QString MonitorDevice::url() const {
    QMutexLocker lock(&d_->mutex);
    return QStringLiteral("%1 local screen(s)").arg(d_->panels.size());
}
