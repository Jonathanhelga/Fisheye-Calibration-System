import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FisheyeCaliJojo

Rectangle {
    id: root

    property string framePath: ""

    // Anything already carrying a scheme is passed through untouched. The live
    // frames arrive as image://moilcamera/live/<revision> from the image
    // provider, and prefixing those with file:// produced a silent blank panel.
    readonly property url frameUrl: PatternIo.toFileUrl(root.framePath)

    // Owned by the controller: the stream is a ROS subscription, and a local bool
    // that says "streaming" while nothing is subscribed is exactly the lie this
    // panel used to tell.
    property bool streaming: false
    property real fps: 0

    property int linkStatus: ProbeStatus.Unknown

    property bool showGrid: false
    property bool showRoi: true

    property int centerX: -1
    property int centerY: -1
    property int roiRadius: 0

    property int edgeRadius: 0
    property color edgeColor: "transparent"
    property int edgeThickness: 2
    property bool edgeVisible: false

    // Told by the controller whether frames are actually arriving. preview.loaded
    // only says the last URL decoded, which stays true after the stream stops.
    property bool receiving: streaming && preview.loaded

    // What the operator should be told the frames come from. It is a ROS topic,
    // not the HTTP endpoint this used to name -- HTTP is a reachability probe in
    // this app and carries no image data at all.
    readonly property string sourceUrl: "/camera/image_raw/compressed"

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
                // Falls back to the ROS camera link, not the HTTP probe: the
                // frames come over a ROS topic now, so an HTTP dot here would be
                // reporting the health of something this panel does not use.
                status: root.receiving ? ProbeStatus.Ok
                      : root.streaming ? ProbeStatus.Checking
                                       : root.linkStatus
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
                    ToolTip.text: qsTr("Frames arrive on %1").arg(root.sourceUrl)
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

            source: root.streaming ? root.frameUrl : ""

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

            // The ROI button governs both markers: they are the same overlay to
            // anyone aiming the rig, and one toggle that leaves half of it on
            // would read as a bug.
            edgeVisible: root.showRoi && root.edgeVisible
            edgeRadius: root.edgeRadius
            edgeColor: root.edgeColor
            edgeThickness: root.edgeThickness
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.rowSpacing

            ActionButton {
                Layout.fillWidth: true
                tone: root.streaming ? "danger" : "accent"
                text: root.streaming ? qsTr("Stop") : qsTr("Go Live")
                // The button asks; the controller decides. `streaming` follows
                // the subscription, so a Go Live that cannot connect leaves the
                // button reading Go Live instead of pretending to stream.
                onClicked: root.streaming ? root.stopRequested() : root.startRequested()
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
