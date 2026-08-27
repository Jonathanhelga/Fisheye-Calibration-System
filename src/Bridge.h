#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>

// The C++ side of the QML <-> C++ conversation, same role ControllerMain
// plays in the old widgets app: QML calls Q_INVOKABLE methods, and reads/
// writes Q_PROPERTY values.
class Bridge : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(int clickCount READ clickCount NOTIFY clickCountChanged)

public:
    explicit Bridge(QObject *parent = nullptr) : QObject(parent) {}

    int clickCount() const { return clickCount_; }

    Q_INVOKABLE void handleClick() {
        ++clickCount_;
        emit clickCountChanged();
    }

signals:
    void clickCountChanged();

private:
    int clickCount_ = 0;
};
