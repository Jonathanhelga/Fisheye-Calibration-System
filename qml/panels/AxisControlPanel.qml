pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FisheyeCaliJojo

Rectangle {
    id: root

    property real xStep: 1.0
    property real yStep: 1.0
    property real zStep: 1.0
    property real yawStep: 1.0
    property real pitchStep: 1.0

    readonly property bool stepMode: travelMode.currentIndex === 1
    readonly property bool moving: AxisController.busy
    readonly property bool ready: AxisController.connected
    readonly property bool fresh: AxisController.dataFresh

    readonly property int speed: [AxisController.High,
                                  AxisController.Mid,
                                  AxisController.Low][speedSelector.currentIndex]

    property string notice: ""

    readonly property string motionText: notice.length > 0 ? notice
        : AxisController.activityText.length > 0 ? AxisController.activityText
        : AxisController.connected ? qsTr("Idle")
        : AxisController.lastError.length > 0 ? AxisController.lastError
                                              : qsTr("Not connected")

    readonly property bool alerting: notice.length > 0

    function report(axis, reason) {
        const label = axis && axis.length > 0 ? axis.toUpperCase() + ": " : ""
        root.notice = label + reason
        noticeTimer.restart()
    }

    Timer {
        id: noticeTimer
        interval: Theme.noticeTimeout
        onTriggered: root.notice = ""
    }

    Connections {
        target: AxisController

        function onCommandRejected(axis, reason) { root.report(axis, reason) }
        function onCommandFailed(axis, reason) { root.report(axis, reason) }
        function onOperationTimedOut(axis, what, ms) {
            root.report(axis, qsTr("%1 timed out after %2 s").arg(what).arg(Math.round(ms / 1000)))
        }
        function onHomeGroupFinished(group, ok, message) {
            if (!ok) root.report("", message)
        }
        function onConnectionChanged() { root.notice = "" }
    }

    property string commandedAxis: ""
    property string commandedDirection: ""

    readonly property string litDirection: {
        if (root.commandedAxis.length === 0) return ""
        const state = AxisController.axis(root.commandedAxis)
        return state && state.busy ? root.commandedDirection : ""
    }

    function litFor(first, second) {
        return root.commandedAxis === first || root.commandedAxis === second
             ? root.litDirection : ""
    }

    function drive(axis, side, step, direction) {
        root.commandedAxis = axis
        root.commandedDirection = direction
        if (root.stepMode)
            AxisController.jog(axis, side, step, root.speed)
        else
            AxisController.driveToLimit(axis, side, root.speed)
    }

    function padEnabled(axis, side) {
        if (!root.ready || root.moving) return false
        const state = AxisController.axis(axis)
        if (!state) return false
        return side === AxisController.HighSide ? !state.highBlocked : !state.lowBlocked
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
                text: "Axis Control"
                font.bold: true
                color: Theme.accent
            }

            StatusDot {
                Layout.alignment: Qt.AlignVCenter
                status: AxisController.connectionState === AxisController.Connected
                            ? ServerProbe.Ok
                      : AxisController.connectionState === AxisController.Degraded
                            ? ServerProbe.Partial
                      : AxisController.connectionState === AxisController.Connecting
                            ? ServerProbe.Checking
                      : AxisController.connectionState === AxisController.Failed
                            ? ServerProbe.Failed
                            : ServerProbe.Unknown
            }

            Label {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredWidth: 0
                text: root.motionText
                color: root.alerting ? Theme.statusPartial
                     : root.moving ? Theme.textPrimary
                                   : Theme.textCaption
                font.pixelSize: Theme.captionFontSize
                elide: Text.ElideRight
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.rowSpacing

            AxisStateReadout { Layout.fillWidth: true; label: "X"; axis: AxisController.x }
            AxisStateReadout { Layout.fillWidth: true; label: "Y"; axis: AxisController.y }
            AxisStateReadout { Layout.fillWidth: true; label: "Z"; axis: AxisController.z }
            AxisStateReadout { Layout.fillWidth: true; label: "Yaw"; axis: AxisController.yaw }
            AxisStateReadout { Layout.fillWidth: true; label: "Pitch"; axis: AxisController.pitch }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.rowSpacing

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.rowSpacing

                Label {
                    text: qsTr("Travel")
                    color: Theme.textCaption
                    font.pixelSize: Theme.captionFontSize
                }

                SegmentedControl {
                    id: travelMode
                    model: [qsTr("Max"), qsTr("Step")]
                    currentIndex: 1
                }

                Label {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    Layout.preferredWidth: 0
                    text: root.stepMode ? qsTr("moves by the step")
                        : AxisController.limitMoveAvailable ? qsTr("drives to the mechanical limit")
                                                            : qsTr("drive-to-limit unavailable")
                    color: !root.stepMode && !AxisController.limitMoveAvailable
                           ? Theme.statusPartial : Theme.textCaption
                    font.pixelSize: Theme.captionFontSize
                    elide: Text.ElideRight
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.rowSpacing

                Label {
                    text: qsTr("Speed")
                    color: Theme.textCaption
                    font.pixelSize: Theme.captionFontSize
                }

                ComboBox {
                    id: speedSelector
                    Layout.preferredHeight: Theme.controlHeight
                    model: ["High", "Mid", "Low"]
                }
            }

            Button {
                id: stopButton

                Layout.preferredWidth: Math.round(Theme.charUnit * 12)
                Layout.preferredHeight: Theme.controlHeight
                Layout.alignment: Qt.AlignRight

                text: qsTr("STOP")
                onClicked: AxisController.stopAll()

                background: Rectangle {
                    radius: Theme.radius
                    color: stopButton.down ? Theme.dangerHover
                         : stopButton.hovered ? Theme.dangerHover
                         : Theme.danger

                    Behavior on color {
                        ColorAnimation { duration: Theme.animFast }
                    }
                }

                contentItem: Text {
                    text: stopButton.text
                    color: Theme.textOnAccent
                    font.pixelSize: Theme.fontTitle
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.panelGap

            SectionFrame {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.horizontalStretchFactor: 2

                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: "Translation (X and Y)"
                    color: Theme.textCaption
                    font.pixelSize: Theme.captionFontSize
                }

                DpadDirectionControl {
                    Layout.alignment: Qt.AlignHCenter
                    danger: !root.stepMode
                    centerText: "X Y"
                    homeText: qsTr("HOME")
                    homeEnabled: root.ready && !root.moving && root.fresh

                    activeDirection: root.litFor("x", "y")

                    upEnabled: root.padEnabled("y", AxisController.HighSide)
                    downEnabled: root.padEnabled("y", AxisController.LowSide)
                    leftEnabled: root.padEnabled("x", AxisController.LowSide)
                    rightEnabled: root.padEnabled("x", AxisController.HighSide)

                    onDirectionClicked: (direction) => {
                        switch (direction) {
                        case "up":    root.drive("y", AxisController.HighSide, root.yStep, direction); break
                        case "down":  root.drive("y", AxisController.LowSide, root.yStep, direction); break
                        case "left":  root.drive("x", AxisController.LowSide, root.xStep, direction); break
                        case "right": root.drive("x", AxisController.HighSide, root.xStep, direction); break
                        }
                    }

                    onHomeClicked: AxisController.homeGroup(AxisController.GroupXY)
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.rowSpacing
                    enabled: root.stepMode

                    LabeledField {
                        Layout.fillWidth: true
                        label: "X (mm)"
                        text: root.xStep
                        validator: DecimalValidator { bottom: 0 }
                        onEdited: (value) => root.xStep = parseFloat(value)
                    }

                    LabeledField {
                        Layout.fillWidth: true
                        label: "Y (mm)"
                        text: root.yStep
                        validator: DecimalValidator { bottom: 0 }
                        onEdited: (value) => root.yStep = parseFloat(value)
                    }
                }
            }

            SectionFrame {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.horizontalStretchFactor: 1

                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: "Translation (Z)"
                    color: Theme.textCaption
                    font.pixelSize: Theme.captionFontSize
                }

                DpadDirectionControl {
                    Layout.alignment: Qt.AlignHCenter
                    horizontalEnabled: false
                    danger: !root.stepMode
                    centerText: "Z"
                    homeText: qsTr("HOME")
                    homeEnabled: root.ready && !root.moving && root.fresh

                    activeDirection: root.litFor("z", "z")

                    upEnabled: root.padEnabled("z", AxisController.HighSide)
                    downEnabled: root.padEnabled("z", AxisController.LowSide)

                    onDirectionClicked: (direction) => root.drive(
                        "z", direction === "up" ? AxisController.HighSide
                                                : AxisController.LowSide, root.zStep, direction)

                    onHomeClicked: AxisController.homeGroup(AxisController.GroupZ)
                }

                LabeledField {
                    Layout.fillWidth: true
                    enabled: root.stepMode
                    label: "Z (mm)"
                    text: root.zStep
                    validator: DecimalValidator { bottom: 0 }
                    onEdited: (value) => root.zStep = parseFloat(value)
                }
            }

            SectionFrame {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.horizontalStretchFactor: 2

                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: "Rotation"
                    color: Theme.textCaption
                    font.pixelSize: Theme.captionFontSize
                }

                DpadDirectionControl {
                    Layout.alignment: Qt.AlignHCenter
                    danger: !root.stepMode
                    centerText: "YAW\nPITCH"
                    homeText: qsTr("HOME")
                    homeEnabled: root.ready && !root.moving && root.fresh

                    activeDirection: root.litFor("yaw", "pitch")

                    upEnabled: root.padEnabled("pitch", AxisController.HighSide)
                    downEnabled: root.padEnabled("pitch", AxisController.LowSide)
                    leftEnabled: root.padEnabled("yaw", AxisController.LowSide)
                    rightEnabled: root.padEnabled("yaw", AxisController.HighSide)

                    onDirectionClicked: (direction) => {
                        switch (direction) {
                        case "up":    root.drive("pitch", AxisController.HighSide, root.pitchStep, direction); break
                        case "down":  root.drive("pitch", AxisController.LowSide, root.pitchStep, direction); break
                        case "left":  root.drive("yaw", AxisController.LowSide, root.yawStep, direction); break
                        case "right": root.drive("yaw", AxisController.HighSide, root.yawStep, direction); break
                        }
                    }

                    onHomeClicked: AxisController.homeGroup(AxisController.GroupRotation)
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.rowSpacing
                    enabled: root.stepMode

                    LabeledField {
                        Layout.fillWidth: true
                        label: "Yaw (°)"
                        text: root.yawStep
                        validator: DecimalValidator { bottom: 0 }
                        onEdited: (value) => root.yawStep = parseFloat(value)
                    }

                    LabeledField {
                        Layout.fillWidth: true
                        label: "Pitch (°)"
                        text: root.pitchStep
                        validator: DecimalValidator { bottom: 0 }
                        onEdited: (value) => root.pitchStep = parseFloat(value)
                    }
                }
            }
        }
    }
}
