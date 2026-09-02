#pragma once

// A capture path that goes straight to Media Foundation, which is what the
// Windows Camera app uses.
//
// Why this exists at all: OpenCV 4.6.0's msmf backend cannot open the rig's
// LRCP U3-imx577 -- every camera id fails, and OPENCV_VIDEOIO_MSMF_ENABLE_HW_
// TRANSFORMS=0 does not change that. The device is not at fault: Media
// Foundation enumerates it, activates it and delivers 3040x3040 in MJPG, NV12
// and YUY2, and the Windows Camera app shows a smooth live view of it. Only the
// OpenCV wrapper is missing.
//
// That left dshow as the only backend OpenCV could open it with, and dshow pins
// this camera to uncompressed YUY2 -- 18.5 MB a frame, 1.17 fps measured. Going
// to Media Foundation directly gets the MJPG mode instead: 0.25 MB a frame.
//
// The interface is deliberately the small part of cv::VideoCapture that
// camera_device.cpp uses, so the two capture paths are interchangeable.

#include <opencv2/core.hpp>

#include <QString>

#include <memory>

class MfCapture {
public:
    MfCapture();
    ~MfCapture();

    MfCapture(const MfCapture &) = delete;
    MfCapture &operator=(const MfCapture &) = delete;

    // Opens capture device `index`, asking for `width` x `height`. The size is a
    // request: the closest mode the camera offers is taken, and what was actually
    // negotiated is reported by width()/height() -- the caller is told rather than
    // silently given something else.
    bool open(int index, int width, int height, QString *err = nullptr);

    // One frame as BGR, the format the rest of the app works in. Blocks until the
    // camera produces it. False means the stream is finished or broken; the caller
    // reopens, exactly as it would after cv::VideoCapture::read() failing.
    bool read(cv::Mat *out);

    void release();
    bool isOpened() const;

    int width() const;
    int height() const;

    // Four-character code of the negotiated stream, for the startup line.
    QString format() const;

private:
    struct Impl;
    std::unique_ptr<Impl> d_;
};
