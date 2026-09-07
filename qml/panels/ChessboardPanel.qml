pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FisheyeCaliJojo
import "PatternConfig.js" as PatternConfig

// Chessboard pattern: a checkerboard tiled outward from the screen center.
// An extra pattern, separate from the 75 PCT values, so it has no layer
// table and no Import/Export (there is no PCT JSON for it).
Rectangle {
    id: panel

    readonly property string patternType: "chessboard"

    property var pixelSizeOptions: [0.2478, 0.155, 0.293]

    property int resolutionH: 1080
    property int resolutionW: 1920
    property real squareMm: 45

    // Declared here because the Auto Update switch below binds to it. Without it
    // the binding assigned [undefined] to a bool and the engine logged
    // "Unable to assign [undefined] to bool" on every startup -- the sibling
    // Concentric and Stripeline panels both declare it and this one did not.
    property bool autoUpdate: false

    property bool crossLine: false
    property color positiveColor: "black"
    property color negativeColor: "#b4b4b4"

    property alias direction: directionCombo.currentIndex

    property url previewSource: ""

    readonly property real pixelSizeMm: panel.pixelSizeOptions[Math.max(0, pixelSizeCombo.currentIndex)]
    readonly property int squarePx: Math.round(squareMm / pixelSizeMm)

    property bool connected: false

    // No layer table here, so every input is a bindable property and specJson()
    // alone is a sufficient fingerprint -- no revision counter needed, unlike the
    // concentric and stripeline panels.
    readonly property string configFingerprint: JSON.stringify(panel.specJson())

    AutoRefresh {
        enabled: panel.autoUpdate && panel.connected
        fingerprint: panel.configFingerprint
        onTriggered: panel.updateRequested()
    }

    signal saveImageRequested()
    signal updateRequested()
    signal showRequested(string direction)

    function selectPixelSize(value) {
        const options = panel.pixelSizeOptions.slice()
        let index = options.findIndex((v) => Math.abs(v - value) < 1e-9)

        if (index < 0) {
            options.push(value)
            panel.pixelSizeOptions = options
            index = options.length - 1
        }

        pixelSizeCombo.currentIndex = index
    }

    function specJson() {
        const doc = PatternConfig.specEnvelope(panel)

        doc.square_mm = panel.squareMm
        doc.pixel_size_mm = panel.pixelSizeMm
        doc.fg_rgb = PatternConfig.rgbArray(panel.positiveColor)
        doc.bg_rgb = PatternConfig.rgbArray(panel.negativeColor)

        return doc
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
                model: PatternConfig.directionLabels()
            }

            ActionButton {
                text: qsTr("Show on Monitor")
                tone: "accent"
                onClicked: panel.showRequested(PatternConfig.wireDirection(directionCombo.currentIndex))

                ToolTip.visible: hovered
                ToolTip.delay: Theme.animSlow
                ToolTip.text: qsTr("Put this pattern on the selected screen. The rig renders it at that screen's own resolution.")
            }

            Item { Layout.fillWidth: true }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.panelBorder
        }

        ColumnLayout{
            id: body

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
                Layout.preferredHeight: Math.round(body.width * panel.resolutionH / panel.resolutionW)
                Layout.minimumHeight: Theme.minPatternPreviewHeight
                Layout.maximumHeight: Theme.maxPatternPreviewHeight

                source: panel.previewSource
                emptyText: qsTr("No preview yet")
                hint: qsTr("%1 x %2").arg(panel.resolutionW).arg(panel.resolutionH)
            }
            RowLayout{
                Layout.fillWidth: true
                spacing: Theme.rowSpacing
                Item { Layout.fillWidth: true }

                ColumnLayout {
                    spacing: Theme.labelSpacing

                    Label {
                        text: qsTr("Square")
                        color: Theme.textCaption
                        font.pixelSize: Theme.captionFontSize
                    }
                    PatternColorButton {
                        Layout.preferredWidth: Math.round(Theme.charUnit * 9)
                        Layout.preferredHeight: Theme.controlHeight
                        value: panel.positiveColor
                        onPicked: (value) => panel.positiveColor = value
                    }
                }

                ColumnLayout {
                    spacing: Theme.labelSpacing

                    Label {
                        text: qsTr("Background")
                        color: Theme.textCaption
                        font.pixelSize: Theme.captionFontSize
                    }
                    PatternColorButton {
                        Layout.preferredWidth: Math.round(Theme.charUnit * 9)
                        Layout.preferredHeight: Theme.controlHeight
                        value: panel.negativeColor
                        onPicked: (value) => panel.negativeColor = value
                    }
                }
                Item { Layout.fillWidth: true }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.rowSpacing

                LabeledField {
                    // Layout.preferredWidth: Math.round(Theme.charUnit * 9)
                    label: qsTr("Square (mm)")
                    text: panel.squareMm
                    validator: DecimalValidator { bottom: 0.1; top: 999; decimals: 2 }
                    onEdited: (value) => panel.squareMm = parseFloat(value)
                }

                ColumnLayout {
                    Layout.preferredWidth: Math.round(Theme.charUnit * 12)
                    spacing: Theme.labelSpacing

                    Label {
                        text: qsTr("Pixel size (mm)")
                        color: Theme.textCaption
                        font.pixelSize: Theme.captionFontSize
                    }
                    SegmentedControl{
                        id: pixelSizeCombo
                        model: panel.pixelSizeOptions
                    }
                }

                Item { Layout.fillWidth: true }

                Label {
                    Layout.alignment: Qt.AlignBottom
                    Layout.bottomMargin: Math.round((Theme.controlHeight - implicitHeight) / 2)
                    text: qsTr("square_px = %1").arg(panel.squarePx)
                    color: Theme.textCaption
                    font.pixelSize: Theme.captionFontSize
                }
            }



            RowLayout{
                Layout.fillWidth: true
                spacing: Theme.spaceLg
                Item { Layout.fillWidth: true }
                PatternToggleSwitch {
                    text: qsTr("Auto Update")
                    checked: panel.autoUpdate
                    onToggled: (value) => panel.autoUpdate = value
                }
                ActionButton {
                    Layout.fillWidth: Math.round(Theme.charUnit * 20)
                    tone: "accent"
                    text: qsTr("Update")
                    onClicked: panel.updateRequested()
                }
                Item { Layout.fillWidth: true }
            }
            

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
            }
        }
    }
}
