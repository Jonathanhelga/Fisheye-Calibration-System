import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import FisheyeCaliJojo

Rectangle {
    id: root

    property string singlePath: ""
    property string positivePath: ""
    property string negativePath: ""

    property string positiveTime: ""
    property string negativeTime: ""

    property bool busy: false
    property string pendingMode: ""

    property int centerX: -1
    property int centerY: -1
    property int roiRadius: 0
    property bool centerLocked: false

    readonly property bool hasPositive: positivePath !== ""
    readonly property bool hasNegative: negativePath !== ""
    readonly property bool hasPair: hasPositive && hasNegative

    readonly property string patternMode: viewSelector.currentIndex === 1 ? "Positive"
                                        : viewSelector.currentIndex === 2 ? "Negative"
                                                                          : ""

    readonly property string imagePath: viewSelector.currentIndex === 1 ? positivePath
                                      : viewSelector.currentIndex === 2 ? negativePath
                                                                        : singlePath

    readonly property url imageUrl: root.imagePath === "" ? ""
                                 : root.imagePath.startsWith("qrc:") ? root.imagePath
                                                                     : "file://" + root.imagePath

    readonly property string fileName:
        imagePath.substring(imagePath.lastIndexOf("/") + 1)

    readonly property string statusText: busy
        ? (pendingMode ? qsTr("Capturing %1 shot...").arg(pendingMode)
                       : qsTr("Capturing..."))
        : (imagePath ? qsTr("Showing %1").arg(fileName) : qsTr("Idle"))

    readonly property string pairText: hasPair
        ? qsTr("Pos %1   Neg %2").arg(positiveTime).arg(negativeTime)
        : hasPositive ? qsTr("Pos %1   Neg missing").arg(positiveTime)
        : hasNegative ? qsTr("Pos missing   Neg %1").arg(negativeTime)
                      : ""

    signal captureRequested(string mode)
    signal pairRequested()
    signal browseRequested()
    signal directionDiffRequested()
    signal centerPicked(string mode, int x, int y)

    function requestCapture(mode) {
        pendingMode = mode
        captureRequested(mode)
    }

    function showView(mode) {
        viewSelector.currentIndex = mode === "Positive" ? 1
                                  : mode === "Negative" ? 2
                                                        : 0
    }

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
                text: qsTr("Camera Panel")
                font.bold: true
                color: Theme.accent
            }

            StatusDot {
                Layout.alignment: Qt.AlignVCenter
                status: root.busy ? ServerProbe.Checking
                      : root.imagePath ? ServerProbe.Ok
                                       : ServerProbe.Unknown
            }

            Label {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredWidth: 0
                text: root.statusText
                color: root.busy ? Theme.textPrimary : Theme.textCaption
                font.pixelSize: Theme.captionFontSize
                elide: Text.ElideRight
            }

            ActionButton {
                text: qsTr("Magnify")
                enabled: root.imagePath !== ""
                checked: magnifier.visible
                onClicked: magnifier.visible = !magnifier.visible

                ToolTip.visible: hovered
                ToolTip.delay: Theme.animSlow
                ToolTip.text: qsTr("Open a larger view to fine-tune the center")
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.rowSpacing

            Label {
                text: qsTr("Image")
                color: Theme.textCaption
                font.pixelSize: Theme.captionFontSize
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.minimumWidth: Theme.fieldMinWidth
                Layout.preferredWidth: 0
                implicitHeight: Theme.controlHeight

                color: Theme.fieldBackground
                border.color: Theme.panelBorder
                radius: Theme.radius

                Text {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.fieldPadding
                    anchors.rightMargin: Theme.fieldPadding

                    text: root.imagePath ? root.imagePath : qsTr("No image loaded")
                    color: root.imagePath ? Theme.textPrimary : Theme.textCaption
                    font.bold: root.imagePath !== ""
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideLeft
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: root.imagePath !== ""
                    acceptedButtons: Qt.NoButton

                    ToolTip.visible: containsMouse
                    ToolTip.text: root.imagePath
                    ToolTip.delay: Theme.animSlow
                }
            }

            ActionButton {
                text: qsTr("Open Img")
                enabled: !root.busy
                onClicked: root.browseRequested()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.rowSpacing

            SegmentedControl {
                id: viewSelector
                model: [qsTr("Single"), qsTr("Pos"), qsTr("Neg")]
                currentIndex: 0
            }

            Label {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredWidth: 0
                text: root.pairText
                color: root.hasPair ? Theme.textCaption : Theme.statusPartial
                font.pixelSize: Theme.captionFontSize
                elide: Text.ElideRight
            }

            AxisReadout {
                label: qsTr("Source")
                value: preview.loaded
                    ? preview.sourceWidth + "x" + preview.sourceHeight
                    : "?"
            }

            AxisReadout {
                label: qsTr("Zoom")
                value: preview.loaded ? Math.round(preview.zoom * 100) + "%" : "?"
            }
        }

        ImagePreview {
            id: preview

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: Theme.unit * 12

            source: root.imageUrl
            emptyText: root.patternMode
                ? qsTr("No %1 shot yet").arg(root.patternMode.toLowerCase())
                : qsTr("No image. Press Capture.")

            pickEnabled: root.patternMode !== "" && !root.centerLocked
            hint: pickEnabled ? qsTr("Click to set the %1 center").arg(root.patternMode.toLowerCase())
                : root.centerLocked ? qsTr("Center is locked. Unlock it in the Centering panel.")
                                    : qsTr("Preview only. Switch to Pos or Neg to set the center.")

            centerX: root.centerX
            centerY: root.centerY
            roiRadius: root.roiRadius

            onPicked: (x, y) => root.centerPicked(root.patternMode, x, y)
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.rowSpacing

            ActionButton {
                Layout.fillWidth: true
                tone: "accent"
                text: qsTr("Pair Shot")
                enabled: !root.busy
                onClicked: root.pairRequested()

                ToolTip.visible: hovered
                ToolTip.delay: Theme.animSlow
                ToolTip.text: qsTr("Positive then negative, back to back. Keep the rig still.")
            }

            ActionButton {
                Layout.preferredWidth: Math.round(Theme.charUnit * 11)
                tone: "accent"
                text: qsTr("Capture")
                enabled: !root.busy
                onClicked: root.requestCapture("")
            }

            ActionButton {
                Layout.preferredWidth: Math.round(Theme.charUnit * 8)
                text: qsTr("Pos")
                enabled: !root.busy
                onClicked: root.requestCapture("Positive")

                ToolTip.visible: hovered
                ToolTip.delay: Theme.animSlow
                ToolTip.text: qsTr("Retake the positive shot only")
            }

            ActionButton {
                Layout.preferredWidth: Math.round(Theme.charUnit * 8)
                text: qsTr("Neg")
                enabled: !root.busy
                onClicked: root.requestCapture("Negative")

                ToolTip.visible: hovered
                ToolTip.delay: Theme.animSlow
                ToolTip.text: qsTr("Retake the negative shot only")
            }

            ActionButton {
                Layout.preferredWidth: Math.round(Theme.charUnit * 14)
                text: qsTr("Direction Diff")
                enabled: !root.busy && root.hasPair
                onClicked: root.directionDiffRequested()

                ToolTip.visible: hovered
                ToolTip.delay: Theme.animSlow
                ToolTip.text: enabled
                    ? qsTr("Compare N-S, W-E, NW-SE, SW-NE on the current pair")
                    : qsTr("Take a pair first")
            }
        }
    }

    Window {
        id: magnifier

        title: root.fileName !== "" ? root.fileName : qsTr("Camera Preview")
        color: Theme.panelBackground

        width: Theme.unit * 56
        height: Theme.unit * 44
        minimumWidth: Theme.unit * 32
        minimumHeight: Theme.unit * 24

        ImagePreview {
            anchors.fill: parent
            anchors.margins: Theme.panelMargin

            source: preview.source
            emptyText: preview.emptyText

            pickEnabled: preview.pickEnabled
            hint: preview.hint

            centerX: root.centerX
            centerY: root.centerY
            roiRadius: root.roiRadius

            onPicked: (x, y) => root.centerPicked(root.patternMode, x, y)
        }
    }
}
