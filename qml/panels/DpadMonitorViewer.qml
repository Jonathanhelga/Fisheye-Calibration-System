import QtQuick
import QtQuick.Dialogs
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

    signal patternFileChosen(var slot, url fileUrl)
    signal patternPushRequested(var slot)

    readonly property real slotMinimumWidth:  Math.max(Theme.minMonitorSlotWidth,  topSlot.minimumWidth)
    readonly property real slotMinimumHeight: Math.max(Theme.minMonitorSlotHeight, topSlot.minimumHeight)

    function turnOffFourSides() {
        northSlot.turnOff()
        westSlot.turnOff()
        southSlot.turnOff()
        eastSlot.turnOff()
    }

    function pushSlot(slot, brightness) {
        if (slot.patternType && slot.configUrl !== slot.appliedConfigUrl)
            root.patternPushRequested(slot)

        if (slot.brightnessSupported && brightness !== slot.appliedBrightness)
            PatternController.setMonitorBrightness(slot.direction, brightness)
    }

    function browseFor(slot) {
        browseDialog.target = slot
        browseDialog.open()
    }

    function applyPatternToFourSides(fileUrl, patternType) {
        for (const slot of [northSlot, westSlot, southSlot, eastSlot]) {
            slot.configUrl = fileUrl
            slot.patternType = patternType
            root.pushSlot(slot, slot.brightness)
        }
    }

    Item {}

    MonitorSlotPanel {
        id: northSlot
        label: qsTr("N")
        direction: "n"
        brightnessSupported: false
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumWidth: root.slotMinimumWidth
        Layout.minimumHeight: root.slotMinimumHeight

        onBrowseRequested: root.browseFor(northSlot)
        onUpdateRequested: (brightness) => root.pushSlot(northSlot, brightness)
        onTurnOffRequested: PatternController.closeMonitor(northSlot.direction)
    }

    Item {}

    MonitorSlotPanel {
        id: westSlot
        label: qsTr("W")
        direction: "w"
        brightnessSupported: false
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumWidth: root.slotMinimumWidth
        Layout.minimumHeight: root.slotMinimumHeight

        onBrowseRequested: root.browseFor(westSlot)
        onUpdateRequested: (brightness) => root.pushSlot(westSlot, brightness)
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

        onBrowseRequested: root.browseFor(topSlot)
        onUpdateRequested: (brightness) => root.pushSlot(topSlot, brightness)
        onTurnOffRequested: PatternController.closeMonitor(topSlot.direction)
    }

    MonitorSlotPanel {
        id: eastSlot
        label: qsTr("E")
        direction: "e"
        brightnessSupported: false
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumWidth: root.slotMinimumWidth
        Layout.minimumHeight: root.slotMinimumHeight

        onBrowseRequested: root.browseFor(eastSlot)
        onUpdateRequested: (brightness) => root.pushSlot(eastSlot, brightness)
        onTurnOffRequested: PatternController.closeMonitor(eastSlot.direction)
    }

    Item {}

    MonitorSlotPanel {
        id: southSlot
        label: qsTr("S")
        direction: "s"
        brightnessSupported: false
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumWidth: root.slotMinimumWidth
        Layout.minimumHeight: root.slotMinimumHeight

        onBrowseRequested: root.browseFor(southSlot)
        onUpdateRequested: (brightness) => root.pushSlot(southSlot, brightness)
        onTurnOffRequested: PatternController.closeMonitor(southSlot.direction)
    }

    Item {}

    FileDialog {
        id: browseDialog

        property var target: null

        title: browseDialog.target ? qsTr("Choose pattern JSON for %1").arg(browseDialog.target.label)
                                   : qsTr("Choose pattern JSON")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("Pattern JSON (*.json)"), qsTr("All files (*)")]

        Component.onCompleted: browseDialog.currentFolder = PatternIo.defaultDirectory

        onAccepted: {
            if (!browseDialog.target) return
            root.patternFileChosen(browseDialog.target, browseDialog.selectedFile)
        }
    }
}
