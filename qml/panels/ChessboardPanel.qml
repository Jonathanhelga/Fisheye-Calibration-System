pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FisheyeCaliJojo

// Chessboard pattern: a checkerboard tiled outward from the screen center.
// An extra pattern, separate from the 75 PCT values, so it has no layer
// table and no Import/Export (there is no PCT JSON for it).
Rectangle {
    id: panel

    readonly property var pixelSizeOptions: [0.2478, 0.155, 0.293]

    property int resolutionH: 1080
    property int resolutionW: 1920
    property real squareMm: 45
    property color squareColor: "black"
    property color backgroundColor: "#b4b4b4"

    property alias direction: directionCombo.currentIndex

    property url previewSource: ""

    readonly property real pixelSizeMm: pixelSizeOptions[pixelSizeCombo.currentIndex]
    readonly property int squarePx: Math.round(squareMm / pixelSizeMm)

    signal generateRequested()
    signal saveImageRequested()
    signal updateRequested(string direction)

    implicitWidth: content.implicitWidth + 2 * Theme.panelMargin
    implicitHeight: content.implicitHeight + 2 * Theme.panelMargin

    color: Theme.panelBackground
    border.color: Theme.panelBorder
    radius: Theme.radius

    ColumnLayout {
        id: content

        anchors.fill: parent
        spacing: 0

        RowLayout{
            Layout.fillWidth: true
            Layout.margins: Theme.panelMargin
            Layout.bottomMargin: Theme.rowSpacing
            spacing: Theme.spaceXs

            Label {
                text: qsTr("Chessboard")
                font.bold: true
                font.pixelSize: Theme.fontTitle
                color: Theme.accent
            }

            Label {
                text: qsTr("→")
                color: Theme.textCaption
                font.pixelSize: Theme.captionFontSize
            }
            SegmentedControl {
                id: directionCombo
                model: ["TOP", "North", "West", "South", "East"]
            }

            Item { Layout.fillWidth: true }

            GhostButton { text: qsTr("Import"); onClicked: panel.importRequested() }
            GhostButton { text: qsTr("Export"); onClicked: panel.exportRequested() }

        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.panelBorder
        }

        ColumnLayout{
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: Theme.panelMargin
            Layout.topMargin: Theme.rowSpacing
            Layout.bottomMargin: 0
            spacing: Theme.rowSpacing

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.rowSpacing

                LabeledField {
                    Layout.preferredWidth: Math.round(Theme.charUnit * 7)
                    label: qsTr("Height")
                    text: panel.resolutionH
                    validator: IntValidator { bottom: 1; top: 8192 }
                    onEdited: (value) => panel.resolutionH = parseInt(value)
                }

                LabeledField {
                    Layout.preferredWidth: Math.round(Theme.charUnit * 7)
                    label: qsTr("Width")
                    text: panel.resolutionW
                    validator: IntValidator { bottom: 1; top: 8192 }
                    onEdited: (value) => panel.resolutionW = parseInt(value)
                }

                Item { Layout.fillWidth: true }

                ActionButton {
                    Layout.alignment: Qt.AlignBottom
                    text: qsTr("Save Image")
                    onClicked: panel.saveImageRequested()
                }
            }

            ImagePreview {
                id: preview

                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: Theme.unit * 25
                Layout.maximumHeight: Theme.unit * 30

                source: panel.previewSource
                emptyText: qsTr("No preview yet")
                hint: qsTr("%1 x %2").arg(panel.resolutionW).arg(panel.resolutionH)
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                rowSpacing: Theme.spaceXs
                columnSpacing: Theme.rowSpacing

                LabeledField {
                    Layout.fillWidth: true
                    label: qsTr("Square (mm)")
                    text: panel.squareMm
                    validator: DoubleValidator { bottom: 0.1; top: 999; decimals: 2 }
                    onEdited: (value) => panel.squareMm = parseFloat(value)
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.labelSpacing

                    Label {
                        text: qsTr("Pixel size (mm)")
                        color: Theme.textCaption
                        font.pixelSize: Theme.captionFontSize
                    }

                    ComboBox {
                        id: pixelSizeCombo
                        Layout.fillWidth: true
                        Layout.preferredHeight: Theme.controlHeight
                        model: panel.pixelSizeOptions
                    }
                }

                RowLayout {
                    spacing: Theme.spaceXs
                    PatternColorButton {
                        Layout.preferredWidth: Math.round(Theme.controlHeight * 2)
                        Layout.preferredHeight: Math.round(Theme.controlHeight * 1)
                        value: panel.squareColor
                        onPicked: (value) => panel.squareColor = value
                    }
                    Label {
                        text: qsTr("Square color")
                        color: Theme.textCaption
                        font.pixelSize: Theme.captionFontSize
                    }
                }

                RowLayout {
                    spacing: Theme.spaceXs
                    PatternColorButton {
                        Layout.preferredWidth: Math.round(Theme.controlHeight * 2)
                        Layout.preferredHeight: Math.round(Theme.controlHeight * 1)
                        value: panel.backgroundColor
                        onPicked: (value) => panel.backgroundColor = value
                    }
                    Label {
                        text: qsTr("Background")
                        color: Theme.textCaption
                        font.pixelSize: Theme.captionFontSize
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("square_px = round(%1 / %2) = %3 px")
                        .arg(panel.squareMm).arg(panel.pixelSizeMm).arg(panel.squarePx)
                color: Theme.textCaption
                font.pixelSize: Theme.captionFontSize
            }

            RowLayout{
                Layout.fillWidth: true
                spacing: Theme.rowSpacing
                ActionButton {
                    Layout.fillWidth: true
                    text: qsTr("Generate")
                    onClicked: panel.generateRequested()
                }
                ActionButton {
                    Layout.fillWidth: Math.round(Theme.charUnit * 20)
                    tone: "accent"
                    text: qsTr("Update")
                    onClicked: panel.updateRequested(directionCombo.model[directionCombo.currentIndex])
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
            }
        }
    }
}
