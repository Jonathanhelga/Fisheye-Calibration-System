#include "camera_mf.h"

#ifdef _WIN32

// Some of the test targets already define this on the command line, and a
// redefinition is a warning in a build that otherwise has none.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <mutex>
#include <vector>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "ole32.lib")

namespace {

// MFStartup is process-wide and reference counted. It is started once and never
// shut down: the alternative is shutting it down from whichever thread happens
// to release the camera last, while other MF objects may still be alive, and the
// only thing that buys is tidiness during a teardown the process does not
// survive anyway.
void ensureMediaFoundation() {
    static std::once_flag once;
    std::call_once(once, [] {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        MFStartup(MF_VERSION);
    });
}

template <class T>
void safeRelease(T **p) {
    if (*p) {
        (*p)->Release();
        *p = nullptr;
    }
}

QString fourccOf(const GUID &subtype) {
    // MF's video subtype GUIDs are the fourcc in Data1 for every format this
    // camera offers, so the readable name falls straight out of it.
    char cc[5] = {0};
    for (int i = 0; i < 4; ++i) cc[i] = static_cast<char>((subtype.Data1 >> (8 * i)) & 0xFF);
    for (int i = 0; i < 4; ++i)
        if (cc[i] < 32 || cc[i] > 126) return QStringLiteral("?");
    return QString::fromLatin1(cc, 4);
}

// Lower is better. Bandwidth is what decides this: at 3040x3040 an MJPG frame is
// about 0.25 MB and a YUY2 frame is 18.5 MB, and it is that difference -- not the
// backend name -- that separates a live view from a slideshow. MJPG also matches
// what the detectors were tuned against, since the old camera node published JPEG.
int formatRank(const GUID &subtype) {
    if (subtype == MFVideoFormat_MJPG) return 0;
    if (subtype == MFVideoFormat_NV12) return 1;
    if (subtype == MFVideoFormat_YUY2) return 2;
    if (subtype == MFVideoFormat_RGB32) return 3;
    if (subtype == MFVideoFormat_RGB24) return 4;
    return 100;  // something we cannot convert; only taken if nothing else exists
}

bool convertible(const GUID &subtype) { return formatRank(subtype) < 100; }

}  // namespace

struct MfCapture::Impl {
    IMFMediaSource *source = nullptr;
    IMFSourceReader *reader = nullptr;

    int width = 0;
    int height = 0;
    GUID subtype = GUID_NULL;

    // Reused across frames so a 3040x3040 capture is not a fresh 18 MB
    // allocation every time round the loop.
    std::vector<uchar> scratch;

    void close() {
        safeRelease(&reader);
        if (source) {
            source->Shutdown();
            safeRelease(&source);
        }
        width = height = 0;
        subtype = GUID_NULL;
    }

    ~Impl() { close(); }
};

MfCapture::MfCapture() : d_(std::make_unique<Impl>()) {}
MfCapture::~MfCapture() = default;

bool MfCapture::open(int index, int wantW, int wantH, QString *err) {
    const auto fail = [&](const QString &m) {
        if (err) *err = m;
        d_->close();
        return false;
    };

    ensureMediaFoundation();
    d_->close();

    IMFAttributes *attr = nullptr;
    if (FAILED(MFCreateAttributes(&attr, 1))) return fail(QStringLiteral("MFCreateAttributes failed"));
    attr->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
                  MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);

    IMFActivate **devices = nullptr;
    UINT32 count = 0;
    const HRESULT eh = MFEnumDeviceSources(attr, &devices, &count);
    safeRelease(&attr);
    if (FAILED(eh)) return fail(QStringLiteral("MFEnumDeviceSources failed (0x%1)")
                                    .arg(static_cast<quint32>(eh), 8, 16, QLatin1Char('0')));
    if (index < 0 || static_cast<UINT32>(index) >= count) {
        for (UINT32 i = 0; i < count; ++i) devices[i]->Release();
        CoTaskMemFree(devices);
        return fail(QStringLiteral("no capture device %1 (%2 present)").arg(index).arg(count));
    }

    const HRESULT ah = devices[index]->ActivateObject(IID_PPV_ARGS(&d_->source));
    for (UINT32 i = 0; i < count; ++i) devices[i]->Release();
    CoTaskMemFree(devices);
    if (FAILED(ah))
        return fail(QStringLiteral("device %1 could not be activated (0x%2) -- something else "
                                   "may have it open")
                        .arg(index)
                        .arg(static_cast<quint32>(ah), 8, 16, QLatin1Char('0')));

    if (FAILED(MFCreateSourceReaderFromMediaSource(d_->source, nullptr, &d_->reader)))
        return fail(QStringLiteral("MFCreateSourceReaderFromMediaSource failed"));

    // Pick a mode. Exact size wins over everything, because a calibration frame
    // that is not the panel's own pixel grid is the failure this app exists to
    // avoid; among equal sizes the cheapest format wins.
    int bestIndex = -1;
    int bestRank = 0;
    long long bestAreaGap = 0;
    for (DWORD i = 0;; ++i) {
        IMFMediaType *mt = nullptr;
        if (FAILED(d_->reader->GetNativeMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, i, &mt)))
            break;

        UINT32 w = 0, h = 0;
        GUID sub = GUID_NULL;
        MFGetAttributeSize(mt, MF_MT_FRAME_SIZE, &w, &h);
        mt->GetGUID(MF_MT_SUBTYPE, &sub);
        mt->Release();

        if (!convertible(sub) || w == 0 || h == 0) continue;

        const long long areaGap = std::llabs(static_cast<long long>(w) * h -
                                             static_cast<long long>(wantW) * wantH);
        const int rank = formatRank(sub);
        if (bestIndex < 0 || areaGap < bestAreaGap ||
            (areaGap == bestAreaGap && rank < bestRank)) {
            bestIndex = static_cast<int>(i);
            bestRank = rank;
            bestAreaGap = areaGap;
        }
    }
    if (bestIndex < 0) return fail(QStringLiteral("camera offers no format this app can decode"));

    IMFMediaType *chosen = nullptr;
    if (FAILED(d_->reader->GetNativeMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                                              static_cast<DWORD>(bestIndex), &chosen)))
        return fail(QStringLiteral("chosen media type disappeared"));

    UINT32 w = 0, h = 0;
    MFGetAttributeSize(chosen, MF_MT_FRAME_SIZE, &w, &h);
    chosen->GetGUID(MF_MT_SUBTYPE, &d_->subtype);
    const HRESULT sh =
        d_->reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, chosen);
    chosen->Release();
    if (FAILED(sh)) return fail(QStringLiteral("SetCurrentMediaType failed (0x%1)")
                                    .arg(static_cast<quint32>(sh), 8, 16, QLatin1Char('0')));

    d_->width = static_cast<int>(w);
    d_->height = static_cast<int>(h);
    return true;
}

bool MfCapture::read(cv::Mat *out) {
    if (!d_->reader || !out) return false;

    // A read can legitimately come back with no sample: MF signals gaps in the
    // stream with a tick and no data. Those are skipped rather than reported as
    // failures, which would send the caller off reopening a camera that is fine.
    for (int attempt = 0; attempt < 64; ++attempt) {
        DWORD streamIndex = 0, flags = 0;
        LONGLONG timestamp = 0;
        IMFSample *sample = nullptr;
        const HRESULT rh = d_->reader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0,
                                                  &streamIndex, &flags, &timestamp, &sample);
        if (FAILED(rh)) {
            safeRelease(&sample);
            return false;
        }
        if (flags & MF_SOURCE_READERF_ENDOFSTREAM) {
            safeRelease(&sample);
            return false;
        }
        if (flags & MF_SOURCE_READERF_ERROR) {
            safeRelease(&sample);
            return false;
        }
        if (!sample) continue;  // stream tick

        IMFMediaBuffer *buffer = nullptr;
        if (FAILED(sample->ConvertToContiguousBuffer(&buffer))) {
            safeRelease(&sample);
            return false;
        }

        BYTE *data = nullptr;
        DWORD maxLen = 0, curLen = 0;
        if (FAILED(buffer->Lock(&data, &maxLen, &curLen))) {
            safeRelease(&buffer);
            safeRelease(&sample);
            return false;
        }

        bool ok = false;
        const int w = d_->width;
        const int h = d_->height;

        if (d_->subtype == MFVideoFormat_MJPG) {
            d_->scratch.assign(data, data + curLen);
            cv::Mat decoded = cv::imdecode(d_->scratch, cv::IMREAD_COLOR);
            if (!decoded.empty()) {
                *out = std::move(decoded);
                ok = true;
            }
        } else if (d_->subtype == MFVideoFormat_NV12) {
            // Y plane then interleaved chroma at half height: 1.5 bytes a pixel.
            if (curLen >= static_cast<DWORD>(w) * h * 3 / 2) {
                const cv::Mat yuv(h * 3 / 2, w, CV_8UC1, data);
                cv::cvtColor(yuv, *out, cv::COLOR_YUV2BGR_NV12);
                ok = true;
            }
        } else if (d_->subtype == MFVideoFormat_YUY2) {
            if (curLen >= static_cast<DWORD>(w) * h * 2) {
                const cv::Mat yuv(h, w, CV_8UC2, data);
                cv::cvtColor(yuv, *out, cv::COLOR_YUV2BGR_YUY2);
                ok = true;
            }
        } else if (d_->subtype == MFVideoFormat_RGB32) {
            if (curLen >= static_cast<DWORD>(w) * h * 4) {
                const cv::Mat bgra(h, w, CV_8UC4, data);
                cv::cvtColor(bgra, *out, cv::COLOR_BGRA2BGR);
                ok = true;
            }
        } else if (d_->subtype == MFVideoFormat_RGB24) {
            if (curLen >= static_cast<DWORD>(w) * h * 3) {
                // RGB24 from MF is bottom-up, and a calibration image that is
                // upside down would be found much later than it should be.
                const cv::Mat rgb(h, w, CV_8UC3, data);
                cv::flip(rgb, *out, 0);
                ok = true;
            }
        }

        buffer->Unlock();
        safeRelease(&buffer);
        safeRelease(&sample);

        if (ok) return true;
        return false;  // right format, unusable payload: let the caller reopen
    }
    return false;
}

void MfCapture::release() { d_->close(); }
bool MfCapture::isOpened() const { return d_->reader != nullptr; }
int MfCapture::width() const { return d_->width; }
int MfCapture::height() const { return d_->height; }
QString MfCapture::format() const { return fourccOf(d_->subtype); }

#else  // !_WIN32

// Media Foundation is a Windows API. Off Windows the class still exists so
// camera_device.cpp needs no #ifdef, and it simply never opens -- the backend
// name is only ever chosen on the rig.
struct MfCapture::Impl {};

MfCapture::MfCapture() : d_(std::make_unique<Impl>()) {}
MfCapture::~MfCapture() = default;

bool MfCapture::open(int, int, int, QString *err) {
    if (err) *err = QStringLiteral("the \"mf\" backend is Windows-only");
    return false;
}
bool MfCapture::read(cv::Mat *) { return false; }
void MfCapture::release() {}
bool MfCapture::isOpened() const { return false; }
int MfCapture::width() const { return 0; }
int MfCapture::height() const { return 0; }
QString MfCapture::format() const { return {}; }

#endif
