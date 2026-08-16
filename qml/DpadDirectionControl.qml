pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FisheyeCaliJojo

Item {
    id: pad

    property bool horizontalEnabled: true
    property bool danger: false
    property string centerText: ""

    signal directionClicked(string direction)

    readonly property int columnCount: horizontalEnabled ? 3 : 1
    readonly property int cellSize: Math.max(
        Theme.padButtonSize,
        Math.floor(Math.min((width - (columnCount - 1) * Theme.labelSpacing) / columnCount,
                            (height - 2 * Theme.labelSpacing) / 3)))

    implicitWidth: columnCount * Theme.padButtonSize + (columnCount - 1) * Theme.labelSpacing
    implicitHeight: 3 * Theme.padButtonSize + 2 * Theme.labelSpacing

    component PadButton: Button {
        id: control

        implicitWidth: pad.cellSize
        implicitHeight: pad.cellSize

        background: Rectangle {
            radius: Theme.radius
            color: control.down ? (pad.danger ? Theme.danger : Theme.accent)
                 : control.hovered ? (pad.danger ? Theme.dangerHover : Theme.accentHover)
                 : (pad.danger ? Theme.dangerIdle : Theme.accentIdle)

            Behavior on color {
                ColorAnimation { duration: Theme.animFast }
            }
        }

        contentItem: Text {
            text: control.text
            color: Theme.textOnAccent
            font.pixelSize: Math.round(pad.cellSize * 0.4)
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    GridLayout {
        anchors.centerIn: parent

        columns: pad.columnCount
        rowSpacing: Theme.labelSpacing
        columnSpacing: Theme.labelSpacing

        Item {
            visible: pad.horizontalEnabled
            Layout.preferredWidth: pad.cellSize
            Layout.preferredHeight: pad.cellSize
        }

        PadButton {
            text: "▲"
            onClicked: pad.directionClicked("up")
        }

        Item {
            visible: pad.horizontalEnabled
            Layout.preferredWidth: pad.cellSize
            Layout.preferredHeight: pad.cellSize
        }

        PadButton {
            visible: pad.horizontalEnabled
            text: "◀"
            onClicked: pad.directionClicked("left")
        }

        Label {
            Layout.preferredWidth: pad.cellSize
            Layout.preferredHeight: pad.cellSize
            text: pad.centerText
            color: Theme.textCaption
            font.pixelSize: Theme.captionFontSize
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        PadButton {
            visible: pad.horizontalEnabled
            text: "▶"
            onClicked: pad.directionClicked("right")
        }

        Item {
            visible: pad.horizontalEnabled
            Layout.preferredWidth: pad.cellSize
            Layout.preferredHeight: pad.cellSize
        }

        PadButton {
            text: "▼"
            onClicked: pad.directionClicked("down")
        }

        Item {
            visible: pad.horizontalEnabled
            Layout.preferredWidth: pad.cellSize
            Layout.preferredHeight: pad.cellSize
        }
    }
}
