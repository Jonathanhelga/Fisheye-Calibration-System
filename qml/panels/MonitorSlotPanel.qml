import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FisheyeCaliJojo

// One projector/camera slot in the Monitor Viewer window: preview, image
// path, and brightness controls for a single direction (TOP, N, W, S, E).
//
// Update pushes the current image + brightness to that screen; Turn off asks the
// rig to close the pattern on it, so the panel returns to its desktop. Turn off
// deliberately does NOT zero the brightness: brightness is a monitor hardware
// setting, closing a pattern is a window operation, and conflating them left the
// screen black afterwards with no way to tell which of the two had happened.
Rectangle {
    id: root

    property string label: ""
    property string direction: ""
    property string imagePath: ""
    property alias brightness: brightnessField.value
    property bool on: false

    // What the RIG has, as opposed to what is typed in the boxes. The two drift
    // apart the moment someone edits a path or a brightness and does not press
    // Update, and without this the panel looks identical either way -- so an
    // un-pushed edit reads as a pushed one and the operator shoots against the
    // wrong screen contents.
    property string appliedImagePath: ""
    property real appliedBrightness: 5

    // Only meaningful while something is actually on the screen. An OFF slot has
    // nothing applied to differ from, so it advertised unsaved changes forever.
    readonly property bool pendingChanges: root.on
        && (root.imagePath !== root.appliedImagePath
            || root.brightness !== root.appliedBrightness)

    readonly property string livePreview:
        root.direction ? (PatternController.previewUrls[root.direction] || "") : ""

    onLivePreviewChanged: if (root.livePreview) root.on = true

    signal browseRequested()
    signal updateRequested(real brightness)
    signal turnOffRequested()

    function turnOff() {
        root.turnOffRequested()
    }

    // The ON badge follows the rig, not the button press. An Update that the
    // monitor node refused used to leave the slot reading ON with nothing on the
    // glass, which is the failure mode this whole panel exists to make visible.
    //
    // The brightness is left alone on close -- see the note at the top.
    Connections {
        target: MonitorController

        function onImageShown(direction) {
            if (direction !== root.direction && direction !== "all") return
            root.on = true
            root.appliedImagePath = root.imagePath
        }

        function onPatternClosed(direction) {
            if (direction !== root.direction && direction !== "all") return
            root.on = false
            // Nothing is on the glass any more, so nothing is applied and there
            // is nothing left to push -- clearing both keeps the panel from
            // describing a screen it just emptied.
            root.imagePath = ""
            root.appliedImagePath = ""
        }

        function onBrightnessRead(direction, brightness) {
            if (direction === root.direction) root.appliedBrightness = brightness
        }
    }

    // A pattern that reached this screen from the spec editor also changes what
    // is on the glass, so the slot follows it -- otherwise pushing a pattern and
    // then looking at the slot shows a stale path with no hint that it is stale.
    Connections {
        target: PatternController

        function onPatternShown(direction, width, height, imagePath) {
            if (direction !== root.direction) return
            root.on = true
            if (imagePath !== "") {
                root.imagePath = imagePath
                root.appliedImagePath = imagePath
            }
        }
    }

    readonly property real minimumWidth:  content.Layout.minimumWidth  + 2 * Theme.panelMargin
    readonly property real minimumHeight: content.Layout.minimumHeight + 2 * Theme.panelMargin

    implicitWidth:  content.implicitWidth  + 2 * Theme.panelMargin
    implicitHeight: content.implicitHeight + 2 * Theme.panelMargin

    color: Theme.panelBackground
    border.color: Theme.panelBorder
    radius: Theme.radius

    ColumnLayout {
        id: content

        anchors.fill: parent
        anchors.margins: Theme.panelMargin
        spacing: Theme.rowSpacing

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.rowSpacing

            Label {
                Layout.fillWidth: true
                text: root.label
                font.bold: true
                font.pixelSize: Theme.fontTitle
                color: Theme.accent
                elide: Text.ElideRight
            }

            StatusDot {
                Layout.alignment: Qt.AlignVCenter
                status: !root.on ? ProbeStatus.Failed
                      : (root.livePreview || root.imagePath) ? ProbeStatus.Ok
                                                             : ProbeStatus.Unknown
            }
        }

        ImagePreview {
            id: preview

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth:  Theme.minMonitorPreviewWidth
            Layout.minimumHeight: Theme.minMonitorPreviewHeight

            // PatternIo does the path->URL conversion. "file://" + path is wrong
            // on Windows -- the drive letter becomes the host and the image just
            // silently does not load.
            source: root.livePreview ? root.livePreview
                                     : PatternIo.toFileUrl(root.imagePath)
            emptyText: root.on ? qsTr("No image") : qsTr("Off")
            pickEnabled: false
            opacity: root.on ? 1 : 0.35

            badgeText: root.on ? qsTr("ON") : qsTr("OFF")
            badgeActive: root.on
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.labelSpacing

            Label {
                text: qsTr("Img path")
                color: Theme.textCaption
                font.pixelSize: Theme.captionFontSize
            }

            TextField {
                id: pathField

                Layout.fillWidth: true
                text: root.imagePath
                color: Theme.textPrimary
                font.pixelSize: Theme.captionFontSize
                selectByMouse: true

                onEditingFinished: root.imagePath = text

                background: Rectangle {
                    implicitHeight: Theme.controlHeight
                    color: Theme.fieldBackground
                    border.color: pathField.activeFocus ? Theme.accent : Theme.panelBorder
                    radius: Theme.radius
                }
            }

            ActionButton {
                text: qsTr("Browse...")
                onClicked: root.browseRequested()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.labelSpacing

            ValueSpinBox {
                id: brightnessField

                // The spin box carries its own caption, so there is no separate
                // Label here any more.
                label: qsTr("Brightness")
                from: 0
                to: 100
                value: 5
            }

            Label {
                text: "%"
                color: Theme.textCaption
                font.pixelSize: Theme.captionFontSize
            }

            Item { Layout.fillWidth: true }

            ActionButton {
                // Marked while the boxes differ from what the rig has, so an
                // edit that was never pushed is visible rather than silent.
                text: root.pendingChanges ? qsTr("Update *") : qsTr("Update")
                tone: root.pendingChanges ? "danger" : "accent"
                enabled: root.imagePath !== "" && !MonitorController.busy
                onClicked: root.updateRequested(root.brightness)

                ToolTip.visible: hovered
                ToolTip.delay: Theme.animSlow
                ToolTip.text: root.pendingChanges
                    ? qsTr("This slot's image or brightness differs from what is on the rig.")
                    : qsTr("Push this image and brightness to the screen.")
            }

            ActionButton {
                text: qsTr("Turn off")
                tone: "danger"
                onClicked: root.turnOff()
            }
        }
    }
}
