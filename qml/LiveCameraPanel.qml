import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FisheyeCaliJojo

Rectangle {
    id: root

    property string framePath: ""

    property bool streaming: false
    property real fps: 0

    property bool showGrid: false
    property bool showRoi: true

    property int centerX: -1
    property int centerY: -1
    property int roiRadius: 0

    readonly property bool receiving: streaming && preview.loaded

    readonly property string sourceUrl: ServerProbe.host
        ? "http://" + ServerProbe.host + ":" + ServerProbe.cameraPort + "/single_image"
        : ""

    readonly property string frameSize: preview.sourceWidth + "x" + preview.sourceHeight

    readonly property string statusText: !streaming ? qsTr("Stopped")
        : !preview.loaded ? qsTr("Waiting for frames...")
        : fps > 0 ? qsTr("Live %1 at %2 fps").arg(frameSize).arg(fps.toFixed(1))
                  : qsTr("Live %1").arg(frameSize)

    signal startRequested()
    signal stopRequested()
    signal snapshotRequested()

    implicitWidth: content.implicitWidth + 2 * Theme.panelMargin
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
                text: qsTr("Live Camera")
                font.bold: true
                font.pixelSize: Theme.fontTitle
                color: Theme.accent
            }

            StatusDot {
                Layout.alignment: Qt.AlignVCenter
                status: root.receiving ? ServerProbe.Ok
                      : root.streaming ? ServerProbe.Checking
                                       : ServerProbe.cameraStatus
            }

            Label {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredWidth: 0
                text: root.statusText
                color: root.receiving ? Theme.textPrimary : Theme.textCaption
                font.pixelSize: Theme.captionFontSize
                elide: Text.ElideRight

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.NoButton

                    ToolTip.visible: containsMouse
                    ToolTip.delay: Theme.animSlow
                    ToolTip.text: root.sourceUrl
                        ? root.sourceUrl
                        : qsTr("No camera URL yet. Press Update in the HTTP Server panel.")
                }
            }

            ActionButton {
                Layout.preferredWidth: Math.round(Theme.charUnit * 8)
                text: qsTr("Grid")
                checkable: true
                checked: root.showGrid
                onToggled: root.showGrid = checked

                ToolTip.visible: hovered
                ToolTip.delay: Theme.animSlow
                ToolTip.text: qsTr("Thirds grid plus the image center cross")
            }

            ActionButton {
                Layout.preferredWidth: Math.round(Theme.charUnit * 8)
                text: qsTr("ROI")
                checkable: true
                checked: root.showRoi
                onToggled: root.showRoi = checked

                ToolTip.visible: hovered
                ToolTip.delay: Theme.animSlow
                ToolTip.text: qsTr("Show the pattern center held by the Centering panel")
            }
        }

        ImagePreview {
            id: preview

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: Theme.unit * 8
            Layout.preferredHeight: Theme.unit * 12

            source: root.streaming && root.framePath ? "file://" + root.framePath : ""

            emptyText: root.streaming ? qsTr("Waiting for frames...")
                                      : qsTr("Stream is off. Press Go Live.")
            hint: ""
            pickEnabled: false

            showGrid: root.showGrid
            badgeText: root.streaming ? qsTr("LIVE") : ""
            badgeActive: root.receiving

            centerX: root.showRoi ? root.centerX : -1
            centerY: root.showRoi ? root.centerY : -1
            roiRadius: root.showRoi ? root.roiRadius : 0
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.rowSpacing

            ActionButton {
                Layout.fillWidth: true
                tone: root.streaming ? "danger" : "accent"
                text: root.streaming ? qsTr("Stop") : qsTr("Go Live")
                onClicked: {
                    root.streaming = !root.streaming
                    if (root.streaming)
                        root.startRequested()
                    else
                        root.stopRequested()
                }
            }

            ActionButton {
                Layout.preferredWidth: Math.round(Theme.charUnit * 11)
                text: qsTr("Snapshot")
                enabled: root.receiving
                onClicked: root.snapshotRequested()

                ToolTip.visible: hovered
                ToolTip.delay: Theme.animSlow
                ToolTip.text: enabled
                    ? qsTr("Keep the newest frame as the Camera panel image")
                    : qsTr("Go live first")
            }
        }
    }
}
