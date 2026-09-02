#include "monitor_node.h"

#include <QBuffer>
#include <QByteArray>
#include <QImage>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

#include "server_context.h"

namespace {

const char *kDirections[5] = {"top", "n", "w", "s", "e"};

QByteArray encodePng(const QImage &img, int maxSide) {
    QImage out = img;
    const int side = std::max(img.width(), img.height());
    if (maxSide > 0 && side > maxSide)
        out = img.scaled(maxSide, maxSide, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QByteArray bytes;
    QBuffer buf(&bytes);
    buf.open(QIODevice::WriteOnly);
    out.save(&buf, "PNG");
    return bytes;
}

}  // namespace

MonitorNode::MonitorNode(ServerContext &ctx, const rclcpp::NodeOptions &options)
    : ServerNode("moil_monitor", ctx, options) {
    addService<moil_interfaces::srv::ShowPattern>(
        "/monitor/show_pattern",
        [this](const moil_interfaces::srv::ShowPattern::Request &req, moil_interfaces::srv::ShowPattern::Response &res) {
            const QString dir = QString::fromStdString(req.direction);
            const QByteArray img(reinterpret_cast<const char *>(req.image.data.data()),
                                 static_cast<int>(req.image.data.size()));
            const bool ok = (dir.compare("all", Qt::CaseInsensitive) == 0)
                                ? ctx_.monitor->show_pattern_all(img)
                                : ctx_.monitor->show_pattern(dir, img);
            res.success = ok;
            res.message = ok ? "" : "show_pattern failed for " + req.direction;
        });

    addService<moil_interfaces::srv::ShowPatternSpec>(
        "/monitor/show_pattern_spec",
        [this](const moil_interfaces::srv::ShowPatternSpec::Request &req, moil_interfaces::srv::ShowPatternSpec::Response &res) {
            const QString dir = QString::fromStdString(req.direction);
            const QString spec = QString::fromStdString(req.spec_json);
            const bool all = dir.compare("all", Qt::CaseInsensitive) == 0;

            QStringList targets;
            if (all)
                for (const char *d : kDirections) targets << QString::fromLatin1(d);
            else
                targets << dir;

            bool ok = true;
            for (const QString &d : targets) {
                if (!ctx_.monitor->show_pattern_spec(d, spec)) { ok = false; continue; }
                // Report the size ACTUALLY used, per panel. This is the field that
                // catches a resolution mismatch, and a resolution mismatch on this
                // rig produces a wrong physical pattern size with no visible symptom
                // -- so it is measured from the render rather than echoed back from
                // the request.
                QString err;
                const QImage rendered = ctx_.monitor->renderForDirection(d, spec, &err);
                res.widths.push_back(rendered.width());
                res.heights.push_back(rendered.height());
                res.directions.push_back(d.toStdString());
            }
            res.success = ok;
            res.message = ok ? "" : "one or more panels rejected the spec";
        });

    // Render the calibration patterns ONCE and keep them as files. The client
    // sends two descriptions and no images; both polarities are worked out here.
    // See PreparePatterns.srv.
    addService<moil_interfaces::srv::PreparePatterns>(
        "/monitor/prepare_patterns",
        [this](const moil_interfaces::srv::PreparePatterns::Request &req, moil_interfaces::srv::PreparePatterns::Response &res) {
            const auto colourOf = [](const std::vector<int32_t> &v, const QColor &fallback) {
                if (v.size() < 3) return fallback;
                const auto clamp255 = [](int32_t c) { return std::clamp<int>(c, 0, 255); };
                return QColor(clamp255(v[0]), clamp255(v[1]), clamp255(v[2]));
            };
            // White on black is what the form defaults to, and it is a usable
            // pattern -- better than refusing because a colour was left out.
            const QColor pos = colourOf(req.positive_rgb, QColor(255, 255, 255));
            const QColor neg = colourOf(req.negative_rgb, QColor(0, 0, 0));

            QStringList prepared;
            QVector<QSize> sizes;
            QString err;
            const bool ok = ctx_.monitor->prepare_patterns(
                QString::fromStdString(req.spec_concentric),
                QString::fromStdString(req.spec_stripeline), pos, neg, &prepared, &sizes, &err);

            for (const QString &name : prepared) res.prepared.push_back(name.toStdString());
            for (const QSize &s : sizes) {
                res.widths.push_back(s.width());
                res.heights.push_back(s.height());
            }
            res.directory = ctx_.monitor->preparedDir().toStdString();
            res.success = ok;
            res.message = err.toStdString();
            RCLCPP_INFO(get_logger(), "prepare_patterns: %d written to %s%s%s",
                        int(prepared.size()), res.directory.c_str(),
                        ok ? "" : " -- ", ok ? "" : qPrintable(err));
        });

    // Put a prepared polarity on the glass: a file load and a blit, no render.
    addService<moil_interfaces::srv::ShowPrepared>(
        "/monitor/show_prepared",
        [this](const moil_interfaces::srv::ShowPrepared::Request &req, moil_interfaces::srv::ShowPrepared::Response &res) {
            QStringList shown;
            QString err;
            const bool ok = ctx_.monitor->show_prepared(QString::fromStdString(req.polarity),
                                                        &shown, &err);
            for (const QString &d : shown) res.shown.push_back(d.toStdString());
            res.success = ok;
            res.message = err.toStdString();
        });

    addService<moil_interfaces::srv::ClosePattern>(
        "/monitor/close_pattern",
        [this](const moil_interfaces::srv::ClosePattern::Request &req, moil_interfaces::srv::ClosePattern::Response &res) {
            const QString dir = QString::fromStdString(req.direction);
            res.success = (dir.compare("all", Qt::CaseInsensitive) == 0)
                               ? ctx_.monitor->close_pattern_all()
                               : ctx_.monitor->close_pattern(dir);
            res.message = "";
        });

    addService<moil_interfaces::srv::SetBrightness>(
        "/monitor/set_brightness",
        [this](const moil_interfaces::srv::SetBrightness::Request &req, moil_interfaces::srv::SetBrightness::Response &res) {
            const QString dir = QString::fromStdString(req.direction);
            const bool ok = (dir.compare("all", Qt::CaseInsensitive) == 0)
                                ? ctx_.monitor->set_brightness_all(req.brightness)
                                : ctx_.monitor->set_brightness(dir, req.brightness);
            res.success = ok;
            // DDC/CI is not available on every panel and the rig log records four
            // of these screens refusing it. Reported as a failure rather than
            // quietly treated as success -- a brightness that did not change is a
            // calibration input that is not what the operator set.
            res.message = ok ? "" : "DDC/CI brightness not accepted by " + req.direction;
        });

    addService<moil_interfaces::srv::GetBrightness>(
        "/monitor/get_brightness",
        [this](const moil_interfaces::srv::GetBrightness::Request &req, moil_interfaces::srv::GetBrightness::Response &res) {
            const QString v =
                ctx_.monitor->get_brightness(QString::fromStdString(req.direction));
            bool ok = false;
            res.brightness = v.trimmed().toDouble(&ok);
            res.success = ok;
            res.message = ok ? "" : v.toStdString();
        });

    addService<moil_interfaces::srv::SetDisplayDirection>(
        "/monitor/set_display_direction",
        [this](const moil_interfaces::srv::SetDisplayDirection::Request &req, moil_interfaces::srv::SetDisplayDirection::Response &res) {
            const QString msg = ctx_.monitor->set_display_direction(
                QString::fromStdString(req.display_top), QString::fromStdString(req.display_n),
                QString::fromStdString(req.display_w), QString::fromStdString(req.display_s),
                QString::fromStdString(req.display_e));
            // Persisted by the device into the per-user config, as the monitor node
            // persisted its own copy: an operator should map the rig once, not once
            // per launch -- and now, not once per client either.
            res.success = !msg.isEmpty();
            res.message = msg.toStdString();
        });

    addService<moil_interfaces::srv::GetDisplayDirection>(
        "/monitor/get_display_direction",
        [this](const moil_interfaces::srv::GetDisplayDirection::Request &req, moil_interfaces::srv::GetDisplayDirection::Response &res) {
            // Named for the service, whose documentation said "display number ->
            // direction"; the code on both sides has always taken a DIRECTION and
            // returned the display, and this keeps that.
            const QString d =
                ctx_.monitor->get_display_direction(QString::fromStdString(req.display_number));
            res.success = !d.isEmpty();
            res.direction = d.toStdString();
            res.message = d.isEmpty() ? "not mapped" : "";
        });

    addService<moil_interfaces::srv::MonitorCommand>(
        "/monitor/command",
        [this](const moil_interfaces::srv::MonitorCommand::Request &req, moil_interfaces::srv::MonitorCommand::Response &res) {
            const QString cmd = QString::fromStdString(req.command).trimmed().toLower();
            if (cmd == "show_display_number") {
                const QString msg = ctx_.monitor->show_display_number();
                res.success = !msg.isEmpty();
                res.message = msg.toStdString();
            } else {
                res.success = false;
                res.message = "unknown command: " + req.command;
            }
        });

    addService<moil_interfaces::srv::DescribeScreens>(
        "/monitor/describe_screens",
        [this](const moil_interfaces::srv::DescribeScreens::Request &,
               moil_interfaces::srv::DescribeScreens::Response &res) {
            // describeScreens returns lines like
            //   "DISPLAY5 (EV2730Q) 1920x1920 at (3840,0) -> top"
            // and they are split here so the client never parses. Parsing is the
            // client doing work it can get wrong, and the only reason it was a
            // string was a log line.
            //
            // The pattern below is deliberately tolerant of whatever sits between
            // the display name and its size. It used to be "^(\S+)\s+(\d+)x(\d+)",
            // which assumed "DISPLAY1 1920x1080" -- then the model name in
            // parentheses was added, every match failed, and the whole service went
            // quietly useless: `names` got the entire unsplit line, `widths` and
            // `heights` came back 0, and `directions` came back "-" for every
            // screen because the lookup was comparing a display name against that
            // whole line. `mapping_complete` stayed true throughout, so nothing
            // looked wrong. Observed on the rig 2026-08-19.
            //
            // DO NOT "simplify" this back to \s+. It has been reverted to the
            // strict form twice now, most recently by the 2026-08-25 re-vendor,
            // and the rig reports 7 screens with model names on every one of them.
            static const QRegularExpression re(
                QStringLiteral("^(\\S+).*?(\\d+)x(\\d+)"));
            int mapped = 0;
            for (const QString &s : ctx_.monitor->describeScreens()) {
                const auto m = re.match(s);
                const QString name = m.hasMatch() ? m.captured(1) : s;
                res.names.push_back(name.toStdString());
                res.widths.push_back(m.hasMatch() ? m.captured(2).toInt() : 0);
                res.heights.push_back(m.hasMatch() ? m.captured(3).toInt() : 0);

                QString dir = "-";
                for (const char *d : kDirections) {
                    const QString cand = QString::fromLatin1(d);
                    if (ctx_.monitor->get_display_direction(cand) == name) { dir = cand; break; }
                }
                res.directions.push_back(dir.toStdString());
            }
            for (const char *d : kDirections)
                if (!ctx_.monitor->get_display_direction(QString::fromLatin1(d)).isEmpty()) ++mapped;
            res.mapping_complete = mapped == 5;
            res.success = true;
            res.message = mapped == 5 ? "" : "display mapping incomplete -- directions are UNSET";
        });

    addService<moil_interfaces::srv::RenderForDirection>(
        "/monitor/render_for_direction",
        [this](const moil_interfaces::srv::RenderForDirection::Request &req, moil_interfaces::srv::RenderForDirection::Response &res) {
            QString err;
            const QImage img = ctx_.monitor->renderForDirection(
                QString::fromStdString(req.direction), QString::fromStdString(req.spec_json),
                &err);
            if (img.isNull()) {
                res.success = false;
                res.message = err.toStdString();
                return;
            }
            res.width = img.width();
            res.height = img.height();
            const QByteArray png = encodePng(img, req.max_side);
            res.image.header.stamp = now();
            res.image.format = "png";
            res.image.data.assign(png.begin(), png.end());
            res.success = true;
            res.message = "";
        });

    RCLCPP_INFO(get_logger(), "monitor: %s", qPrintable(ctx_.monitorUrl()));
}
