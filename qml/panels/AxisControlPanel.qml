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

    property string activeMotion: ""

    readonly property bool moving: activeMotion !== ""

    readonly property var motionNames: ({
        x_left:     qsTr("X left"),
        x_right:    qsTr("X right"),
        y_up:       qsTr("Y up"),
        y_down:     qsTr("Y down"),
        z_forward:  qsTr("Z upward"),
        z_back:     qsTr("Z backward"),
        yaw_left:   qsTr("Yaw left"),
        yaw_right:  qsTr("Yaw right"),
        pitch_up:   qsTr("Pitch up"),
        pitch_down: qsTr("Pitch down"),
        xy_home:       qsTr("X and Y home"),
        z_home:        qsTr("Z home"),
        rotation_home: qsTr("Yaw and Pitch home")
    })

    readonly property string motionText: moving
        ? qsTr("Moving %1").arg(motionNames[activeMotion] || activeMotion)
        : qsTr("Idle")

    signal commandRequested(string endpoint, string mode, real distance)
    signal homeRequested(string group)
    signal stopRequested()

    function requestMove(endpoint, step) {
        activeMotion = endpoint
        commandRequested(endpoint, stepMode ? "step" : "max", stepMode ? step : 0)
    }

    function requestHome(group) {
        activeMotion = group + "_home"
        homeRequested(group)
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
                status: root.moving ? ServerProbe.Checking : ServerProbe.Unknown
                color: root.moving ? Theme.statusOk : Theme.statusUnknown
            }

            Label {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredWidth: 0
                text: root.motionText
                color: root.moving ? Theme.textPrimary : Theme.textCaption
                font.pixelSize: Theme.captionFontSize
                elide: Text.ElideRight
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.rowSpacing

            AxisReadout { Layout.fillWidth: true; label: "X"; value: "?" }
            AxisReadout { Layout.fillWidth: true; label: "Y"; value: "?" }
            AxisReadout { Layout.fillWidth: true; label: "Z"; value: "?" }
            AxisReadout { Layout.fillWidth: true; label: "Yaw"; value: "?" }
            AxisReadout { Layout.fillWidth: true; label: "Pitch"; value: "?" }
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
                    currentIndex: 0
                }

                Label {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    Layout.preferredWidth: 0
                    text: root.stepMode ? qsTr("moves by the step"): qsTr(" mechanical limit")
                    color: Theme.textCaption
                    font.pixelSize: Theme.captionFontSize
                    elide: Text.ElideRight
                }
            }
            RowLayout{
                Layout.fillWidth: true
                spacing: Theme.rowSpacing
                Label {
                    text: qsTr("Speed")
                    color: Theme.textCaption
                    font.pixelSize: Theme.captionFontSize
                }
                ComboBox {
                    Layout.preferredHeight: Theme.controlHeight
                    model: [ "High", "Normal", "Slow"]
                }
            }
            Button {
                id: stopButton

                Layout.preferredWidth: Math.round(Theme.charUnit * 12)
                Layout.preferredHeight: Theme.controlHeight
                Layout.alignment: Qt.AlignRight

                text: qsTr("STOP")
                onClicked: {
                    root.activeMotion = ""
                    root.stopRequested()
                }

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
                    homeEnabled: !root.moving

                    onDirectionClicked: (direction) => {
                        const vertical = direction === "up" || direction === "down"
                        const endpoint = ({ up: "y_up", down: "y_down",
                                            left: "x_left", right: "x_right" })[direction]
                        root.requestMove(endpoint, vertical ? root.yStep : root.xStep)
                    }

                    onHomeClicked: root.requestHome("xy")
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.rowSpacing
                    enabled: root.stepMode

                    LabeledField {
                        Layout.fillWidth: true
                        label: "X (mm)"
                        text: root.xStep
                        validator: DoubleValidator { bottom: 0 }
                        onEdited: (value) => root.xStep = parseFloat(value)
                    }

                    LabeledField {
                        Layout.fillWidth: true
                        label: "Y (mm)"
                        text: root.yStep
                        validator: DoubleValidator { bottom: 0 }
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
                    homeEnabled: !root.moving

                    onDirectionClicked: (direction) => root.requestMove(
                        direction === "up" ? "z_forward" : "z_back", root.zStep)

                    onHomeClicked: root.requestHome("z")
                }

                LabeledField {
                    Layout.fillWidth: true
                    enabled: root.stepMode
                    label: "Z (mm)"
                    text: root.zStep
                    validator: DoubleValidator { bottom: 0 }
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
                    homeEnabled: !root.moving

                    onDirectionClicked: (direction) => {
                        const vertical = direction === "up" || direction === "down"
                        const endpoint = ({ up: "pitch_up", down: "pitch_down",
                                            left: "yaw_left", right: "yaw_right" })[direction]
                        root.requestMove(endpoint, vertical ? root.pitchStep : root.yawStep)
                    }

                    onHomeClicked: root.requestHome("rotation")
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.rowSpacing
                    enabled: root.stepMode

                    LabeledField {
                        Layout.fillWidth: true
                        label: "Yaw (°)"
                        text: root.yawStep
                        validator: DoubleValidator { bottom: 0 }
                        onEdited: (value) => root.yawStep = parseFloat(value)
                    }

                    LabeledField {
                        Layout.fillWidth: true
                        label: "Pitch (°)"
                        text: root.pitchStep
                        validator: DoubleValidator { bottom: 0 }
                        onEdited: (value) => root.pitchStep = parseFloat(value)
                    }
                }
            }
        }
    }
}
