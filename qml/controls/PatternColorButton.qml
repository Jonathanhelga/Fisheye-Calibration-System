pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import FisheyeCaliJojo

// Colour swatch that opens a palette popup.
AbstractButton {
    id: control

    property color value: "black"
    readonly property var swatchColors: ["#000000", "#ffffff", "#b4b4b4"].concat(Theme.curvePalette)

    signal picked(color value)

    hoverEnabled: true
    implicitWidth: Theme.controlHeight
    implicitHeight: Theme.controlHeight

    onClicked: popup.opened ? popup.close() : popup.open()

    background: Rectangle {
        radius: Theme.radius
        color: control.value
        border.color: control.hovered ? Theme.accent : Theme.panelBorder
        border.width: control.hovered ? 2 : 1
    }

    Popup {
        id: popup

        y: control.height + Theme.spaceXs
        padding: Theme.spaceSm
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

        background: Rectangle {
            color: Theme.panelBackground
            border.color: Theme.panelBorder
            radius: Theme.radius
        }

        contentItem: Grid {
            columns: 5
            spacing: Theme.spaceXs

            Repeater {
                model: control.swatchColors

                ColorSwatch {
                    id: chip

                    required property string modelData

                    implicitWidth: Math.round(Theme.charUnit * 3)
                    implicitHeight: Math.round(Theme.controlHeight * 0.8)
                    color: modelData
                    selected: Qt.colorEqual(control.value, modelData)

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            control.value = chip.modelData
                            control.picked(chip.modelData)
                            popup.close()
                        }
                    }
                }
            }
        }
    }
}
