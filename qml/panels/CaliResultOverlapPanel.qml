pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FisheyeCaliJojo

// Overlap: the old client's tab_overlap, ported.
//
// Two plots side by side, exactly as `cali_result.ui` had them:
//   left  -- label_overlap,        driven by btn_update_overlap
//   right -- label_dist_vs_aggr,   driven by btn_update_dist_vs_aggr
//
// The right plot does NOT run a search. In the old controller
// `updatePlotDistVsAggr()` only redraws `distAggrSamples_`, which the
// Aggregation tab's searches filled. Wiring it to a search here would start a
// minutes-long job from a button labelled "update", so it stays a redraw.
//
// The panel is a pure view: every series arrives as a property and every button
// leaves as a signal. MoilCalibrationResult.qml binds them to
// CalibrationController. That is how the New-UI branch wrote it and it keeps the
// two plots testable without a rig.
Rectangle {
    id: panel

    readonly property int readoutFontSize: Math.round(Theme.unit * 1.1)

    property var zflRounds: []
    property var distAggrSamples: []

    // Per-round colours from the table, so a round is the same colour here as in
    // the Data tab's rows. Empty falls back to the palette, which is what the
    // New-UI branch did unconditionally.
    property var roundColors: []

    // One round drawn on top, fetched with ict_zfl_points. That op ignores the
    // round's enabled flag on purpose -- you need to see a round you have
    // switched OFF in order to decide whether switching it off was right.
    property var inspected: []

    property bool busy: false

    signal updateOverlapRequested()
    signal updateDistVsAggrRequested()
    signal showAloneRequested(int round)
    signal clearInspectedRequested()

    function roundColor(index) {
        const given = panel.roundColors[index]
        return given ? given : Theme.curvePalette[index % Theme.curvePalette.length]
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
        // Drawn last and in the marker colour so it reads on top of the rest
        // rather than becoming one more indistinguishable scatter.
        if (panel.inspected.length > 0)
            out.push({ color: Theme.previewMarker, points: panel.inspected, style: "scatter" })
        return out
    }

    readonly property var overlapLegend: {
        const out = []
        for (let r = 0; r < panel.zflRounds.length; ++r)
            out.push({ color: panel.roundColor(r), label: qsTr("Round %1").arg(r + 1) })
        if (panel.snakePoints.length >= 2)
            out.push({ color: Theme.textPrimary, label: qsTr("Snake") })
        if (panel.inspected.length > 0)
            out.push({ color: Theme.previewMarker, label: qsTr("Inspected") })
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
    clip: true

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
                Layout.minimumWidth: 0
                Layout.preferredWidth: 0
                text: qsTr("Check whether every round lands on one curve. Rounds that separate mean the assumed distance is wrong.")
                color: Theme.textCaption
                font.pixelSize: Theme.captionFontSize
                elide: Text.ElideRight
            }

            Label {
                text: qsTr("Inspect:")
                color: Theme.textCaption
                font.pixelSize: Theme.captionFontSize
            }

            SegmentedControl {
                id: inspectRound
                model: ["1", "2", "3", "4", "5", "6", "7", "8", "9", "10"]
            }

            ActionButton {
                text: qsTr("Show Alone")
                enabled: !panel.busy
                onClicked: panel.showAloneRequested(inspectRound.currentIndex + 1)

                ToolTip.visible: hovered
                ToolTip.delay: Theme.animSlow
                ToolTip.text: qsTr("Draw this round on top, in red, even if it is switched off "
                                 + "-- which is how you tell whether switching it off was right.")
            }

            ActionButton {
                text: qsTr("Clear")
                visible: panel.inspected.length > 0
                onClicked: panel.clearInspectedRequested()
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
