#pragma once

#include <QByteArray>
#include <QImage>
#include <QString>

// The captures, by slot name, held once for the whole process.
//
// Why this exists rather than each controller holding its own copy: a capture is
// wanted by three unrelated pieces of code at three different times, and two of
// them need a DIFFERENT representation of it.
//
//   * CameraController receives it and wants a QImage, to hand the QML image
//     provider something to paint;
//   * ComputeController sends it back out to /compute/detect and wants the
//     ORIGINAL COMPRESSED BYTES -- re-encoding a QImage to PNG would hand the
//     detect ops a picture that is not the one the camera produced, and every
//     centre fit in this system is a measurement of exact pixel values;
//   * the file-open path puts bytes in from disk with no capture involved at all.
//
// Keeping one store of (bytes, format, image) per slot is what lets "Capture",
// "Open Img" and "Snapshot" all feed the same downstream analysis without any of
// them knowing about the others.
//
// Slot names in use: "single", "positive", "negative", "live".
//
// Thread-safe. Frames are written from ROS executor threads and read from the
// GUI thread and from the image provider's own thread, so every accessor takes
// the mutex; the QImage/QByteArray copies handed out are implicitly shared, so
// the copy under the lock is cheap.
namespace ImageStore {

struct Frame {
    QByteArray bytes;   // exactly what arrived on the wire, or was read from disk
    QString format;     // "png" | "jpeg", as CompressedImage spells it
    QImage image;       // decoded, for display
    int width = 0;      // as reported by the source, which can differ from
    int height = 0;     // image.size() when the rig re-encoded
};

// Replaces the slot outright and returns its new revision number. The revision
// is what makes the QML image URL change; without it Qt serves the cached
// picture and a new capture appears to do nothing.
int put(const QString &slot, const QByteArray &bytes, const QString &format, const QImage &image,
        int width, int height);

bool has(const QString &slot);
Frame frame(const QString &slot);
QImage image(const QString &slot);
int revision(const QString &slot);

void clear(const QString &slot);
void clearAll();

}  // namespace ImageStore
