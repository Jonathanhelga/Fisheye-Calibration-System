import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FisheyeCaliJojo

// One projector/camera slot in the Monitor Viewer window: preview, pattern
// file, and brightness controls for a single direction (TOP, N, W, S, E).
// Browse loads a pattern JSON into the panel that owns that pattern type;
// Update pushes that panel's spec plus brightness to this screen; Turn off asks
// the rig to close the pattern on it, so the panel returns to its desktop.
Rectangle {
    id: root

    property string label: ""
    property string direction: ""
    property url configUrl
    property string patternType: ""
    property alias brightness: brightnessField.value
    property bool brightnessSupported: true
    property bool on: false

    property url appliedConfigUrl
    property real appliedBrightness: 5

    // toLocalPath, not localPath -- this branch's PatternIo spells it that way.
    readonly property string configPath: PatternIo.toLocalPath(root.configUrl)

    readonly property bool pendingChanges: PatternController.status === ProbeStatus.Ok
                                        && (root.configUrl !== root.appliedConfigUrl
                                         || (root.on
                                             && root.brightnessSupported
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
            root.configUrl = ""
            root.appliedConfigUrl = ""
            root.patternType = ""
        }

        function onPatternShown(direction, width, height, imagePath) {
            if (direction !== root.direction) return
            root.appliedConfigUrl = root.configUrl
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
                      : root.livePreview ? ProbeStatus.Ok
                                         : ProbeStatus.Unknown
            }
        }

        ImagePreview {
            id: preview

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth:  Theme.minMonitorPreviewWidth
            Layout.minimumHeight: Theme.minMonitorPreviewHeight

            source: root.livePreview
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
                text: qsTr("Pattern")
                color: Theme.textCaption
                font.pixelSize: Theme.captionFontSize
            }

            TextField {
                id: pathField

                Layout.fillWidth: true
                readOnly: true
                text: root.configPath
                placeholderText: qsTr("No pattern file loaded")
                color: Theme.textPrimary
                placeholderTextColor: Theme.textDisabled
                font.pixelSize: Theme.captionFontSize
                selectByMouse: true

                ToolTip.visible: hovered && root.configPath.length > 0
                ToolTip.delay: Theme.animSlow
                ToolTip.text: root.configPath

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

                ToolTip.visible: hovered
                ToolTip.delay: Theme.animSlow
                ToolTip.text: qsTr("Load a pattern JSON. The rig renders it at this panel's own resolution, and keeps the layers so a Negative shot can be produced from them.")
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
