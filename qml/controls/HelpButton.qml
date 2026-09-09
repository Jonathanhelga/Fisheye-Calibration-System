import QtQuick
import QtQuick.Controls
import FisheyeCaliJojo

AbstractButton {
    id: control

    property string page: ""

    hoverEnabled: true

    leftPadding:  Theme.spaceSm
    rightPadding: Theme.spaceSm

    implicitWidth: label.implicitWidth + leftPadding + rightPadding
    implicitHeight: Math.round(Theme.unit * 1.25)

    background: Rectangle {
        radius: Theme.radius
        color: control.down || control.hovered ? Theme.accent : "transparent"
        border.color: control.down || control.hovered ? Theme.accent : Theme.panelBorder
        border.width: 1

        Behavior on color {
            ColorAnimation { duration: Theme.animFast }
        }
    }

    contentItem: Text {
        id: label

        text: qsTr("HELP")
        color: control.down || control.hovered ? Theme.textOnAccent : Theme.textCaption
        font.pixelSize: Theme.captionFontSize
        font.bold: true
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    ToolTip.visible: control.hovered
    ToolTip.delay: Theme.noticeTimeout / 10
    ToolTip.text: qsTr("Help for this page")

    onClicked: console.log("[Help] documentation requested for", control.page)
}
