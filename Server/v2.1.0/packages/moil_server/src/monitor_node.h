#pragma once

#include <memory>

#include "server_node.h"

#include "moil_interfaces/srv/close_pattern.hpp"
#include "moil_interfaces/srv/describe_screens.hpp"
#include "moil_interfaces/srv/get_brightness.hpp"
#include "moil_interfaces/srv/get_display_direction.hpp"
#include "moil_interfaces/srv/monitor_command.hpp"
#include "moil_interfaces/srv/prepare_patterns.hpp"
#include "moil_interfaces/srv/render_for_direction.hpp"
#include "moil_interfaces/srv/show_prepared.hpp"
#include "moil_interfaces/srv/set_brightness.hpp"
#include "moil_interfaces/srv/set_display_direction.hpp"
#include "moil_interfaces/srv/show_pattern.hpp"
#include "moil_interfaces/srv/show_pattern_spec.hpp"

struct ServerContext;

// /moil_monitor -- the five calibration screens.
//
// The panels are wired to THIS machine, which is the reason the server has to be
// a GUI process at all: the patterns are Qt windows on the server's own desktop.
// A client on another machine cannot put anything on this glass except by asking
// here.
//
// show_pattern_spec is the path that should be used. It sends ~1 KB of
// description instead of ~80 KB of PNG per screen, and -- the part that actually
// matters -- the node renders at each panel's NATIVE resolution. The client does
// not know which panel will display a pattern, and a wrong resolution guess only
// stretches the image, silently changing the pattern's physical size on the glass.
// That corrupts a calibration without ever looking like an error. This rig is
// exactly the case it protects: the top screen is 1920x1920 and the four sides
// are 2160x3840 -- 4K EV2785 panels mounted portrait -- so no single rendered
// image can be correct for all of them.
//
// (The sides were documented as 1440x2560 until 2026-08-24; that was wrong, and
// measured directly off the rig through /monitor/describe_screens. The figure
// came back in the 2026-08-25 re-vendor and is corrected again here. It is the
// same number as in ShowPatternSpec.srv and the two client headers -- if these
// ever disagree, the rig is the tie-breaker, not the comment.)
//
// show_pattern (the PNG path) is kept for callers holding bytes and no
// description, and because a v2.0.0 client still calls it.
//
// Threading: MonitorDevice marshals every one of these to the GUI thread and
// waits, so they may be called from any executor thread. What must NOT come back
// is a call that blocks the GUI thread while the GUI thread waits for it -- that
// deadlock is what monitorconcurrency_test in the v2.0 tree exists to catch, and
// the shape it catches is still here.
class MonitorNode : public ServerNode {
public:
    MonitorNode(ServerContext &ctx, const rclcpp::NodeOptions &options = rclcpp::NodeOptions());

private:

    // Prepared patterns: rendered once and kept as files, blitted at shot time.
};
