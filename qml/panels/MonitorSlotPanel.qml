import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FisheyeCaliJojo

// One projector/camera slot in the Monitor Viewer window: preview, image
// path, and brightness controls for a single direction (TOP, N, W, S, E).
// Update pushes the current image + brightness to that screen; Turn off asks
// the rig to close the pattern on it, so the panel returns to its desktop.
Rectangle {
    id: root

    property string label: ""
    property string direction: ""
    property string imagePath: ""
    property alias brightness: brightnessField.value
    property bool brightnessSupported: true
    property bool on: false

    property string appliedImagePath: ""
    property real appliedBrightness: 5

    readonly property bool pendingChanges: PatternController.status === ProbeStatus.Ok
                                        && root.on
                                        && (root.imagePath !== root.appliedImagePath
                                         || (root.brightnessSupported
                                             && root.brightness !== root.appliedBrightness))

    readonly property string livePreview:
        root.direction ? (PatternController.previewUrls[root.direction] || "") : ""

    onLivePreviewChanged: if (root.livePreview) root.on = true

    signal browseRequested()
    signal updateRequested(real brightness)
    signal turnOffRequested()

    function turnOff() {
        root.turnOffRequested()
    }

    Connections {
        target: PatternController

        function onMonitorClosed(direction) {
            if (direction !== root.direction && direction !== "all") return
            root.on = false
            root.imagePath = ""
            root.appliedImagePath = ""
        }

        function onPatternShown(direction, width, height, imagePath) {
            if (direction !== root.direction) return
            root.imagePath = imagePath
            root.appliedImagePath = imagePath
            root.on = true
        }

        function onBrightnessApplied(direction, brightness) {
            if (direction === root.direction) root.appliedBrightness = brightness
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
                text: root.label
                font.bold: true
                font.pixelSize: Theme.fontTitle
                color: Theme.accent
            }

            Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignRight
                text: root.pendingChanges ? qsTr("Press Update") : ""
                color: Theme.statusPartial
                font.pixelSize: Theme.captionFontSize
                elide: Text.ElideRight
            }

            StatusDot {
                Layout.alignment: Qt.AlignVCenter
                status: !root.on ? ProbeStatus.Failed
                      : (root.livePreview || root.appliedImagePath) ? ProbeStatus.Ok
                                                                    : ProbeStatus.Unknown
            }
        }

        ImagePreview {
            id: preview

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth:  Theme.minMonitorPreviewWidth
            Layout.minimumHeight: Theme.minMonitorPreviewHeight

            source: root.livePreview ? root.livePreview
                  : root.appliedImagePath ? "file://" + root.appliedImagePath
                                          : ""
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

                label: qsTr("Brightness")
                from: 0
                to: 100
                value: 5
                enabled: root.brightnessSupported

                ToolTip.visible: hovered && !root.brightnessSupported
                ToolTip.delay: Theme.animSlow
                ToolTip.text: qsTr("Only the TOP panel accepts DDC/CI brightness on this rig.")
            }

            Label {
                text: "%"
                color: root.brightnessSupported ? Theme.textCaption : Theme.textDisabled
                font.pixelSize: Theme.captionFontSize
            }

            Item { Layout.fillWidth: true }

            ActionButton {
                text: qsTr("Update")
                tone: "accent"
                onClicked: root.updateRequested(root.brightness)
            }

            ActionButton {
                text: qsTr("Turn off")
                tone: "danger"
                onClicked: root.turnOff()
            }
        }
    }
}
