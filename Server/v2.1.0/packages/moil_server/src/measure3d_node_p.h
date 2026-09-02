#pragma once

// The two helpers both halves of Measure3dNode need.
//
// The node was one 750-line file. It is two now, cut where the subject changes:
//
//   measure3d_node.cpp    loading a camera and looking through it -- the source
//                         map, the Moildev instances, the remap cache, and the
//                         anypoint and panorama views built from them
//   measure3d_detect.cpp  finding the board and measuring with it -- detection,
//                         the auto-frame search that hunts for a pose that sees
//                         it, and the triangulation that turns two views into a
//                         plane fit
//
// Everything else each half uses is local to its own file. These two are here
// because a picture crosses the boundary in both directions: every service
// answers with one, and two of them are given one.

#include <QByteArray>

#include <opencv2/core.hpp>
#include <sensor_msgs/msg/compressed_image.hpp>

namespace measure3d_detail {

// Encode for the wire, shrinking first when asked. Empty in, empty out.
QByteArray encodePng(const cv::Mat &m, int maxSide);

cv::Mat decode(const sensor_msgs::msg::CompressedImage &img);

}  // namespace measure3d_detail
