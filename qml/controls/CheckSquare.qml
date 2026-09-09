import QtQuick
import QtQuick.Controls
import FisheyeCaliJojo

Rectangle {
    id: box

    property bool checked: false

    readonly property alias hovered: area.containsMouse

    signal toggled(bool checked)

    implicitWidth: Math.round(Theme.unit)
    implicitHeight: Math.round(Theme.unit)
    radius: Theme.radius

    color: box.checked ? Theme.accent
         : !box.enabled ? Theme.fieldDisabledBackground
                        : Theme.fieldBackground
    border.color: box.checked ? Theme.accent
                : area.containsMouse ? Theme.accentHover
                                     : Theme.panelBorder

    Behavior on color {
        ColorAnimation { duration: Theme.animFast }
    }

    Label {
        anchors.centerIn: parent
        text: "✓"
        visible: box.checked
        color: Theme.textOnAccent
        font.pixelSize: Math.round(Theme.unit * 0.7)
        font.bold: true
    }

    MouseArea {
        id: area

        anchors.fill: parent
        hoverEnabled: true
        enabled: box.enabled
        cursorShape: Qt.PointingHandCursor
        onClicked: box.toggled(!box.checked)
    }
}
