import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FisheyeCaliJojo

// Graphs: ZFL against ICT, one curve per round, all on one axis.
//
// The Parameter tab shows the same ZFL-IH series in a small block beside the
// coefficient boxes; this is the same numbers with the whole tab to draw in,
// which is what makes a round that diverges only at the edges visible at all.
//
// Nothing is derived here. The points are `ict_zfl` from the server, cached
// against the table version they were computed from -- so switching to this tab,
// resizing, or moving the cursor reads the cache and touches no wire.
Rectangle {
    id: panel

    readonly property var rounds: CalibrationController.zflRounds
    readonly property var colors: CalibrationController.roundColors
    readonly property bool hasData: rounds.length > 0

    readonly property real ictMax: CalibrationController.maxIct > 0
                                     ? CalibrationController.maxIct : 2500

    function roundColor(index) {
        const given = panel.colors[index]
        return given ? given : Theme.curvePalette[index % Theme.curvePalette.length]
    }

    readonly property var curves: {
        const out = []
        for (let r = 0; r < panel.rounds.length; ++r)
            out.push({ color: panel.roundColor(r), points: panel.rounds[r], style: "scatter" })
        return out
    }

    readonly property var legend: {
        const out = []
        for (let r = 0; r < panel.rounds.length; ++r)
            out.push({ color: panel.roundColor(r), label: qsTr("Round %1").arg(r + 1) })
        return out
    }

    color: Theme.panelBackground
    border.color: Theme.panelBorder
    radius: Theme.radius

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.panelMargin
        spacing: Theme.rowSpacing

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spaceSm

            Label {
                text: qsTr("IH-ZFL")
                font.bold: true
                font.pixelSize: Theme.fontTitle
                color: Theme.accent
            }

            Repeater {
                model: panel.legend

                RowLayout {
                    id: entry

                    required property var modelData

                    spacing: Theme.spaceXs

                    // implicit*, not width/height: this sits in a RowLayout, and
                    // setting width on a layout-managed item is undefined.
                    Rectangle {
                        implicitWidth: Theme.spaceSm
                        implicitHeight: Theme.spaceSm
                        radius: width / 2
                        color: entry.modelData.color
                    }
                    Label {
                        text: entry.modelData.label
                        color: Theme.textCaption
                        font.pixelSize: Theme.captionFontSize
                    }
                }
            }

            Item { Layout.fillWidth: true }

            Label {
                visible: !CalibrationController.seriesFresh && panel.hasData
                text: qsTr("stale -- the table changed")
                color: Theme.statusPartial
                font.pixelSize: Theme.captionFontSize
            }

            ActionButton {
                text: qsTr("Update Graphs")
                tone: "accent"
                enabled: !CalibrationController.busy
                onClicked: CalibrationController.updateSeries()
            }
        }

        HistogramPlotView {
            Layout.fillWidth: true
            Layout.fillHeight: true

            xLabel: qsTr("ICT (pixel)")
            yLabel: qsTr("ZFL (pixel)")
            emptyText: qsTr("No series yet -- press Update Table, then Update Graphs")

            defaultXMin: -panel.ictMax
            defaultXMax: panel.ictMax
            defaultYMin: 0
            defaultYMax: 3000

            curves: panel.curves
        }
    }
}
