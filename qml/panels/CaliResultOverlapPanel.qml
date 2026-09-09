pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FisheyeCaliJojo

Rectangle {
    id: panel

    readonly property int readoutFontSize: Math.round(Theme.unit * 1.1)

    property var zflRounds: []
    property var distAggrSamples: []

    signal updateOverlapRequested()
    signal updateDistVsAggrRequested()

    function roundColor(index) {
        return Theme.curvePalette[index % Theme.curvePalette.length]
    }

    readonly property var snakePoints: {
        const pooled = []
        for (let r = 0; r < panel.zflRounds.length; ++r)
            for (let i = 0; i < panel.zflRounds[r].length; ++i)
                pooled.push(panel.zflRounds[r][i])
        pooled.sort((a, b) => Math.abs(a.x) - Math.abs(b.x))
        return pooled
    }

    readonly property var overlapCurves: {
        const out = []
        for (let r = 0; r < panel.zflRounds.length; ++r)
            out.push({ color: panel.roundColor(r), points: panel.zflRounds[r], style: "scatter" })
        if (panel.snakePoints.length >= 2)
            out.push({ color: Theme.textPrimary, points: panel.snakePoints })
        return out
    }

    readonly property var overlapLegend: {
        const out = []
        for (let r = 0; r < panel.zflRounds.length; ++r)
            out.push({ color: panel.roundColor(r), label: qsTr("Round %1").arg(r + 1) })
        if (panel.snakePoints.length >= 2)
            out.push({ color: Theme.textPrimary, label: qsTr("Snake") })
        return out
    }

    readonly property var distAggrCurves: {
        if (panel.distAggrSamples.length === 0)
            return []
        return [{ color: Theme.accent, points: panel.distAggrSamples },
                { color: Theme.curvePalette[0], points: panel.distAggrSamples, style: "scatter" }]
    }

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
            spacing: Theme.spaceSm

            Label {
                Layout.fillWidth: true
                text: qsTr("Check whether every round lands on one curve. Rounds that separate mean the assumed distance is wrong.")
                color: Theme.textCaption
                font.pixelSize: Theme.captionFontSize
                elide: Text.ElideRight
            }

            ActionButton {
                id: aboutButton

                text: qsTr("About overlap")
                onClicked: aboutPopup.open()

                ToolTip.visible: hovered
                ToolTip.delay: Theme.animSlow
                ToolTip.text: qsTr("Show how the snake line and the aggregation number relate.")

                Popup {
                    id: aboutPopup

                    y: aboutButton.height + Theme.spaceXs
                    x: -width + aboutButton.width
                    padding: Theme.panelMargin
                    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

                    background: Rectangle {
                        color: Theme.panelBackground
                        border.color: Theme.panelBorder
                        radius: Theme.radius
                    }

                    contentItem: ColumnLayout {
                        spacing: Theme.rowSpacing

                        Label {
                            text: qsTr("How to read the overlap plot")
                            font.bold: true
                            font.pixelSize: Theme.fontTitle
                            color: Theme.accent
                        }

                        Repeater {
                            model: [
                                { caption: qsTr("What one dot is"),
                                  formula: qsTr("One pattern ring in one direction: x is its ICT in pixels, y is its ZFL") },
                                { caption: qsTr("Where ZFL comes from"),
                                  formula: "ZFL = ICT / tan(α)" },
                                { caption: qsTr("Why α depends on distance"),
                                  formula: qsTr("α is derived from the assumed camera-to-panel distance, so every dot moves when that distance changes") },
                                { caption: qsTr("The snake line"),
                                  formula: qsTr("All rounds pooled and joined in order of |ICT|, so it crosses between rounds wherever they disagree") },
                                { caption: qsTr("Aggregation"),
                                  formula: qsTr("The total length of that line. Rounds on one curve give a short line, rounds apart give a long one") },
                                { caption: qsTr("What good looks like"),
                                  formula: qsTr("One tight band with the snake line running smoothly along it, no vertical zigzag") }
                            ]

                            ColumnLayout {
                                id: aboutEntry

                                required property var modelData

                                Layout.fillWidth: true
                                spacing: 0

                                Label {
                                    text: aboutEntry.modelData.caption
                                    color: Theme.textCaption
                                    font.pixelSize: Theme.captionFontSize
                                }
                                Label {
                                    text: aboutEntry.modelData.formula
                                    color: Theme.textPrimary
                                    font.bold: true
                                }
                            }
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.panelGap

            PlotBlock {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 1

                title: qsTr("Overlap")
                legend: panel.overlapLegend
                readoutFontSize: panel.readoutFontSize

                xLabel: qsTr("ICT(pixel)")
                yLabel: qsTr("ZFL(pixel)")
                xMin: -2500
                xMax: 2500
                yMin: -600
                yMax: 3000

                curves: panel.overlapCurves

                actionText: qsTr("Update Overlap")
                onActionTriggered: panel.updateOverlapRequested()
            }

            PlotBlock {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 1

                title: qsTr("Aggregation vs. Distance")
                emptyText: qsTr("No search has run yet")
                readoutFontSize: panel.readoutFontSize

                xLabel: qsTr("Distance(pixel)")
                yLabel: qsTr("Aggregation(pixel)")
                xMin: 0
                xMax: 400
                yMin: 1300
                yMax: 1800

                curves: panel.distAggrCurves

                actionText: qsTr("Update Dist vs. Aggr")
                onActionTriggered: panel.updateDistVsAggrRequested()
            }
        }
    }
}
