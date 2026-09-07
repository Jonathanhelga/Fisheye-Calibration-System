pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import FisheyeCaliJojo

// The five calibration screens laid out the way they sit on the rig.
//
// Every slot's Update / Turn off / Browse goes to MonitorController, which is a
// device controller -- it pushes the PNG bytes and the brightness the operator
// chose. It is NOT PatternController: that one renders the spec the operator is
// authoring and reaches the glass through show_pattern_spec. Both can put
// something on the same screen, which is why the slot shows whichever arrived
// last (livePreview wins) rather than pretending only one of them exists.
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

    readonly property var sideSlots: [northSlot, westSlot, southSlot, eastSlot]

    function turnOffFourSides() {
        for (let i = 0; i < sideSlots.length; ++i)
            sideSlots[i].turnOff()
    }

    function applyImageToFourSides(path) {
        for (let i = 0; i < sideSlots.length; ++i) {
            sideSlots[i].imagePath = path
            root.pushSlot(sideSlots[i])
        }
    }

    // Brightness first, then the picture. The other order shows the image at
    // whatever brightness the panel was left on, which on a screen the operator
    // just turned off is a black frame that looks like a failed push.
    function pushSlot(slot) {
        if (slot.imagePath === "") {
            slot.on = false
            return
        }
        MonitorController.setBrightness(slot.direction, slot.brightness)
        MonitorController.showImagePath(slot.direction, slot.imagePath)
    }

    // Turn off closes the pattern and stops there: the panel goes back to its
    // desktop. It deliberately does not also zero the brightness -- that is a
    // monitor hardware setting, and driving it to 0 here leaves a black screen
    // that looks identical to a failed close.
    function turnOffSlot(slot) {
        MonitorController.closePattern(slot.direction)
    }

    function browseFor(slot) {
        browseDialog.slot = slot
        browseDialog.open()
    }

    FileDialog {
        id: browseDialog

        property var slot: null

        title: qsTr("Image for %1").arg(browseDialog.slot ? browseDialog.slot.label : "")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("Images (*.png *.jpg *.jpeg *.bmp)"), qsTr("All files (*)")]

        Component.onCompleted: browseDialog.currentFolder = PatternIo.defaultImageDirectory

        // Stored as a local path: that is what the operator reads in the field
        // and what MonitorController opens. The preview converts it back.
        onAccepted: {
            if (!browseDialog.slot)
                return
            browseDialog.slot.imagePath = PatternIo.toLocalPath(browseDialog.selectedFile)
        }
    }

    // The rig is the authority on brightness, not the field. When a slot asks and
    // the answer differs from what is typed, the answer wins.
    Connections {
        target: MonitorController

        function onBrightnessRead(direction, brightness) {
            const slots = [topSlot, northSlot, westSlot, southSlot, eastSlot]
            for (let i = 0; i < slots.length; ++i)
                if (slots[i].direction === direction)
                    slots[i].brightness = brightness
        }
    }

    component Slot: MonitorSlotPanel {
        id: slot

        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumWidth: root.slotMinimumWidth
        Layout.minimumHeight: root.slotMinimumHeight

        onBrowseRequested: root.browseFor(slot)
        onUpdateRequested: root.pushSlot(slot)
        onTurnOffRequested: root.turnOffSlot(slot)
    }

    Item {}

    Slot {
        id: northSlot
        label: qsTr("N")
        direction: "n"
    }

    Item {}

    Slot {
        id: westSlot
        label: qsTr("W")
        direction: "w"
    }

    Slot {
        id: topSlot
        label: qsTr("TOP")
        direction: "top"
    }

    Slot {
        id: eastSlot
        label: qsTr("E")
        direction: "e"
    }

    Item {}

    Slot {
        id: southSlot
        label: qsTr("S")
        direction: "s"
    }

    Item {}
}
