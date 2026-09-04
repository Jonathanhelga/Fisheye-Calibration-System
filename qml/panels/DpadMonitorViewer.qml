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
        onUpdateRequested: (brightness) => console.log("[Pattern And Monitor] N: Update requested, brightness=" + brightness)
        onTurnOffRequested: console.log("[Pattern And Monitor] N: Turn off requested")
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
        onUpdateRequested: (brightness) => console.log("[Pattern And Monitor] W: Update requested, brightness=" + brightness)
        onTurnOffRequested: console.log("[Pattern And Monitor] W: Turn off requested")
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
        onUpdateRequested: (brightness) => console.log("[Pattern And Monitor] TOP: Update requested, brightness=" + brightness)
        onTurnOffRequested: console.log("[Pattern And Monitor] TOP: Turn off requested")
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
        onUpdateRequested: (brightness) => console.log("[Pattern And Monitor] E: Update requested, brightness=" + brightness)
        onTurnOffRequested: console.log("[Pattern And Monitor] E: Turn off requested")
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
        onUpdateRequested: (brightness) => console.log("[Pattern And Monitor] S: Update requested, brightness=" + brightness)
        onTurnOffRequested: console.log("[Pattern And Monitor] S: Turn off requested")
    }

    Item {}
}
