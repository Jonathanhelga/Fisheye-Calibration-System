pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FisheyeCaliJojo
import "PatternConfig.js" as PatternConfig

// Stripeline pattern: parallel stripes. Usually shown on the side screens
// (N/W/S/E). Each row's Height is a step, the pixel thickness of that stripe
// from the previous one, not an absolute position.
Rectangle {
    id: panel

    readonly property string patternType: "stripeline"
    readonly property int layerCount: 50
    readonly property alias layers: layerModel

    property int resolutionH: 3840
    property int resolutionW: 2160
    property bool crossLine: false
    property bool autoUpdate: false
    property color positiveColor: "black"
    property color negativeColor: "white"

    property alias direction: directionCombo.currentIndex

    property url previewSource: ""

    signal importRequested()
    signal exportRequested()
    signal saveImageRequested()
    signal updateRequested(string direction)

    function layerColorAt(index, positive) {
        return String((index % 2 === 0) === positive ? panel.positiveColor : panel.negativeColor)
    }

    function applyPositivePattern() {
        for (let i = 0; i < layerModel.count; i++)
            layerModel.setProperty(i, "color", panel.layerColorAt(i, true))
    }

    function applyNegativePattern() {
        for (let i = 0; i < layerModel.count; i++)
            layerModel.setProperty(i, "color", panel.layerColorAt(i, false))
    }

    function specJson() {
        const doc = PatternConfig.specEnvelope(panel)
        const layers = []

        for (let i = 0; i < layerModel.count; i++) {
            const row = layerModel.get(i)
            const interval = Math.round(row.interval)
            if (interval <= 0) continue

            layers.push({
                "interval": interval,
                "rgb": PatternConfig.rgbArray(row.color)
            })
        }

        doc.layers = layers
        return doc
    }

    function configJson() {
        const doc = PatternConfig.configEnvelope(panel)

        for (let i = 0; i < layerModel.count; i++) {
            const row = layerModel.get(i)
            doc[String(i + 1)] = {
                "interval": Math.round(row.interval),
                "color": PatternConfig.rgbArray(row.color)
            }
        }

        return doc
    }

    function loadConfig(doc) {
        if (!doc || String(doc.type) !== panel.patternType)
            return false

        PatternConfig.applyConfigEnvelope(doc, panel)
        const derived = doc.pos_neg_color === true

        for (let i = 0; i < layerModel.count; i++) {
            const layer = doc[String(i + 1)]
            if (!layer) continue

            layerModel.setProperty(i, "interval", PatternConfig.toInt(layer.interval, 0))
            layerModel.setProperty(i, "color",
                                   derived ? panel.layerColorAt(i, true)
                                           : PatternConfig.toColor(layer.color, "#ffffff"))
        }

        return true
    }

    ListModel {
        id: layerModel

        Component.onCompleted: {
            for (let i = 0; i < panel.layerCount; i++)
                layerModel.append({ interval: 77, color: panel.layerColorAt(i, true) })
        }
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
                text: qsTr("Stripeline")
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
                currentIndex: 2
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
                Layout.minimumHeight: Theme.minPatternPreviewHeight
                Layout.maximumHeight: Theme.maxPatternPreviewHeight

                source: panel.previewSource
                emptyText: qsTr("No preview yet")
                hint: qsTr("%1 x %2").arg(panel.resolutionW).arg(panel.resolutionH)
                badgeText: panel.crossLine ? qsTr("CrossLine on") : qsTr("CrossLine off")
                badgeActive: panel.crossLine
            }

            RowLayout{
                Layout.fillWidth: true
                spacing: Theme.spaceXl

                Item { Layout.fillWidth: true }

                PatternToggleSwitch {
                    text: qsTr("CrossLine")
                    checked: panel.crossLine
                    onToggled: (value) => panel.crossLine = value
                }
                RowLayout{
                    RowLayout {
                        spacing: Theme.spaceXs
                        Label {
                            text: qsTr("Positive")
                            color: Theme.textCaption
                            font.pixelSize: Theme.captionFontSize
                        }
                        PatternColorButton {
                            Layout.preferredWidth: Math.round(Theme.controlHeight * 2)
                            Layout.preferredHeight: Math.round(Theme.controlHeight * 1)
                            value: panel.positiveColor
                            onPicked: (value) => panel.positiveColor = value
                        }
                    }
                    RowLayout {
                        spacing: Theme.spaceXs
                        Label {
                            text: qsTr("Negative")
                            color: Theme.textCaption
                            font.pixelSize: Theme.captionFontSize
                        }
                        PatternColorButton {
                            Layout.preferredWidth: Math.round(Theme.controlHeight * 2)
                            Layout.preferredHeight: Math.round(Theme.controlHeight * 1)
                            value: panel.negativeColor
                            onPicked: (value) => panel.negativeColor = value
                        }
                    }
                }

                Item { Layout.fillWidth: true }
            }

            RowLayout{
                Layout.fillWidth: true
                spacing: Theme.rowSpacing
                ActionButton {
                    Layout.fillWidth: true
                    text: qsTr("Positive ( + ) Pattern")
                    onClicked: panel.applyPositivePattern()
                }
                ActionButton {
                    Layout.fillWidth: true
                    text: qsTr("Negative ( - ) Pattern")
                    onClicked: panel.applyNegativePattern()
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
                    onClicked: panel.updateRequested(PatternConfig.wireDirection(directionCombo.currentIndex))
                }
                Item { Layout.fillWidth: true }
            }

            // ---- table ----
            // Single source of truth for column widths: both the header row below
            // and every StripelineLayerRow delegate bind to these same values, so
            // they can never drift apart.
            QtObject {
                id: tableColumns
                readonly property int noWidth:     Math.round(Theme.charUnit * 2.5)
                readonly property int heightWidth: Math.round(Theme.charUnit * 6)
                readonly property int colorWidth:  Math.round(Theme.controlHeight * 0.65)
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: Theme.minPatternTableHeight
                spacing: 0

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.rowSpacing

                    Label { Layout.preferredWidth: tableColumns.noWidth;     Layout.fillWidth: true; text: qsTr("No.");    color: Theme.textCaption; font.pixelSize: Theme.captionFontSize; font.bold: true; horizontalAlignment: Text.AlignHCenter }
                    Label { Layout.preferredWidth: tableColumns.heightWidth; Layout.fillWidth: true; text: qsTr("Height"); color: Theme.textCaption; font.pixelSize: Theme.captionFontSize; font.bold: true; horizontalAlignment: Text.AlignHCenter }
                    Label { Layout.preferredWidth: tableColumns.colorWidth;  Layout.fillWidth: true; text: qsTr("Color");  color: Theme.textCaption; font.pixelSize: Theme.captionFontSize; font.bold: true; horizontalAlignment: Text.AlignHCenter }
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: Theme.panelBorder
                }

                ListView {
                    id: table

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.topMargin: Theme.spaceXs

                    clip: true
                    spacing: Theme.spaceMd
                    boundsBehavior: Flickable.StopAtBounds
                    model: layerModel
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                    delegate: Item {
                        id: cell

                        required property int index
                        required property real interval
                        required property color color

                        width: table.width
                        height: layerRow.implicitHeight + Theme.spaceXs

                        Rectangle {
                            anchors.fill: parent
                            color: cell.index % 2 === 0 ? "transparent" : Theme.fieldDisabledBackground
                        }

                        StripelineLayerRow {
                            id: layerRow

                            anchors.fill: parent
                            anchors.topMargin: Math.round(Theme.spaceXs / 2)
                            anchors.bottomMargin: Math.round(Theme.spaceXs / 2)
                            noWidth: tableColumns.noWidth
                            heightWidth: tableColumns.heightWidth
                            colorWidth: tableColumns.colorWidth
                            layerNumber: cell.index + 1
                            interval: cell.interval
                            color: cell.color

                            onIntervalEdited: (value) => layerModel.setProperty(cell.index, "interval", value)
                            onColorEdited:    (value) => layerModel.setProperty(cell.index, "color", String(value))
                        }
                    }
                }
            }
        }
    }
}
