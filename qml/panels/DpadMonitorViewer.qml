import QtQuick
import QtQuick.Controls
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

    readonly property real slotMinimumWidth:  Math.max(Theme.minMonitorSlotWidth,  topSlot.minimumWidth)
    readonly property real slotMinimumHeight: Math.max(Theme.minMonitorSlotHeight, topSlot.minimumHeight)

    function turnOffFourSides() {
        northSlot.turnOff()
        westSlot.turnOff()
        southSlot.turnOff()
        eastSlot.turnOff()
    }

    function pushSlot(slot, brightness) {
        if (slot.imagePath && slot.imagePath !== slot.appliedImagePath)
            PatternController.showImageOnMonitor(slot.direction, slot.imagePath)

        if (brightness !== slot.appliedBrightness)
            PatternController.setMonitorBrightness(slot.direction, brightness)
    }

    function browseFor(slot) {
        browseDialog.target = slot
        browseDialog.open()
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

        onBrowseRequested: root.browseFor(northSlot)
        onUpdateRequested: (brightness) => root.pushSlot(northSlot, brightness)
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

        title: browseDialog.target ? qsTr("Choose pattern image for %1").arg(browseDialog.target.label)
                                   : qsTr("Choose pattern image")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("Images (*.png *.jpg *.jpeg *.bmp)"), qsTr("All files (*)")]

        Component.onCompleted: browseDialog.currentFolder = PatternIo.defaultImageDirectory

        onAccepted: {
            const path = PatternIo.localPath(browseDialog.selectedFile)
            if (!browseDialog.target || !path) return

            browseDialog.target.imagePath = path
            root.pushSlot(browseDialog.target, browseDialog.target.brightness)
        }
    }
}
