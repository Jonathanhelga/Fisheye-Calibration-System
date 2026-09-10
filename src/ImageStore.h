#pragma once

#include <QByteArray>
#include <QImage>
#include <QString>

// Captures by slot, held once process-wide. Thread-safe.
namespace ImageStore {

struct Frame {
    QByteArray bytes;   // as received, or read from disk
    QString format;     // "png" | "jpeg"
    QImage image;       // decoded, for display
    int width = 0;      // as reported; may differ from
    int height = 0;     // image.size()
};

// Replaces the slot; returns its new revision.
int put(const QString &slot, const QByteArray &bytes, const QString &format, const QImage &image,
        int width, int height);

bool has(const QString &slot);
Frame frame(const QString &slot);
QImage image(const QString &slot);
int revision(const QString &slot);

void clear(const QString &slot);
void clearAll();

}  // namespace ImageStore
