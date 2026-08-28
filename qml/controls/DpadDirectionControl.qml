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

    property string homeText: ""
    property bool homeEnabled: true

    property bool upEnabled: true
    property bool downEnabled: true
    property bool leftEnabled: true
    property bool rightEnabled: true

    property string activeDirection: ""

    signal directionClicked(string direction)
    signal homeClicked()

    readonly property int columnCount: horizontalEnabled ? 3 : 1
    readonly property int cellSize: Theme.padButtonSize

    implicitWidth: columnCount * Theme.padButtonSize + (columnCount - 1) * Theme.dpadSpacing
    implicitHeight: 3 * Theme.padButtonSize + 2 * Theme.dpadSpacing

    component PadButton: Button {
        id: control

        property string direction: ""

        readonly property bool active: control.direction.length > 0
                                       && control.direction === pad.activeDirection

        implicitWidth: pad.cellSize
        implicitHeight: pad.cellSize

        background: Rectangle {
            radius: Theme.radius
            color: control.active ? (pad.danger ? Theme.danger : Theme.accent)
                 : !control.enabled ? Theme.fieldDisabledBackground
                 : control.down ? (pad.danger ? Theme.danger : Theme.accent)
                 : control.hovered ? (pad.danger ? Theme.dangerHover : Theme.accentHover)
                 : (pad.danger ? Theme.dangerIdle : Theme.accentIdle)

            Behavior on color {
                ColorAnimation { duration: Theme.animFast }
            }
        }

        contentItem: Text {
            text: control.text
            color: control.enabled || control.active ? Theme.textOnAccent : Theme.textDisabled
            font.pixelSize: Math.round(pad.cellSize * 0.4)
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    GridLayout {
        anchors.centerIn: parent

        columns: pad.columnCount
        rowSpacing: Theme.dpadSpacing
        columnSpacing: Theme.dpadSpacing

        Item {
            visible: pad.horizontalEnabled
            Layout.preferredWidth: pad.cellSize
            Layout.preferredHeight: pad.cellSize
        }

        PadButton {
            text: "▲"
            enabled: pad.upEnabled
            direction: "up"
            onClicked: pad.directionClicked("up")
        }

        Item {
            visible: pad.horizontalEnabled
            Layout.preferredWidth: pad.cellSize
            Layout.preferredHeight: pad.cellSize
        }

        PadButton {
            visible: pad.horizontalEnabled
            enabled: pad.leftEnabled
            text: "◀"
            direction: "left"
            onClicked: pad.directionClicked("left")
        }

        Item {
            Layout.preferredWidth: pad.cellSize
            Layout.preferredHeight: pad.cellSize

            Label {
                anchors.fill: parent
                visible: pad.homeText.length === 0
                text: pad.centerText
                color: Theme.textCaption
                font.pixelSize: Theme.captionFontSize
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            Button {
                id: homeButton

                anchors.fill: parent
                visible: pad.homeText.length > 0
                enabled: pad.homeEnabled
                padding: Theme.spaceXs

                background: Rectangle {
                    radius: Theme.radius
                    color: homeButton.enabled && (homeButton.down || homeButton.hovered)
                           ? Theme.accent : "transparent"
                    border.color: homeButton.enabled ? Theme.accent : Theme.panelBorder

                    Behavior on color {
                        ColorAnimation { duration: Theme.animFast }
                    }
                }

                contentItem: Text {
                    text: pad.homeText
                    color: !homeButton.enabled ? Theme.textDisabled
                         : homeButton.down || homeButton.hovered ? Theme.textOnAccent
                                                                 : Theme.accent
                    font.pixelSize: Theme.captionFontSize
                    font.bold: true
                    fontSizeMode: Text.HorizontalFit
                    minimumPixelSize: Math.round(Theme.captionFontSize * 0.6)
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                onClicked: pad.homeClicked()
            }
        }

        PadButton {
            visible: pad.horizontalEnabled
            enabled: pad.rightEnabled
            text: "▶"
            direction: "right"
            onClicked: pad.directionClicked("right")
        }

        Item {
            visible: pad.horizontalEnabled
            Layout.preferredWidth: pad.cellSize
            Layout.preferredHeight: pad.cellSize
        }

        PadButton {
            text: "▼"
            enabled: pad.downEnabled
            direction: "down"
            onClicked: pad.directionClicked("down")
        }

        Item {
            visible: pad.horizontalEnabled
            Layout.preferredWidth: pad.cellSize
            Layout.preferredHeight: pad.cellSize
        }
    }
}
