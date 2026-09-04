import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FisheyeCaliJojo

// One projector/camera slot in the Monitor Viewer window: preview, image
// path, and brightness controls for a single direction (TOP, N, W, S, E).
// Port of monitor_viewer.ui / ControllerMonitor: Update pushes the current
// image + brightness to that screen; Turn off pushes a black frame at 0%
// brightness through the same path, so it snaps brightness to 0 too.
Rectangle {
    id: root

    property string label: ""
    property string direction: ""
    property string imagePath: ""
    property real brightness: 5
    property bool on: false

    readonly property string livePreview:
        root.direction ? (PatternController.previewUrls[root.direction] || "") : ""

    onLivePreviewChanged: if (root.livePreview) root.on = true

    signal browseRequested()
    signal updateRequested(real brightness)
    signal turnOffRequested()

    function turnOff() {
        root.brightness = 0
        root.on = false
        root.turnOffRequested()
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

            source: root.livePreview ? root.livePreview
                  : root.imagePath ? "file://" + root.imagePath
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

            Label {
                text: qsTr("Brightness")
                color: Theme.textCaption
                font.pixelSize: Theme.captionFontSize
            }

            ValueField {
                id: brightnessField

                Layout.preferredWidth: Theme.readoutWidth
                horizontalAlignment: Text.AlignRight
                editable: true
                validator: IntValidator { bottom: 0; top: 100 }
                value: Math.round(root.brightness)

                onEdited: (value) => root.brightness = parseInt(value)
            }

            Label {
                text: "%"
                color: Theme.textCaption
                font.pixelSize: Theme.captionFontSize
            }

            Item { Layout.fillWidth: true }

            ActionButton {
                text: qsTr("Update")
                tone: "accent"
                onClicked: {
                    root.on = root.imagePath !== ""
                    root.updateRequested(root.brightness)
                }
            }

            ActionButton {
                text: qsTr("Turn off")
                tone: "danger"
                onClicked: root.turnOff()
            }
        }
    }
}
