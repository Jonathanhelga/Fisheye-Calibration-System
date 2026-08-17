import QtQuick
import FisheyeCaliJojo

Rectangle {
    id: swatch

    implicitWidth: Math.round(Theme.charUnit * 5)
    implicitHeight: Theme.controlHeight

    border.color: Theme.panelBorder
    radius: Theme.radius
}
