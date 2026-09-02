// What goes on the glass.
//
// Two ways in, and the difference is which side knows the panel's resolution:
//
//   show_pattern()       the caller sends encoded bytes. Whatever size they are
//                        is the size that is shown.
//   show_pattern_spec()  the caller sends a JSON description and the pattern is
//                        rendered HERE, at the panel's own pixel grid. This is
//                        the path that cannot get the physical size wrong.
//
// And prepared patterns, which are the spec path run once and kept as files --
// see PreparePatterns.srv for why a calibration round should not re-render.

#include "monitor_device_p.h"

#include <iostream>
#include <vector>

#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVector>

#include <opencv2/imgcodecs.hpp>

#include "ComputeOps.h"

using monitor_detail::matToImage;

// ---------------------------------------------------------------- live push --

bool MonitorDevice::show_pattern(const QString &direction, const QByteArray &image) {
    // Decoded on the caller's thread: it is the expensive part, the caller is
    // already a worker, and it needs nothing from the GUI.
    const std::vector<uchar> buffer(image.begin(), image.end());
    const cv::Mat decoded = cv::imdecode(buffer, cv::IMREAD_COLOR);
    const QImage picture = matToImage(decoded);
    if (picture.isNull()) {
        std::cerr << "[monitor] show_pattern: bytes are not a decodable image\n";
        return false;
    }

    bool all = true;
    for (const QString &target : d_->targets(direction.trimmed().toLower())) {
        const Impl::Snapshot panel = d_->snapshotFor(this, target);
        if (!panel.mapped) {
            std::cerr << "[monitor] show_pattern: direction \"" << qUtf8Printable(target)
                      << "\" is not mapped to a display yet -- run the display mapping "
                         "first\n";
            all = false;
            continue;
        }
        d_->presentAsync(this, target, picture);
    }
    return all;
}

bool MonitorDevice::show_pattern_all(const QByteArray &image) {
    return show_pattern(QStringLiteral("all"), image);
}

QImage MonitorDevice::renderForDirection(const QString &direction, const QString &spec,
                                         QString *err) {
    const Impl::Snapshot panel = d_->snapshotFor(this, direction.trimmed().toLower());
    if (!panel.mapped) {
        if (err)
            *err = QStringLiteral("direction \"%1\" is not mapped to a display yet")
                       .arg(direction);
        return {};
    }

    // At the panel's OWN grid. This is the whole point of the spec path: the
    // caller does not have to know, and used to get wrong, how many pixels each
    // screen has. Runs on the caller's thread -- rendering a 4K pattern is the
    // expensive half of a push, and doing it on the GUI thread is what made the
    // window freeze while five screens were updated.
    QMutexLocker lock(&d_->renderMutex);
    QString renderErr;
    const cv::Mat rendered = ComputeOps::renderPattern(spec, panel.physical.width(),
                                                       panel.physical.height(), &renderErr);
    if (rendered.empty()) {
        if (err) *err = renderErr;
        return {};
    }
    return matToImage(rendered);
}

bool MonitorDevice::show_pattern_spec(const QString &direction, const QString &spec) {
    bool all = true;
    for (const QString &target : d_->targets(direction.trimmed().toLower())) {
        QString err;
        const QImage picture = renderForDirection(target, spec, &err);
        if (picture.isNull()) {
            // Nothing has touched the glass, so the previous pattern is still up
            // -- worth saying, because the operator is looking at one.
            std::cerr << "[monitor] show_pattern_spec rejected (screen unchanged): "
                      << qUtf8Printable(err) << "\n";
            all = false;
            continue;
        }
        d_->presentAsync(this, target, picture);
    }
    return all;
}

// ---- prepared patterns -----------------------------------------------------

namespace {

// With the SERVER's own files, beside config/ -- the same rule
// DeviceConfig::bundledPath() uses for devices.json.
//
// These are not the operator's documents and they are not per-user: they are
// this server's current patterns, regenerated whenever Update to Monitor is
// pressed, and they belong to the installation rather than to whoever happens to
// be logged in. Putting them under the home directory made them look like
// something a person owns and had to be hunted for; here they sit next to the
// binary that writes them.
QString preparedRoot() {
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("patterns"));
}

// Colour a pattern's layers for one polarity.
//
// Ports applyPosNegConcentric/applyPosNegStripeline, which used to run in the
// client over its colour buttons: layer 1 takes `odd`, layer 2 takes `even`,
// alternating down the stack. Inverting the pair is what makes the negative
// pattern the negative of the positive one.
//
// Everything else in the spec -- shapes, radii, intervals, crossline, the 0x0
// that means "the panel decides" -- is passed through untouched. This changes
// colours, nothing else.
QString colourSpec(const QString &specJson, const QColor &odd, const QColor &even, QString *err) {
    QJsonParseError pe{};
    const QJsonDocument doc = QJsonDocument::fromJson(specJson.toUtf8(), &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        if (err) *err = QStringLiteral("pattern spec is not JSON: %1").arg(pe.errorString());
        return {};
    }
    QJsonObject root = doc.object();
    QJsonArray layers = root.value(QStringLiteral("layers")).toArray();
    if (layers.isEmpty()) {
        if (err) *err = QStringLiteral("pattern spec has no layers");
        return {};
    }

    const auto rgbOf = [](const QColor &c) {
        QJsonArray a;
        a.append(c.red());
        a.append(c.green());
        a.append(c.blue());
        return a;
    };

    for (int i = 0; i < layers.size(); ++i) {
        QJsonObject layer = layers.at(i).toObject();
        // 1-based like the client's btn_color_<n>, so layer 1 is "odd".
        layer[QStringLiteral("rgb")] = rgbOf(((i + 1) % 2 == 1) ? odd : even);
        layers.replace(i, layer);
    }
    root[QStringLiteral("layers")] = layers;
    if (err) err->clear();
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

}  // namespace

QString MonitorDevice::preparedDir() const { return preparedRoot(); }

bool MonitorDevice::prepare_patterns(const QString &specConcentric, const QString &specStripeline,
                                     const QColor &positive, const QColor &negative,
                                     QStringList *prepared, QVector<QSize> *sizes, QString *err) {
    QDir().mkpath(preparedRoot());

    // Which panel each pattern is FOR decides the pixel grid it is rendered at,
    // and that is the reason this cannot be done on a client: the top panel is
    // 1920x1920 and the sides are 1440x2560, and a pattern rendered at the wrong
    // grid is silently stretched -- which changes the physical size of the rings
    // on the glass without ever looking like an error.
    struct Job {
        const QString &spec;
        const char *name;
        QString direction;  // the panel whose resolution to use
        bool invert;        // negative = the pair swapped
    };
    const QVector<Job> jobs = {
        {specConcentric, "concentric_positive", QStringLiteral("top"), false},
        {specConcentric, "concentric_negative", QStringLiteral("top"), true},
        {specStripeline, "stripeline_positive", QStringLiteral("n"), false},
        {specStripeline, "stripeline_negative", QStringLiteral("n"), true},
    };

    bool all = true;
    for (const Job &job : jobs) {
        if (job.spec.trimmed().isEmpty()) continue;  // leave any previous copy alone

        QString cerr;
        const QString coloured = colourSpec(job.spec, job.invert ? negative : positive,
                                            job.invert ? positive : negative, &cerr);
        if (coloured.isEmpty()) {
            if (err) *err = QStringLiteral("%1: %2").arg(QLatin1String(job.name), cerr);
            all = false;
            continue;
        }

        QString rerr;
        const QImage picture = renderForDirection(job.direction, coloured, &rerr);
        if (picture.isNull()) {
            if (err)
                *err = QStringLiteral("%1: %2").arg(QLatin1String(job.name), rerr);
            all = false;
            continue;
        }

        const QString path = preparedRoot() + QLatin1Char('/') + QLatin1String(job.name) + ".png";
        // Quality 90 -> zlib level 1. These are flat two-colour patterns, which
        // compress almost as well at level 1 as at the default, and the default
        // is the slow half of preparing: four images at up to 1440x2560, deflated
        // hard, while the operator waits for a button. Losing nothing visually --
        // PNG is lossless at every level; only the effort spent packing changes.
        if (!picture.save(path, "PNG", 90)) {
            if (err) *err = QStringLiteral("could not write %1").arg(path);
            all = false;
            continue;
        }
        if (prepared) prepared->append(QLatin1String(job.name));
        if (sizes) sizes->append(picture.size());
    }
    return all;
}

bool MonitorDevice::show_prepared(const QString &polarity, QStringList *shown, QString *err) {
    const QString pol = polarity.trimmed().toLower();
    if (pol != QLatin1String("positive") && pol != QLatin1String("negative")) {
        if (err) *err = QStringLiteral("polarity must be \"positive\" or \"negative\"");
        return false;
    }

    // concentric -> TOP, stripeline -> the four sides. One file each; the sides
    // share a pattern because they share a resolution.
    //
    // The side directions are listed EXPLICITLY rather than passed to targets():
    // that helper only expands "all", and hands anything else back unchanged, so
    // asking it for "side" yields the literal string "side" -- which no panel is
    // mapped to, so every push silently found nothing. "side" is a client-side
    // shorthand (expandDirs) and was never a direction this device understood.
    struct Plan {
        QString file;
        QStringList directions;
    };
    const QVector<Plan> plan = {
        {QStringLiteral("concentric_") + pol, {QStringLiteral("top")}},
        {QStringLiteral("stripeline_") + pol,
         {QStringLiteral("n"), QStringLiteral("w"), QStringLiteral("s"), QStringLiteral("e")}},
    };

    bool all = true;
    QStringList unmapped;
    for (const Plan &entry : plan) {
        const QString path = preparedRoot() + QLatin1Char('/') + entry.file + QStringLiteral(".png");
        QImage picture;
        if (!picture.load(path)) {
            // Deliberately NOT rendering a replacement -- see ShowPrepared.srv.
            if (err)
                *err = QStringLiteral("nothing prepared: %1 is missing. Update the pattern first.")
                           .arg(path);
            return false;
        }
        for (const QString &target : entry.directions) {
            const Impl::Snapshot panel = d_->snapshotFor(this, target);
            if (!panel.mapped) {
                // Reported through *err as well as the console: this travels back
                // to whoever asked, and a failure with no reason attached is what
                // made this look like a camera fault at the operator's end.
                unmapped << target;
                all = false;
                continue;
            }
            d_->presentAsync(this, target, picture);
            if (shown) shown->append(target);
        }
    }

    if (!all) {
        const QString msg =
            QStringLiteral("no display is mapped to: %1. Monitor Viewer > Setup Monitor Direction.")
                .arg(unmapped.join(QStringLiteral(", ")));
        std::cerr << "[monitor] show_prepared: " << qUtf8Printable(msg) << "\n";
        if (err) *err = msg;
        return false;
    }
    if (err) err->clear();
    return true;
}

// ------------------------------------------------------------------- taking down --

bool MonitorDevice::close_pattern(const QString &direction) {
    // Deleting a window is GUI-thread work, but nothing waits on the answer, so
    // this is queued for the same reason presentAsync() is: a "turn everything
    // off" from a worker must not be able to hang against the teardown wait.
    QMetaObject::invokeMethod(this, [this, direction] {
        const QString dir = direction.trimmed().toLower();

        // "all" means every window this class put on a screen, including the
        // numbering ones from show_display_number -- those are keyed by display
        // number, not by direction, so iterating the five directions would leave
        // them up. "Turn everything off" has to turn everything off.
        if (dir.compare(QLatin1String("all"), Qt::CaseInsensitive) == 0) {
            for (auto *window : std::as_const(d_->windows)) delete window;
            d_->windows.clear();
            return;
        }

        const auto it = d_->windows.find(dir);
        if (it != d_->windows.end()) {
            delete it.value();
            d_->windows.erase(it);
        }
    });
    // The close is queued, so this says "accepted", not "the glass is dark". No
    // caller distinguishes the two, and nothing can fail here that the operator
    // could act on: an unmapped direction simply has no window to remove.
    return true;
}

bool MonitorDevice::close_pattern_all() { return close_pattern(QStringLiteral("all")); }
