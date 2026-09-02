#include "camera_device.h"
#include "camera_mf.h"

#include <memory>

#include <algorithm>  // std::clamp, for the capture wait budget
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <cstdint>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/videoio.hpp>

#include <iostream>

namespace {

// Same three names moil_camera.yaml accepted, resolving to the same flags.
// "msmf" measured 21.3 fps at 3040x3040 against dshow's 1.1, because dshow pins
// the stream to uncompressed YUY2. Off Windows both constants exist but neither
// backend does, so anything resolves to CAP_ANY.
int backendFlag(const QString &name) {
#ifdef _WIN32
    const QString n = name.toLower();
    if (n == QLatin1String("dshow")) return cv::CAP_DSHOW;
    if (n == QLatin1String("any")) return cv::CAP_ANY;
    return cv::CAP_MSMF;
#else
    Q_UNUSED(name);
    return cv::CAP_ANY;
#endif
}

// How long to wait before trying a camera that is not answering. The same
// reopen_period_sec moil_camera.yaml used, for the same reason: a rig powering
// up should be picked up on its own, and a rig with no camera should cost
// nothing while it has none.
constexpr int kRetryMs = 2000;

}  // namespace

struct CameraDevice::Impl {
    DeviceConfig cfg;

    cv::VideoCapture cap;
    // The "mf" backend does not go through OpenCV at all -- see camera_mf.h for
    // why it has to exist. Exactly one of the two is live at a time.
    MfCapture mf;
    bool usingMf = false;

    bool captureOpened() const { return usingMf ? mf.isOpened() : cap.isOpened(); }
    void captureRelease() {
        if (usingMf)
            mf.release();
        else
            cap.release();
    }

    std::thread grabber;
    // Signalled as grabLoop() returns. std::thread has no timed join, and the
    // destructor needs one.
    std::promise<void> finishedPromise;
    std::shared_future<void> finished = finishedPromise.get_future().share();
    std::atomic<bool> stop{false};
    std::atomic<bool> open{false};

    std::mutex mtx;
    std::condition_variable cv;
    cv::Mat latest;
    std::uint64_t seq = 0;  // bumped per frame, so a caller can wait for a NEW one

    // Measured gap between frames. A capture has to wait for a frame that does
    // not exist yet, so how long it may wait is a property of the stream, not a
    // constant -- see the note on frame().
    std::atomic<int> frameIntervalMs{0};

    // Why the last mf open failed, so the retry line can say more than "cannot
    // open". cv::VideoCapture has no equivalent to report.
    QString lastOpenError;

    bool openDevice() {
        usingMf = cfg.cameraBackend.compare(QLatin1String("mf"), Qt::CaseInsensitive) == 0;
        if (usingMf) {
            QString err;
            if (!mf.open(cfg.cameraId, cfg.imageWidth, cfg.imageHeight, &err)) {
                // Said every attempt would flood; the caller throttles. But the
                // reason has to survive, because "cannot open" alone does not
                // separate "unplugged" from "another program has it".
                lastOpenError = err;
                return false;
            }
            lastOpenError.clear();
            return true;
        }

        cap.open(cfg.cameraId, backendFlag(cfg.cameraBackend));
        if (!cap.isOpened()) return false;

        // The stream format is left to the backend, as moil_camera.yaml's empty
        // `fourcc` did -- MSMF negotiates its own and the measured 21 fps is what
        // it negotiates. (capture_format is the encoding of the RESULT, not of
        // the stream.)
        cap.set(cv::CAP_PROP_FRAME_WIDTH, cfg.imageWidth);
        cap.set(cv::CAP_PROP_FRAME_HEIGHT, cfg.imageHeight);
        // Depth 1. Without it the driver hands back its whole queue oldest-first,
        // and the "fresh frame" this class promises is a frame from seconds ago.
        cap.set(cv::CAP_PROP_BUFFERSIZE, 1);

        return true;
    }

    // Said once per working open, not once per attempt: a backend that opens a
    // device which is not really there would otherwise report its 0x0 "camera"
    // every retry, for as long as the app runs.
    void reportOpen() const {
        const int w = usingMf ? mf.width() : static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
        const int h = usingMf ? mf.height() : static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
        std::cerr << "[camera] open: id=" << cfg.cameraId << " "
                  << qUtf8Printable(cfg.cameraBackend) << " " << w << "x" << h;
        // Which stream format was negotiated, for mf. It is the difference
        // between 0.25 MB and 18.5 MB a frame on this camera, so it decides
        // whether the live view moves -- worth one word in the startup line.
        if (usingMf) std::cerr << " " << qUtf8Printable(mf.format());
        if (w != cfg.imageWidth || h != cfg.imageHeight)
            std::cerr << "  (asked for " << cfg.imageWidth << "x" << cfg.imageHeight << ")";
        std::cerr << "\n";
    }

    // Sleep in short steps so a stop request is noticed promptly rather than
    // after the whole retry period.
    void sleepInterruptibly(int totalMs) {
        for (int slept = 0; slept < totalMs && !stop.load(); slept += 100)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    void grabLoop() {
        // Consecutive failures, for the log throttle. A camera that is simply not
        // plugged in must not fill the console at the retry rate: on a rig with
        // no camera the useful information is "it is not there", said once, and
        // then again occasionally to show the app is still trying.
        int failures = 0;
        std::chrono::steady_clock::time_point lastFrameAt{};

        while (!stop.load()) {
            if (!captureOpened()) {
                open = false;
                if (!openDevice()) {
                    if (failures++ % 30 == 0) {
                        std::cerr << "[camera] cannot open id=" << cfg.cameraId;
                        if (!lastOpenError.isEmpty())
                            std::cerr << ": " << qUtf8Printable(lastOpenError);
                        std::cerr << "; retrying every " << kRetryMs / 1000 << " s\n";
                    }
                    sleepInterruptibly(kRetryMs);
                    continue;
                }
            }

            cv::Mat frame;
            const bool got = usingMf ? mf.read(&frame) : cap.read(frame);
            if (!got || frame.empty()) {
                // A device that has gone away fails every read, and releasing
                // sends us back to the reopen branch above.
                //
                // The retry wait belongs HERE as well as on the open. Some
                // backends "open" a device that is not really there -- v4l2 via
                // GStreamer will hand back a capture whose every read fails --
                // and without this the loop is open / read-fail / release with
                // nothing in between, spinning a core flat out and printing a
                // line per iteration.
                if (failures++ % 30 == 0)
                    std::cerr << "[camera] read failed on id=" << cfg.cameraId
                              << "; reopening every " << kRetryMs / 1000 << " s\n";
                captureRelease();
                open = false;
                sleepInterruptibly(kRetryMs);
                continue;
            }

            if (!open.load()) {
                // First frame off this handle: now it is genuinely working, which
                // is the only point at which saying so is worth anything.
                if (failures > 0)
                    std::cerr << "[camera] recovered after " << failures
                              << " failed attempt(s)\n";
                failures = 0;
                reportOpen();
                open = true;
            }

            {
                // Smoothed so one slow frame does not set the budget for every
                // capture after it, and one fast frame does not shrink it.
                const auto now = std::chrono::steady_clock::now();
                if (lastFrameAt.time_since_epoch().count() != 0) {
                    const int gap = static_cast<int>(
                        std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFrameAt)
                            .count());
                    const int prev = frameIntervalMs.load();
                    frameIntervalMs.store(prev == 0 ? gap : (3 * prev + gap) / 4);
                }
                lastFrameAt = now;
            }

            {
                std::lock_guard<std::mutex> lock(mtx);
                latest = std::move(frame);
                ++seq;
            }
            cv.notify_all();
        }
        captureRelease();
        open = false;
        // Tells the destructor it is safe to join rather than detach.
        finishedPromise.set_value();
    }

    // Shutdown gate, for the same reason AxisDevice has one: a caller can be
    // parked in waitForFresh for a second, and the app can be quit during that
    // second. Without this the destructor frees the mutex and condition variable
    // the caller is still sleeping on.
    bool stopping = false;
    int inFlight = 0;
    std::condition_variable drained;

    // Block until a frame newer than the one present on entry arrives.
    cv::Mat waitForFresh(int timeoutMs) {
        std::unique_lock<std::mutex> lock(mtx);
        if (stopping) return {};
        ++inFlight;

        const std::uint64_t start = seq;
        // `stopping` in the predicate as well as a new frame: on shutdown the
        // waiter is woken at once instead of sitting out the timeout, which is
        // what makes quitting feel immediate rather than taking a second per
        // pending capture.
        const bool fresh = cv.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                                       [&] { return seq > start || stopping; }) &&
                           !stopping;
        cv::Mat out = fresh ? latest.clone() : cv::Mat();

        if (--inFlight == 0) drained.notify_all();
        return out;
    }

    // Refuse new reads and wait for the ones in flight, before anything they are
    // using is torn down.
    void closeGate() {
        std::unique_lock<std::mutex> lock(mtx);
        stopping = true;
        cv.notify_all();
        drained.wait_for(lock, std::chrono::seconds(5), [this] { return inFlight == 0; });
    }
};

CameraDevice::CameraDevice(DeviceConfig cfg) : d_(std::make_shared<Impl>()) {
    d_->cfg = std::move(cfg);
    // The thread keeps the state alive by holding its own reference, so it can
    // outlive this object if it has to. See the destructor.
    d_->grabber = std::thread([state = d_] { state->grabLoop(); });
}

CameraDevice::~CameraDevice() {
    // 1. Stop taking new reads and release the ones already waiting.
    d_->closeGate();
    d_->stop = true;

    // 2. Ask the grab thread to finish, but do NOT wait indefinitely.
    //
    // Most of the loop checks `stop` often and exits in milliseconds. One part
    // cannot: cv::VideoCapture::open() on a camera that is absent or wedged
    // blocks for seconds inside the backend, and no flag reaches into it. Waiting
    // there makes closing the window look like a hang -- and an operator who
    // force-kills a hung window on Windows leaves the COM ports held, so the next
    // launch fails with "Access is denied". Trading a tidy join for that is a bad
    // deal.
    //
    // So: a short wait for the common case, then let it go. The thread holds its
    // own reference to the state, so nothing it touches is freed underneath it;
    // the state simply outlives this object by however long the stuck call takes,
    // and is released when the thread returns.
    if (d_->grabber.joinable()) {
        if (d_->finished.wait_for(std::chrono::milliseconds(500)) ==
            std::future_status::ready)
            d_->grabber.join();
        else
            d_->grabber.detach();
    }
}

cv::Mat CameraDevice::frame() {
    // A fixed second was fine while every rig ran msmf: a frame is 48 ms at 21
    // fps, so the wait only expired when the device was gone -- the answer
    // wanted. It stops being fine the moment the stream is slower than the
    // constant. This camera cannot be opened through OpenCV 4.6.0's msmf backend
    // at all and has to run on dshow, which pins it to uncompressed YUY2 and
    // delivers 1.17 fps: 855 ms a frame, against a 1000 ms budget. Captures then
    // succeeded or came back black depending on where in the frame period the
    // button was pressed -- a coin toss, and a black frame that looks like a
    // camera fault rather than a timeout.
    //
    // So the budget is taken from the stream: several frame periods, floored at
    // the old second so a fast camera behaves exactly as before, and capped so a
    // device that has genuinely gone still reports it promptly rather than
    // hanging the capture. Waiting is only ever as long as the camera is slow.
    const int interval = d_->frameIntervalMs.load();
    const int timeoutMs = std::clamp(interval * 4, 1000, 10000);
    return d_->waitForFresh(timeoutMs);
}

cv::Mat CameraDevice::capture_frame() {
    // A CAPTURE must show what is on the glass NOW. frame() cannot promise that.
    //
    // waitForFresh returns the first frame whose seq is greater than the one
    // present on entry -- but that frame was already being exposed, encoded and
    // carried up the USB pipe when the request was made. At 3040x3040 MJPG that
    // is tens to hundreds of milliseconds of work already in flight, so the
    // "fresh" frame is an image of the pattern that was up BEFORE the one just
    // pushed. The operator sees a shot that is always one pattern behind, and
    // nothing about it looks wrong: it is a sharp, correctly exposed picture of
    // the wrong thing, which is the worst kind of measurement error.
    //
    // So a capture throws away the frames that were already on their way and
    // takes the next one after those. Two is deliberate rather than one: it
    // covers a single-frame driver buffer as well as the frame in transit, and
    // costs about 100 ms at 21 fps -- nothing next to the settle time the caller
    // already waits for the panel.
    //
    // The PREVIEW deliberately does not do this. It wants the newest frame going,
    // not a correct one, and dropping two frames there would just make it lag.
    constexpr int kDiscardInFlight = 2;
    cv::Mat img;
    for (int k = 0; k <= kDiscardInFlight; ++k) {
        img = frame();
        if (img.empty()) return {};  // stream stalled; say so rather than guess
    }
    return img;
}

QByteArray CameraDevice::single_image() {
    const cv::Mat img = capture_frame();
    if (img.empty()) return {};

    std::vector<uchar> buffer;
    const bool jpeg = d_->cfg.captureFormat.compare(QLatin1String("png"),
                                                    Qt::CaseInsensitive) != 0;
    const std::vector<int> params =
        jpeg ? std::vector<int>{cv::IMWRITE_JPEG_QUALITY, d_->cfg.jpegQuality}
             : std::vector<int>{};

    if (!cv::imencode(jpeg ? ".jpg" : ".png", img, buffer, params)) {
        std::cerr << "[camera] encode failed\n";
        return {};
    }
    return QByteArray(reinterpret_cast<const char *>(buffer.data()),
                      static_cast<int>(buffer.size()));
}

bool CameraDevice::isOpen() const { return d_->open.load(); }

void CameraDevice::setUrl(const QString &) {
    // There is no address any more; the camera is a device id on this machine.
}

QString CameraDevice::url() const {
    return QStringLiteral("camera id=%1 (%2)")
        .arg(QString::number(d_->cfg.cameraId), d_->cfg.cameraBackend);
}
