#pragma once

#include <memory>

#include <QByteArray>
#include <QColor>
#include <QImage>
#include <QObject>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVector>

#include "device_config.h"

// The five calibration screens, driven by THIS process.
//
// Replaces MonitorRosClient -> /monitor/* -> monitor_node.py ->
// monitor_module_autodetect.py. The public API is the one MonitorRosClient had,
// so the pattern-generator and monitor-viewer flows did not have to change.
//
// Two things are better for the merge rather than merely the same:
//
//   * show_pattern_spec() renders in-process with the SAME ComputeOps renderer
//     the preview uses, at the panel's own pixel grid. On the rig this took a
//     service call from the monitor node to the compute node to do; here it is a
//     function call, so spec_supported() is now simply true and the PNG fallback
//     exists only for callers that hold bytes and no description.
//
//   * the patterns are Qt windows instead of OpenCV ones. cv::imshow needs
//     cv::waitKey() pumped from the thread that created the window or the screen
//     goes white-on-repaint, which is why the rig wrote each pattern to a PNG on
//     disk and read it straight back. Nothing here touches the disk.
//
// Threading: the monitor-viewer pushes patterns from a QtConcurrent worker, and
// windows may only be touched on the GUI thread, so every public method marshals
// there and waits. The wait is what the service round trip used to be.
class MonitorDevice : public QObject {
    Q_OBJECT

public:
    explicit MonitorDevice(DeviceConfig cfg = DeviceConfig::load(),
                           QObject *parent = nullptr);
    ~MonitorDevice() override;

    MonitorDevice(const MonitorDevice &) = delete;
    MonitorDevice &operator=(const MonitorDevice &) = delete;

    // Big number on every attached screen, so the operator can fill in the
    // mapping form. Returns a human-readable summary, or "" if nothing could be
    // enumerated.
    QString show_display_number();

    // Map the five directions onto display numbers ("1" or "DISPLAY1"). Persisted
    // to config/devices.json, as the monitor node persisted its own copy: an
    // operator should map the rig once, not once per launch.
    QString set_display_direction(const QString &dis_num_top, const QString &dis_num_n,
                                  const QString &dis_num_w, const QString &dis_num_s,
                                  const QString &dis_num_e);

    // Display number a direction is mapped to. Named for the ROS service, whose
    // documentation said "display number -> direction"; the code on both sides has
    // always taken a DIRECTION and returned the display, and this keeps that.
    QString get_display_direction(const QString &direction);

    bool set_brightness(const QString &direction, double brightness);
    bool set_brightness_all(double brightness);
    QString get_brightness(const QString &direction);

    bool show_pattern(const QString &direction, const QByteArray &image);
    bool show_pattern_all(const QByteArray &image);

    // Draw from the pattern's JSON description at the panel's own resolution.
    // `spec` is the JSON that was documented in ShowPatternSpec.srv.
    bool show_pattern_spec(const QString &direction, const QString &spec);

    // Render what show_pattern_spec would put up, WITHOUT touching the glass.
    // This is how the mapping and the renderer can be checked on a machine whose
    // screens are somebody's desktop, and how --test-monitor checks them without
    // blanking the rig mid-calibration. Null image with *err set.
    QImage renderForDirection(const QString &direction, const QString &spec, QString *err);

    // ---- prepared patterns -------------------------------------------------
    //
    // Render the calibration patterns once, keep them as files, and blit them at
    // shot time. See PreparePatterns.srv for why.
    //
    // The polarity swap lives here: odd layers take one colour and even layers
    // the other, inverted between positive and negative. That is arithmetic over
    // the operator's design, so it belongs on this side rather than being applied
    // by whichever client happens to be driving.

    // Render `specConcentric` for TOP and `specStripeline` for the side panels,
    // in both polarities, and write four PNGs. An empty spec is skipped, leaving
    // any previously prepared copy alone. `prepared` gets the names written and
    // `sizes` the pixel grid each was rendered at.
    bool prepare_patterns(const QString &specConcentric, const QString &specStripeline,
                          const QColor &positive, const QColor &negative,
                          QStringList *prepared, QVector<QSize> *sizes, QString *err);

    // Blit the prepared polarity: concentric to TOP, stripeline to N/W/S/E.
    // `shown` gets the directions that actually took it.
    //
    // Fails rather than rendering a replacement. A shot taken against a pattern
    // quietly regenerated from an edited spec measures the wrong thing and looks
    // exactly like a good one.
    bool show_prepared(const QString &polarity, QStringList *shown, QString *err);

    // Where the prepared PNGs live, so an operator with a suspect round can look
    // at the exact picture that was on the glass.
    QString preparedDir() const;

    // The panels this process can see, as "DISPLAY1 1920x1080" strings, and the
    // direction each is mapped to (or "-"). For diagnostics: the first question
    // when a pattern does not appear is which screens the app thinks it has.
    QStringList describeScreens();

    // Always true now: the renderer is compiled into this binary, so there is no
    // older-node case to fall back from. Kept so callers need not change.
    bool spec_supported() const { return true; }

    bool close_pattern(const QString &direction);
    bool close_pattern_all();

    // Kept so the existing "monitor URL" field in the UI stays harmless.
    void setUrl(const QString &ignored);
    QString url() const;

private:
    struct Impl;
    std::unique_ptr<Impl> d_;
};
