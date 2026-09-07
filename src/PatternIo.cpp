#include "PatternIo.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>

PatternIo::PatternIo(QObject *parent) : QObject(parent) {}

QUrl PatternIo::workspaceSubdirectory(const QString &name) {
    QString base = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (base.isEmpty()) base = QDir::homePath();

    const QString path = base + QStringLiteral("/MoilFisheyeCali/") + name;
    QDir().mkpath(path);

    return QUrl::fromLocalFile(path);
}

QUrl PatternIo::defaultDirectory() const {
    return workspaceSubdirectory(QStringLiteral("pattern_json"));
}

QUrl PatternIo::defaultImageDirectory() const {
    return workspaceSubdirectory(QStringLiteral("pattern_image"));
}

bool PatternIo::setError(const QString &message) {
    if (lastError_ != message) {
        lastError_ = message;
        emit changed();
    }
    return message.isEmpty();
}

bool PatternIo::writeText(const QUrl &fileUrl, const QString &text) {
    if (!fileUrl.isLocalFile()) return setError(tr("%1 is not a local file").arg(fileUrl.toString()));

    const QString path = fileUrl.toLocalFile();
    const QDir dir = QFileInfo(path).absoluteDir();
    if (!dir.exists() && !QDir().mkpath(dir.absolutePath()))
        return setError(tr("could not create %1").arg(dir.absolutePath()));

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return setError(file.errorString());

    const QByteArray bytes = text.toUtf8();
    if (file.write(bytes) != bytes.size()) return setError(file.errorString());
    if (!file.commit()) return setError(file.errorString());

    return setError(QString());
}

QString PatternIo::readText(const QUrl &fileUrl) {
    if (!fileUrl.isLocalFile()) {
        setError(tr("%1 is not a local file").arg(fileUrl.toString()));
        return QString();
    }

    QFile file(fileUrl.toLocalFile());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setError(file.errorString());
        return QString();
    }

    const QByteArray bytes = file.readAll();
    if (file.error() != QFileDevice::NoError) {
        setError(file.errorString());
        return QString();
    }
    if (bytes.isEmpty()) {
        setError(tr("the file is empty"));
        return QString();
    }

    setError(QString());
    return QString::fromUtf8(bytes);
}
