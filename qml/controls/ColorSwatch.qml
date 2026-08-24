import QtQuick
import FisheyeCaliJojo

Rectangle {
    id: swatch

    property bool selected: false

    implicitWidth: Math.round(Theme.charUnit * 5)
    implicitHeight: Theme.controlHeight

    border.color: selected ? Theme.textPrimary : Theme.panelBorder
    border.width: selected ? 2 : 1
    radius: Theme.radius
}
