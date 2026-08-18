pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FisheyeCaliJojo

ColumnLayout {
    id: rosette

    property string label: ""
    property var directions: []
    property var colors: ({})
    property var paired: []

    readonly property int total: 8

    signal toggled(string direction)
    signal cleared()

    spacing: Theme.labelSpacing

    component Cell: AbstractButton {
        id: cell

        property string direction: ""

        readonly property bool active: rosette.directions.indexOf(direction) >= 0
        readonly property bool pairedHere: rosette.paired.indexOf(direction) >= 0
        readonly property color tint: rosette.colors[direction] !== undefined
                                      ? rosette.colors[direction]
                                      : Theme.accentIdle

        implicitWidth: Theme.rosetteCell
        implicitHeight: Theme.rosetteCell

        hoverEnabled: true
        text: direction.toUpperCase()

        onClicked: rosette.toggled(direction)

        background: Rectangle {
            radius: Theme.radius
            color: cell.active ? cell.tint
                 : cell.hovered ? Theme.fieldDisabledBackground
                                : Theme.fieldBackground
            border.color: cell.active ? "transparent" : Theme.panelBorder

            Behavior on color {
                ColorAnimation { duration: Theme.animFast }
            }
        }

        contentItem: Text {
            text: cell.text
            color: cell.active ? Theme.textOnAccent : Theme.textCaption
            font.pixelSize: Math.round(Theme.rosetteCell * 0.38)
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        Rectangle {
            visible: cell.pairedHere

            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 2

            width: Math.round(Theme.rosetteCell * 0.2)
            height: width
            radius: width / 2
            antialiasing: true
            color: cell.active ? Theme.textOnAccent : Theme.accent
        }
    }

    Label {
        Layout.alignment: Qt.AlignHCenter

        text: rosette.label
        color: Theme.textCaption
        font.pixelSize: Theme.captionFontSize
        font.bold: true
    }

    GridLayout {
        Layout.alignment: Qt.AlignHCenter

        columns: 3
        rowSpacing: Theme.spaceXs
        columnSpacing: Theme.spaceXs

        Cell { direction: "nw" }
        Cell { direction: "n" }
        Cell { direction: "ne" }
        Cell { direction: "w" }

        AbstractButton {
            id: counter

            implicitWidth: Theme.rosetteCell
            implicitHeight: Theme.rosetteCell

            hoverEnabled: true
            enabled: rosette.directions.length > 0

            onClicked: rosette.cleared()

            background: Rectangle {
                radius: Theme.radius
                color: counter.enabled && (counter.down || counter.hovered)
                       ? Theme.danger : "transparent"
                border.color: counter.enabled ? Theme.panelBorder : "transparent"

                Behavior on color {
                    ColorAnimation { duration: Theme.animFast }
                }
            }

            contentItem: Text {
                text: counter.hovered && counter.enabled
                      ? qsTr("clear")
                      : rosette.directions.length + "/" + rosette.total
                color: counter.hovered && counter.enabled ? Theme.textOnAccent
                                                          : Theme.textCaption
                font.pixelSize: Math.round(Theme.rosetteCell * 0.32)
                font.bold: true
                fontSizeMode: Text.HorizontalFit
                minimumPixelSize: Math.round(Theme.rosetteCell * 0.22)
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }

        Cell { direction: "e" }
        Cell { direction: "sw" }
        Cell { direction: "s" }
        Cell { direction: "se" }
    }
}
