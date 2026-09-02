#pragma once

#include <memory>

#include <QByteArray>
#include <QString>

#include <opencv2/core.hpp>

#include "device_config.h"

// The camera, opened by THIS process.
//
// Replaces CameraRosClient, which subscribed to
// /camera/image_raw/compressed and kept the newest frame. The subscription is
// gone; a background grab thread takes its place, and for the same reason the
// node had one: cap.read() on a 3040x3040 USB stream blocks for ~50 ms, and a
// capture must never be a frame the device had buffered from before the pattern
// on the glass changed.
//
// single_image() therefore keeps the ROS client's contract exactly: it returns a
// frame that arrived AFTER the call, never a cached one, and empty when none
// arrives in time -- so a Capture with the camera unplugged fails loudly instead
// of silently re-analysing the previous shot.
class CameraDevice {
public:
    explicit CameraDevice(DeviceConfig cfg = DeviceConfig::load());
    ~CameraDevice();

    CameraDevice(const CameraDevice &) = delete;
    CameraDevice &operator=(const CameraDevice &) = delete;

    // Latest frame as encoded bytes (JPEG or PNG, per DeviceConfig), or empty.
    QByteArray single_image();

    // The same frame undecoded, for callers that are going to cv::imdecode() it
    // straight back. Empty Mat when no fresh frame arrives.
    cv::Mat frame();

    // A frame that is genuinely OF THE CURRENT SCENE, for captures.
    //
    // frame() returns the next one to arrive, which was already in flight when it
    // was asked for -- so on a rig where the pattern has just changed it is an
    // image of the PREVIOUS pattern. This discards what was already on its way
    // first. Costs about two frame periods; use it wherever the picture is a
    // measurement rather than a preview.
    cv::Mat capture_frame();

    // Whether the device is currently open. The grab thread reopens on its own
    // when it is not, so false here means "not right now", not "give up".
    bool isOpen() const;

    // Kept so the existing "camera URL" field in the UI stays harmless; url()
    // answers with the device actually in use, which is what the capture-failed
    // message wants to name.
    void setUrl(const QString &ignored);
    QString url() const;

private:
    struct Impl;
    // shared_ptr, not unique_ptr: the grab thread holds a reference of its own.
    // Closing the app must not wait for a blocking cv::VideoCapture::open() to
    // come back, and the thread must not be left pointing at freed state either
    // -- so on shutdown the thread is let go and the state outlives this object
    // until it exits. See the destructor.
    std::shared_ptr<Impl> d_;
};
