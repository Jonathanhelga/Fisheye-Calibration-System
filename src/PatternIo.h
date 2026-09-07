#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

class PatternIo : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QUrl defaultDirectory READ defaultDirectory CONSTANT)
    Q_PROPERTY(QUrl defaultImageDirectory READ defaultImageDirectory CONSTANT)
    Q_PROPERTY(QString lastError READ lastError NOTIFY changed)

public:
    explicit PatternIo(QObject *parent = nullptr);

    QUrl defaultDirectory() const;
    QUrl defaultImageDirectory() const;
    QString lastError() const { return lastError_; }

    Q_INVOKABLE bool writeText(const QUrl &fileUrl, const QString &text);
    Q_INVOKABLE QString readText(const QUrl &fileUrl);

    // Path <-> URL, done by Qt rather than by string surgery in QML.
    //
    // "file://" + path is wrong on Windows and right on Linux: a Windows path
    // starts with a drive letter and needs three slashes, so the two-slash form
    // makes "C:" the HOST and the image silently fails to load. Both directions
    // are here because QML needs a local path to hand a controller and a URL to
    // hand an Image.
    Q_INVOKABLE QUrl toFileUrl(const QString &path) const;
    Q_INVOKABLE QString toLocalPath(const QUrl &fileUrl) const;

signals:
    void changed();

private:
    static QUrl workspaceSubdirectory(const QString &name);
    bool setError(const QString &message);

    QString lastError_;
};
