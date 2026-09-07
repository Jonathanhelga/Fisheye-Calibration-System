import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FisheyeCaliJojo

Dialog {
    id: dialog

    modal: true
    standardButtons: Dialog.Close
    padding: Theme.panelMargin

    property int topNumber: 1
    property int northNumber: 2
    property int westNumber: 3
    property int southNumber: 4
    property int eastNumber: 5

    property string statusText: ""

    readonly property bool linked: PatternController.status === ProbeStatus.Ok

    onOpened: dialog.statusText = ""

    Connections {
        target: PatternController

        function onDisplaySetupReplied(ok, message) { dialog.statusText = message }
    }

    header: Label {
        text: qsTr("Setup Monitor Direction")
        padding: Theme.panelMargin
        font.bold: true
        font.pixelSize: Theme.fontTitle
        color: Theme.accent
    }

    background: Rectangle {
        implicitWidth: Theme.unit * 34
        color: Theme.panelBackground
        border.color: Theme.panelBorder
        radius: Theme.radius
    }

    component DirectionSpin: SpinBox {
        id: spin

        from: 1
        to: 16
        editable: true

        implicitHeight: Theme.controlHeight
        implicitWidth: Math.round(Theme.charUnit * 10)

        contentItem: TextInput {
            text: spin.textFromValue(spin.value, spin.locale)
            font.pixelSize: Theme.fontTitle
            font.bold: true
            color: Theme.textPrimary
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            readOnly: !spin.editable
            validator: spin.validator
            selectByMouse: true
        }

        up.indicator: Rectangle {
            x: spin.width - width
            height: spin.height
            implicitWidth: Theme.controlHeight
            radius: Theme.radius
            color: spin.up.pressed ? Theme.accentHover : "transparent"

            Text {
                anchors.centerIn: parent
                text: "+"
                color: Theme.accent
                font.bold: true
            }
        }

        down.indicator: Rectangle {
            height: spin.height
            implicitWidth: Theme.controlHeight
            radius: Theme.radius
            color: spin.down.pressed ? Theme.accentHover : "transparent"

            Text {
                anchors.centerIn: parent
                text: "-"
                color: Theme.accent
                font.bold: true
            }
        }

        background: Rectangle {
            implicitHeight: Theme.controlHeight
            color: Theme.fieldBackground
            border.color: spin.activeFocus ? Theme.accent : Theme.panelBorder
            radius: Theme.radius
        }
    }

    contentItem: ColumnLayout {
        spacing: Theme.rowSpacing

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.textPrimary
            text: qsTr("1. Click \"Show Numbers\" each screen will display a number.\n"
                      + "2. Look at the rig and enter the number shown at each position.\n"
                      + "3. Click \"Apply Mapping\" to send it to the monitor.")
        }

        ActionButton {
            Layout.fillWidth: true
            tone: "accent"
            text: qsTr("Show Numbers on Screens")
            enabled: dialog.linked
            onClicked: PatternController.showDisplayNumbers()
        }

        GridLayout {
            Layout.fillWidth: true
            Layout.topMargin: Theme.spaceSm
            columns: 2
            rowSpacing: Theme.rowSpacing
            columnSpacing: Theme.rowSpacing

            Label { text: qsTr("Top display #"); color: Theme.textCaption }
            DirectionSpin {
                value: dialog.topNumber
                onValueModified: dialog.topNumber = value
            }

            Label { text: qsTr("North (N) display #"); color: Theme.textCaption }
            DirectionSpin {
                value: dialog.northNumber
                onValueModified: dialog.northNumber = value
            }

            Label { text: qsTr("West (W) display #"); color: Theme.textCaption }
            DirectionSpin {
                value: dialog.westNumber
                onValueModified: dialog.westNumber = value
            }

            Label { text: qsTr("South (S) display #"); color: Theme.textCaption }
            DirectionSpin {
                value: dialog.southNumber
                onValueModified: dialog.southNumber = value
            }

            Label { text: qsTr("East (E) display #"); color: Theme.textCaption }
            DirectionSpin {
                value: dialog.eastNumber
                onValueModified: dialog.eastNumber = value
            }
        }

        ActionButton {
            Layout.fillWidth: true
            Layout.topMargin: Theme.spaceSm
            tone: "accent"
            text: qsTr("Apply Mapping")
            enabled: dialog.linked
            onClicked: PatternController.applyDisplayDirection(dialog.topNumber, dialog.northNumber,
                                                               dialog.westNumber, dialog.southNumber,
                                                               dialog.eastNumber)
        }

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            visible: !dialog.linked || dialog.statusText.length > 0
            text: dialog.linked ? dialog.statusText
                                : qsTr("Not connected to the rig, press ROS Update first.")
            color: Theme.textCaption
            font.pixelSize: Theme.captionFontSize
        }
    }
}
