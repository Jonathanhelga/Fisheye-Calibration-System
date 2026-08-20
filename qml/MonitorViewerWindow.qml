import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import FisheyeCaliJojo

Window {
    id: root

    title: qsTr("Monitor Viewer")
    color: Theme.panelBackground

    width:  content.implicitWidth + 2 * Theme.spaceMd
    height: content.implicitHeight + 2 * Theme.spaceMd
    minimumWidth: Theme.unit * 62
    minimumHeight: Theme.unit * 30

    property alias topSlot:   topSlot
    property alias northSlot: northSlot
    property alias westSlot:  westSlot
    property alias southSlot: southSlot
    property alias eastSlot:  eastSlot

    property alias directionDialog: directionDialog
    property alias fourSideImagePath: fourSidePathField.text

    signal showNumbersRequested()
    signal applyMappingRequested(int top, int north, int west, int south, int east)
    signal fourSideBrowseRequested()

    function turnOffFourSides() {
        northSlot.turnOff()
        westSlot.turnOff()
        southSlot.turnOff()
        eastSlot.turnOff()
    }

    function applyImageToFourSides() {
        const path = fourSidePathField.text
        northSlot.imagePath = path
        westSlot.imagePath = path
        southSlot.imagePath = path
        eastSlot.imagePath = path
    }

    ColumnLayout {
        id: content

        anchors.fill: parent
        anchors.margins: Theme.spaceMd
        spacing: Theme.spaceMd

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.rowSpacing

            Label {
                text: qsTr("Monitor Viewer")
                font.bold: true
                font.pixelSize: Theme.fontTitle
                color: Theme.accent
            }

            Label {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredWidth: 0
                text: qsTr("Push a pattern image and brightness to each screen around the rig.")
                color: Theme.textCaption
                font.pixelSize: Theme.captionFontSize
                elide: Text.ElideRight
            }

            ActionButton {
                text: qsTr("Setup Monitor Direction")
                onClicked: directionDialog.open()

                ToolTip.visible: hovered
                ToolTip.delay: Theme.animSlow
                ToolTip.text: qsTr("Map each physical display to TOP / N / W / S / E. Required once before patterns will show.")
            }

            ActionButton {
                tone: "danger"
                text: qsTr("4-Side Off (N/W/S/E)")
                onClicked: root.turnOffFourSides()

                ToolTip.visible: hovered
                ToolTip.delay: Theme.animSlow
                ToolTip.text: qsTr("Turns off North, West, South, and East together. TOP is left as is.")
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.labelSpacing

            Label {
                text: qsTr("Apply image to N/W/S/E:")
                color: Theme.textCaption
                font.pixelSize: Theme.captionFontSize
            }

            TextField {
                id: fourSidePathField

                Layout.fillWidth: true
                color: Theme.textPrimary
                font.pixelSize: Theme.captionFontSize
                selectByMouse: true

                background: Rectangle {
                    implicitHeight: Theme.controlHeight
                    color: Theme.fieldBackground
                    border.color: fourSidePathField.activeFocus ? Theme.accent : Theme.panelBorder
                    radius: Theme.radius
                }
            }

            ActionButton {
                text: qsTr("Browse...")
                onClicked: {
                    console.log("[Monitor Viewer] 4-Side: Browse requested")
                    root.fourSideBrowseRequested()
                }
            }

            ActionButton {
                text: qsTr("Apply to 4 Sides")
                tone: "accent"
                onClicked: root.applyImageToFourSides()
            }
        }

        // Temporary: log each signal so the buttons are testable before the
        // real C++ monitor controller is wired up. Remove once that lands.
        RowLayout {
            id: slots

            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.spaceMd

            MonitorSlotPanel {
                id: topSlot
                label: qsTr("TOP")
                Layout.fillWidth: true
                Layout.fillHeight: true

                onBrowseRequested: console.log("[Monitor Viewer] TOP: Browse requested")
                onUpdateRequested: (brightness) => console.log("[Monitor Viewer] TOP: Update requested, brightness=" + brightness)
                onTurnOffRequested: console.log("[Monitor Viewer] TOP: Turn off requested")
            }

            MonitorSlotPanel {
                id: northSlot
                label: qsTr("N")
                Layout.fillWidth: true
                Layout.fillHeight: true

                onBrowseRequested: console.log("[Monitor Viewer] N: Browse requested")
                onUpdateRequested: (brightness) => console.log("[Monitor Viewer] N: Update requested, brightness=" + brightness)
                onTurnOffRequested: console.log("[Monitor Viewer] N: Turn off requested")
            }

            MonitorSlotPanel {
                id: westSlot
                label: qsTr("W")
                Layout.fillWidth: true
                Layout.fillHeight: true

                onBrowseRequested: console.log("[Monitor Viewer] W: Browse requested")
                onUpdateRequested: (brightness) => console.log("[Monitor Viewer] W: Update requested, brightness=" + brightness)
                onTurnOffRequested: console.log("[Monitor Viewer] W: Turn off requested")
            }

            MonitorSlotPanel {
                id: southSlot
                label: qsTr("S")
                Layout.fillWidth: true
                Layout.fillHeight: true

                onBrowseRequested: console.log("[Monitor Viewer] S: Browse requested")
                onUpdateRequested: (brightness) => console.log("[Monitor Viewer] S: Update requested, brightness=" + brightness)
                onTurnOffRequested: console.log("[Monitor Viewer] S: Turn off requested")
            }

            MonitorSlotPanel {
                id: eastSlot
                label: qsTr("E")
                Layout.fillWidth: true
                Layout.fillHeight: true

                onBrowseRequested: console.log("[Monitor Viewer] E: Browse requested")
                onUpdateRequested: (brightness) => console.log("[Monitor Viewer] E: Update requested, brightness=" + brightness)
                onTurnOffRequested: console.log("[Monitor Viewer] E: Turn off requested")
            }
        }
    }

    MonitorDirectionDialog {
        id: directionDialog
        anchors.centerIn: parent

        onShowNumbersRequested: {
            console.log("[Monitor Viewer] Show Numbers on Screens requested")
            root.showNumbersRequested()
        }
        onApplyMappingRequested: (top, north, west, south, east) => {
            console.log("[Monitor Viewer] Apply Mapping requested: top=" + top
                + " n=" + north + " w=" + west + " s=" + south + " e=" + east)
            root.applyMappingRequested(top, north, west, south, east)
        }
    }
}
