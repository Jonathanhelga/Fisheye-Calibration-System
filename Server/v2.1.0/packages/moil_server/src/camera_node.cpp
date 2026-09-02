#include "camera_node.h"

#include <QByteArray>
#include <QSize>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "server_context.h"

namespace {

// Encode a frame for the wire, shrinking it first when asked. Returns empty on an
// empty Mat, which is how "no fresh frame arrived" reaches the caller.
std::vector<unsigned char> encode(const cv::Mat &frame, int maxSide, int quality,
                                  bool png, int *outW, int *outH) {
    if (frame.empty()) return {};
    if (outW) *outW = frame.cols;
    if (outH) *outH = frame.rows;

    cv::Mat out = frame;
    const int side = std::max(frame.cols, frame.rows);
    if (maxSide > 0 && side > maxSide) {
        const double s = static_cast<double>(maxSide) / side;
        // INTER_AREA: this is always a downscale, and it is the only interpolation
        // that does not alias a pattern of concentric rings into a moire that
        // looks like a defect on the glass.
        cv::resize(frame, out, cv::Size(), s, s, cv::INTER_AREA);
    }

    std::vector<unsigned char> buf;
    if (png)
        cv::imencode(".png", out, buf);
    else
        cv::imencode(".jpg", out, buf, {cv::IMWRITE_JPEG_QUALITY, quality});
    return buf;
}

}  // namespace

CameraNode::CameraNode(ServerContext &ctx, const rclcpp::NodeOptions &options)
    : ServerNode("moil_camera", ctx, options) {
    addService<Capture>("/camera/capture", &CameraNode::onCapture);

    // Same topic name and QoS the rig has always published on, so a v2.0.0 client
    // subscribes to this server unchanged.
    auto qos = rclcpp::QoS(rclcpp::KeepLast(1)).best_effort().durability_volatile();
    pubImage_ = create_publisher<sensor_msgs::msg::CompressedImage>(
        "/camera/image_raw/compressed", qos);

    const double hz = declare_parameter<double>("preview_hz", 10.0);
    previewMaxSide_ = declare_parameter<int>("preview_max_side", 720);
    previewQuality_ = declare_parameter<int>("preview_quality", 70);

    if (hz > 0.0) {
        previewTimer_ = create_wall_timer(
            std::chrono::milliseconds(static_cast<int>(1000.0 / hz)),
            [this] { publishPreview(); }, group_);
    }

    RCLCPP_INFO(get_logger(), "camera: %s -- preview %.1f Hz at <=%d px, capture full size",
                qPrintable(ctx_.cameraUrl()), hz, previewMaxSide_);
}

void CameraNode::publishPreview() {
    // Nobody is looking: no work at all. Same principle as the axis watch list,
    // whose publishWatchedStates returns early for the same reason.
    //
    // This matters more here than the one line suggests. The rig's server sat at
    // roughly 28 s of CPU per minute while completely idle, and all of it was this
    // timer: a resize of a 3040x3040 frame and a JPEG encode, ten times a second,
    // published to nothing. The topic is BEST_EFFORT/volatile, so there is no
    // latched last frame a late subscriber would miss -- it gets the next tick,
    // at most 100 ms later.
    //
    // The check is BEFORE frame(), which blocks until a fresh frame arrives (up to
    // several frame periods on the dshow path); doing it after would leave the
    // timer waiting on the camera to produce something it then throws away.
    if (pubImage_->get_subscription_count() == 0) return;

    // frame() returns empty when no fresh frame arrived; publishing nothing is
    // correct then. A preview that repeats its last frame looks like a camera that
    // is working.
    const cv::Mat f = ctx_.camera->frame();
    if (f.empty()) return;

    int w = 0, h = 0;
    auto buf = encode(f, previewMaxSide_, previewQuality_, /*png=*/false, &w, &h);
    if (buf.empty()) return;

    sensor_msgs::msg::CompressedImage msg;
    msg.header.stamp = now();
    msg.header.frame_id = "camera";
    msg.format = "jpeg";
    msg.data = std::move(buf);
    pubImage_->publish(msg);
}

void CameraNode::onCapture(const Capture::Request &req, Capture::Response &res) {
    if (!ctx_.camera) {
        res.success = false;
        res.message = "no camera on this server";
        return;
    }

    // Into a session slot: the store grabs the frame itself and keeps it, which is
    // the arrangement that means captures are never uploaded.
    if (!req.session_slot.empty()) {
        QString err;
        const QString slot = QString::fromStdString(req.session_slot);
        const double timeout = req.timeout > 0.0 ? req.timeout : 5.0;
        if (!ctx_.sessions->capture(slot, timeout, &err)) {
            res.success = false;
            res.message = err.toStdString();
            return;
        }
        // Hand back a display-sized copy so the client can show what it just took
        // without a second round trip for the full frame.
        //
        // captureBytes, not captureImage. The store already encodes the shrunk
        // copy, so taking a QImage back meant decoding that encode and then
        // encoding it AGAIN here: two PNG encodes and a decode of the same 1024 px
        // picture, for bytes the store had ready. Same mistake as /session/image
        // had, and the same fix.
        QSize stored;
        const QByteArray small = ctx_.sessions->captureBytes(slot, 1024, &stored, &err);
        if (small.isEmpty()) {
            res.success = false;
            res.message = err.toStdString();
            return;
        }
        res.success = true;
        // The frame AS CAPTURED, which is what Capture.srv promises for these two
        // -- not the size of the display copy above. They read 1024x1024 before, so
        // the one field that says how big the measurement is was describing the
        // preview instead.
        res.width = stored.width();
        res.height = stored.height();
        res.message = "";
        // The stored capture is the measurement; this copy is only to look at.
        res.image.format = small.startsWith("\x89PNG") ? "png" : "jpeg";
        res.image.header.stamp = now();
        res.image.data.assign(small.begin(), small.end());
        return;
    }

    // A plain capture. single_image() keeps the contract that matters: the frame
    // arrived after this call, and it is empty rather than stale when none did.
    const QByteArray bytes = ctx_.camera->single_image();
    if (bytes.isEmpty()) {
        res.success = false;
        res.message = "no fresh frame within the timeout -- camera is " +
                       std::string(ctx_.camera->isOpen() ? "open" : "NOT open");
        return;
    }
    res.success = true;
    res.image.header.stamp = now();
    res.image.header.frame_id = "camera";
    res.image.format = ctx_.config.captureFormat.toStdString();
    res.image.data.assign(bytes.begin(), bytes.end());
    res.width = ctx_.config.imageWidth;
    res.height = ctx_.config.imageHeight;
    res.message = "";
}
