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

    signal commandRequested(string endpoint, real distance)

    implicitWidth: content.implicitWidth + 2 * Theme.panelMargin
    implicitHeight: content.implicitHeight + 2 * Theme.panelMargin
    Layout.minimumWidth: implicitWidth

    color: Theme.panelBackground
    border.color: Theme.panelBorder
    radius: Theme.radius

    ColumnLayout {
        id: content

        anchors.fill: parent
        anchors.margins: Theme.panelMargin
        spacing: Theme.rowSpacing

        Label {
            Layout.alignment: Qt.AlignVCenter
            text: "Axis Control"
            font.bold: true
            color: Theme.accent
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.rowSpacing

            AxisReadout { label: "X"; value: "?" }
            AxisReadout { label: "Y"; value: "?" }
            AxisReadout { label: "Z"; value: "?" }
            AxisReadout { label: "Yaw"; value: "?" }
            AxisReadout { label: "Pitch"; value: "?" }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.panelGap

            SectionFrame {
                Layout.fillWidth: true
                Layout.horizontalStretchFactor: 2
                Layout.minimumWidth: 3 * Theme.padButtonSize + 2 * Theme.fieldPadding
                Layout.alignment: Qt.AlignTop

                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: "Translation (X and Y)"
                    color: Theme.textCaption
                    font.pixelSize: Theme.captionFontSize
                }

                DpadDirectionControl {
                    Layout.alignment: Qt.AlignHCenter
                    centerText: "X Y"

                    onDirectionClicked: (direction) => {
                        const vertical = direction === "up" || direction === "down"
                        const endpoint = ({ up: "y_up", down: "y_down",
                                            left: "x_left", right: "x_right" })[direction]
                        root.commandRequested(
                            endpoint, vertical ? root.yStep : root.xStep)
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.rowSpacing

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
                Layout.horizontalStretchFactor: 1
                Layout.minimumWidth: 3 * Theme.padButtonSize + 2 * Theme.fieldPadding
                Layout.alignment: Qt.AlignTop
                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: "Translation (Z)"
                    color: Theme.textCaption
                    font.pixelSize: Theme.captionFontSize
                }

                DpadDirectionControl {
                    Layout.alignment: Qt.AlignHCenter
                    horizontalEnabled: false
                    centerText: "Z"

                    onDirectionClicked: (direction) => root.commandRequested(
                        direction === "up" ? "z_forward" : "z_back", root.zStep)
                }

                LabeledField {
                    Layout.fillWidth: true
                    label: "Z (mm)"
                    text: root.zStep
                    validator: DoubleValidator { bottom: 0 }
                    onEdited: (value) => root.zStep = parseFloat(value)
                }
            }

            SectionFrame {
                Layout.fillWidth: true
                Layout.horizontalStretchFactor: 2
                Layout.minimumWidth: 3 * Theme.padButtonSize + 2 * Theme.fieldPadding
                Layout.alignment: Qt.AlignTop

                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: "Rotation"
                    color: Theme.textCaption
                    font.pixelSize: Theme.captionFontSize
                }

                DpadDirectionControl {
                    Layout.alignment: Qt.AlignHCenter
                    centerText: "YAW\nPITCH"

                    onDirectionClicked: (direction) => {
                        const vertical = direction === "up" || direction === "down"
                        const endpoint = ({ up: "pitch_up", down: "pitch_down",
                                            left: "yaw_left", right: "yaw_right" })[direction]
                        root.commandRequested(
                            endpoint, vertical ? root.pitchStep : root.yawStep)
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.rowSpacing

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
