import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FisheyeCaliJojo

Rectangle {
    id: toast

    property int holdMs: 3200
    property bool failure: false

    function show(message, isFailure) {
        label.text = message
        toast.failure = (isFailure === true)
        toast.opacity = 1
        life.restart()
    }

    implicitWidth: row.implicitWidth + 2 * Theme.spaceMd
    implicitHeight: row.implicitHeight + 2 * Theme.spaceSm

    radius: Theme.radius
    color: Theme.textPrimary
    opacity: 0
    visible: opacity > 0

    Behavior on opacity {
        NumberAnimation { duration: 160; easing.type: Easing.OutQuad }
    }

    RowLayout {
        id: row

        anchors.centerIn: parent
        spacing: Theme.spaceSm

        Rectangle {
            Layout.alignment: Qt.AlignVCenter
            implicitWidth: Theme.spaceSm
            implicitHeight: Theme.spaceSm
            radius: width / 2
            color: toast.failure ? Theme.statusFailed : Theme.statusOk
        }

        Label {
            id: label
            color: Theme.textOnAccent
            font.pixelSize: Theme.captionFontSize
        }
    }

    Timer {
        id: life
        interval: toast.holdMs
        onTriggered: toast.opacity = 0
    }
}
