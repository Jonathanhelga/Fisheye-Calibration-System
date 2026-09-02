#pragma once

#include <memory>

#include "server_node.h"
#include <sensor_msgs/msg/compressed_image.hpp>

#include "moil_interfaces/srv/capture.hpp"

struct ServerContext;

// /moil_camera -- the fisheye camera.
//
// Two paths, for two different jobs, and the difference is the whole design:
//
//   the TOPIC is the live preview. BEST_EFFORT, KEEP_LAST(1): a dropped preview
//   frame costs nothing, and a reliable stream of 0.29 MB frames at 19 Hz would
//   spend the link retransmitting pictures nobody will look at. It is also
//   throttled well below the capture rate -- see preview_hz.
//
//   the SERVICE is the measurement. It answers with a frame that arrived AFTER
//   the request, never a buffered one, because the device holds frames from
//   before the pattern on the glass changed and answering with one of those
//   silently re-measures the previous pattern.
//
// The preview costs real bandwidth and it is the one thing on this link that can
// starve the control path. The rig log records exactly this failure on the v2.0
// client: 0.29 MB per sample fragments into ~200 datagrams, ~3800 per second, and
// with BEST_EFFORT a single lost fragment discards the whole frame. So the
// preview is published at a reduced size and rate by default, and full-resolution
// pixels move only when something actually asks for them.
class CameraNode : public ServerNode {
public:
    CameraNode(ServerContext &ctx, const rclcpp::NodeOptions &options = rclcpp::NodeOptions());

private:
    using Capture = moil_interfaces::srv::Capture;

    void onCapture(const Capture::Request &req, Capture::Response &res);
    void publishPreview();

    rclcpp::Publisher<sensor_msgs::msg::CompressedImage>::SharedPtr pubImage_;
    rclcpp::TimerBase::SharedPtr previewTimer_;

    int previewMaxSide_ = 720;
    int previewQuality_ = 70;
};
