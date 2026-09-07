#include "ImageStore.h"

#include <QHash>
#include <QMutex>

namespace {

QMutex storeMutex;
QHash<QString, ImageStore::Frame> frames;
QHash<QString, int> revisions;

}  // namespace

namespace ImageStore {

int put(const QString &slot, const QByteArray &bytes, const QString &format, const QImage &image,
        int width, int height) {
    QMutexLocker locker(&storeMutex);

    Frame f;
    f.bytes = bytes;
    f.format = format;
    f.image = image;
    f.width = width > 0 ? width : image.width();
    f.height = height > 0 ? height : image.height();

    frames.insert(slot, f);
    return ++revisions[slot];
}

bool has(const QString &slot) {
    QMutexLocker locker(&storeMutex);
    const auto it = frames.constFind(slot);
    return it != frames.constEnd() && !it->image.isNull();
}

Frame frame(const QString &slot) {
    QMutexLocker locker(&storeMutex);
    return frames.value(slot);
}

QImage image(const QString &slot) {
    QMutexLocker locker(&storeMutex);
    return frames.value(slot).image;
}

int revision(const QString &slot) {
    QMutexLocker locker(&storeMutex);
    return revisions.value(slot, 0);
}

void clear(const QString &slot) {
    QMutexLocker locker(&storeMutex);
    frames.remove(slot);
    // The revision is deliberately NOT reset. A slot that is cleared and refilled
    // must not reuse a URL Qt has already cached, or the old picture comes back.
    ++revisions[slot];
}

void clearAll() {
    QMutexLocker locker(&storeMutex);
    for (auto it = frames.constBegin(); it != frames.constEnd(); ++it) ++revisions[it.key()];
    frames.clear();
}

}  // namespace ImageStore
