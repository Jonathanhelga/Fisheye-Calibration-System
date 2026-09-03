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
    Q_PROPERTY(QString lastError READ lastError NOTIFY changed)

public:
    explicit PatternIo(QObject *parent = nullptr);

    QUrl defaultDirectory() const;
    QString lastError() const { return lastError_; }

    Q_INVOKABLE bool writeText(const QUrl &fileUrl, const QString &text);
    Q_INVOKABLE QString readText(const QUrl &fileUrl);

signals:
    void changed();

private:
    bool setError(const QString &message);

    QString lastError_;
};
