import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FisheyeCaliJojo

GridLayout {
    id: root

    columns: 3
    rowSpacing: Theme.dpadSpacing
    columnSpacing: Theme.dpadSpacing

    property alias topSlot:   topSlot
    property alias northSlot: northSlot
    property alias westSlot:  westSlot
    property alias southSlot: southSlot
    property alias eastSlot:  eastSlot

    readonly property real slotMinimumWidth:  Math.max(Theme.minMonitorSlotWidth,  topSlot.minimumWidth)
    readonly property real slotMinimumHeight: Math.max(Theme.minMonitorSlotHeight, topSlot.minimumHeight)

    function turnOffFourSides() {
        northSlot.turnOff()
        westSlot.turnOff()
        southSlot.turnOff()
        eastSlot.turnOff()
    }

    function applyImageToFourSides(path) {
        northSlot.imagePath = path
        westSlot.imagePath = path
        southSlot.imagePath = path
        eastSlot.imagePath = path
    }

    Item {}

    MonitorSlotPanel {
        id: northSlot
        label: qsTr("N")
        direction: "n"
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumWidth: root.slotMinimumWidth
        Layout.minimumHeight: root.slotMinimumHeight

        onBrowseRequested: console.log("[Pattern And Monitor] N: Browse requested")
        onUpdateRequested: (brightness) => PatternController.setMonitorBrightness(northSlot.direction, brightness)
        onTurnOffRequested: PatternController.closeMonitor(northSlot.direction)
    }

    Item {}

    MonitorSlotPanel {
        id: westSlot
        label: qsTr("W")
        direction: "w"
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumWidth: root.slotMinimumWidth
        Layout.minimumHeight: root.slotMinimumHeight

        onBrowseRequested: console.log("[Pattern And Monitor] W: Browse requested")
        onUpdateRequested: (brightness) => PatternController.setMonitorBrightness(westSlot.direction, brightness)
        onTurnOffRequested: PatternController.closeMonitor(westSlot.direction)
    }

    MonitorSlotPanel {
        id: topSlot
        label: qsTr("TOP")
        direction: "top"
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumWidth: root.slotMinimumWidth
        Layout.minimumHeight: root.slotMinimumHeight

        onBrowseRequested: console.log("[Pattern And Monitor] TOP: Browse requested")
        onUpdateRequested: (brightness) => PatternController.setMonitorBrightness(topSlot.direction, brightness)
        onTurnOffRequested: PatternController.closeMonitor(topSlot.direction)
    }

    MonitorSlotPanel {
        id: eastSlot
        label: qsTr("E")
        direction: "e"
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumWidth: root.slotMinimumWidth
        Layout.minimumHeight: root.slotMinimumHeight

        onBrowseRequested: console.log("[Pattern And Monitor] E: Browse requested")
        onUpdateRequested: (brightness) => PatternController.setMonitorBrightness(eastSlot.direction, brightness)
        onTurnOffRequested: PatternController.closeMonitor(eastSlot.direction)
    }

    Item {}

    MonitorSlotPanel {
        id: southSlot
        label: qsTr("S")
        direction: "s"
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumWidth: root.slotMinimumWidth
        Layout.minimumHeight: root.slotMinimumHeight

        onBrowseRequested: console.log("[Pattern And Monitor] S: Browse requested")
        onUpdateRequested: (brightness) => PatternController.setMonitorBrightness(southSlot.direction, brightness)
        onTurnOffRequested: PatternController.closeMonitor(southSlot.direction)
    }

    Item {}
}
