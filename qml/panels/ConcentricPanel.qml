pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FisheyeCaliJojo
import "PatternConfig.js" as PatternConfig

Rectangle {
    id: panel

    readonly property string patternType: "concentric"
    readonly property int layerCount: 25
    readonly property alias layers: layerModel

    property int resolutionH: 1920
    property int resolutionW: 1920
    property bool crossLine: false
    property bool autoUpdate: false
    property color positiveColor: "black"
    property color negativeColor: "white"

    property alias direction: directionCombo.currentIndex

    property url previewSource: ""

    signal importRequested()
    signal exportRequested()
    signal saveImageRequested()
    signal updateRequested()
    signal showRequested(string direction)

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
            const radius = Math.round(row.radius)
            if (radius <= 0) continue

            layers.push({
                "shape": String(row.shape).toLowerCase(),
                "radius": radius,
                "cx": Math.round(row.cx),
                "cy": Math.round(row.cy),
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
                "shape": String(row.shape).toLowerCase(),
                "radius": Math.round(row.radius),
                "cx": Math.round(row.cx),
                "cy": Math.round(row.cy),
                "color": PatternConfig.rgbArray(row.color)
            }
        }

        return doc
    }

    function loadConfig(doc) {
        if (!PatternConfig.isConfigFor(doc, panel))
            return false

        PatternConfig.applyConfigEnvelope(doc, panel)
        const derived = doc.pos_neg_color === true

        for (let i = 0; i < layerModel.count; i++) {
            const layer = doc[String(i + 1)]
            if (!layer) {
                layerModel.setProperty(i, "radius", 0)
                continue
            }

            layerModel.setProperty(i, "shape",
                                   String(layer.shape).toLowerCase() === "square" ? "Square" : "Circle")
            layerModel.setProperty(i, "radius", PatternConfig.toInt(layer.radius, 0))
            layerModel.setProperty(i, "cx", PatternConfig.toInt(layer.cx, 0))
            layerModel.setProperty(i, "cy", PatternConfig.toInt(layer.cy, 0))
            layerModel.setProperty(i, "color",
                                   derived ? panel.layerColorAt(i, true)
                                           : PatternConfig.toColor(layer.color, "#000000"))
        }

        return true
    }

    ListModel {
        id: layerModel

        Component.onCompleted: {
            for (let i = 0; i < panel.layerCount; i++)
                layerModel.append({ shape: "Circle", radius: 60,
                                    color: panel.layerColorAt(i, true), cx: 0, cy: 0 })
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
                text: qsTr("Concentric")
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

            GhostButton { text: qsTr("Import"); onClicked: panel.importRequested() }
            GhostButton { text: qsTr("Export"); onClicked: panel.exportRequested() }

        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
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
                    text: qsTr("( + ) Positive Pattern")
                    onClicked: panel.applyPositivePattern()
                }
                ActionButton {
                    Layout.fillWidth: true
                    text: qsTr("( - ) Negative Pattern")
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
                    onClicked: panel.updateRequested()
                }
                Item { Layout.fillWidth: true }
            }

            // ---- table ----
            // Single source of truth for column widths: both the header row below
            // and every ConcentricLayerRow delegate bind to these same values, so
            // they can never drift apart.
            QtObject {
                id: tableColumns
                readonly property int noWidth:     Math.round(Theme.charUnit * 2.5)
                readonly property int shapeWidth:  Math.round(Theme.charUnit * 9)
                readonly property int radiusWidth: Math.round(Theme.charUnit * 6)
                readonly property int colorWidth:  Math.round(Theme.controlHeight * 0.65)
                readonly property int cxWidth:     Math.round(Theme.charUnit * 5)
                readonly property int cyWidth:     Math.round(Theme.charUnit * 5)
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
                    Label { Layout.preferredWidth: tableColumns.shapeWidth;  Layout.fillWidth: true; text: qsTr("Shape");  color: Theme.textCaption; font.pixelSize: Theme.captionFontSize; font.bold: true; horizontalAlignment: Text.AlignHCenter }
                    Label { Layout.preferredWidth: tableColumns.radiusWidth; Layout.fillWidth: true; text: qsTr("Radius"); color: Theme.textCaption; font.pixelSize: Theme.captionFontSize; font.bold: true; horizontalAlignment: Text.AlignHCenter }
                    Label { Layout.preferredWidth: tableColumns.colorWidth;  Layout.fillWidth: true; text: qsTr("Color");  color: Theme.textCaption; font.pixelSize: Theme.captionFontSize; font.bold: true; horizontalAlignment: Text.AlignHCenter }
                    Label { Layout.preferredWidth: tableColumns.cxWidth;     Layout.fillWidth: true; text: qsTr("Cx");     color: Theme.textCaption; font.pixelSize: Theme.captionFontSize; font.bold: true; horizontalAlignment: Text.AlignHCenter }
                    Label { Layout.preferredWidth: tableColumns.cyWidth;     Layout.fillWidth: true; text: qsTr("Cy");     color: Theme.textCaption; font.pixelSize: Theme.captionFontSize; font.bold: true; horizontalAlignment: Text.AlignHCenter }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
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
                        required property string shape
                        required property real radius
                        required property color color
                        required property real cx
                        required property real cy

                        width: table.width
                        height: layerRow.implicitHeight + Theme.spaceXs

                        Rectangle {
                            anchors.fill: parent
                            color: cell.index % 2 === 0 ? "transparent" : Theme.fieldDisabledBackground
                        }

                        ConcentricLayerRow {
                            id: layerRow

                            anchors.fill: parent
                            anchors.topMargin: Math.round(Theme.spaceXs / 2)
                            anchors.bottomMargin: Math.round(Theme.spaceXs / 2)
                            noWidth: tableColumns.noWidth
                            shapeWidth: tableColumns.shapeWidth
                            radiusWidth: tableColumns.radiusWidth
                            colorWidth: tableColumns.colorWidth
                            cxWidth: tableColumns.cxWidth
                            cyWidth: tableColumns.cyWidth
                            layerNumber: cell.index + 1
                            shape: cell.shape
                            radius: cell.radius
                            color: cell.color
                            cx: cell.cx
                            cy: cell.cy

                            onShapeEdited:  (value) => layerModel.setProperty(cell.index, "shape", value)
                            onRadiusEdited: (value) => layerModel.setProperty(cell.index, "radius", value)
                            onColorEdited:  (value) => layerModel.setProperty(cell.index, "color", String(value))
                            onCxEdited:     (value) => layerModel.setProperty(cell.index, "cx", value)
                            onCyEdited:     (value) => layerModel.setProperty(cell.index, "cy", value)
                        }
                    }
                }
            }
        }
    }
}
