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

    signal commandRequested(string endpoint, string mode, real distance)

    function requestMove(endpoint, step) {
        commandRequested(endpoint, stepMode ? "step" : "max", stepMode ? step : 0)
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

        Label {
            text: "Axis Control"
            font.bold: true
            color: Theme.accent
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
                    text: root.stepMode ? qsTr("moves by the amount below"): qsTr("drives to the mechanical limit")
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
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.panelGap

            SectionFrame {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.horizontalStretchFactor: 2
                Layout.minimumWidth: Theme.dpadMinWidth(3)

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

                    onDirectionClicked: (direction) => {
                        const vertical = direction === "up" || direction === "down"
                        const endpoint = ({ up: "y_up", down: "y_down",
                                            left: "x_left", right: "x_right" })[direction]
                        root.requestMove(endpoint, vertical ? root.yStep : root.xStep)
                    }
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
                Layout.minimumWidth: Theme.dpadMinWidth(1)

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

                    onDirectionClicked: (direction) => root.requestMove(
                        direction === "up" ? "z_forward" : "z_back", root.zStep)
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
                Layout.minimumWidth: Theme.dpadMinWidth(3)

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

                    onDirectionClicked: (direction) => {
                        const vertical = direction === "up" || direction === "down"
                        const endpoint = ({ up: "pitch_up", down: "pitch_down",
                                            left: "yaw_left", right: "yaw_right" })[direction]
                        root.requestMove(endpoint, vertical ? root.pitchStep : root.yawStep)
                    }
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
