import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FisheyeCaliJojo

// Overlap: every enabled round's (ICT, alpha) pairs pooled into one scatter.
//
// The per-round plots in the Parameter tab answer "is this round self-consistent".
// This one answers the different question of whether the rounds agree with EACH
// OTHER -- pooled, they should trace a single curve. A round shot at the wrong
// distance, or with a stale pattern, shows up here as a limb peeling away from
// the others, and nowhere else.
//
// The pooling is `global_ict_alpha` on the server. Which rounds count is the
// operator's per-round enable flags, which travel with every request because the
// node has no session to remember them in.
Rectangle {
    id: panel

    readonly property var points: CalibrationController.globalIctAlpha
    readonly property bool hasData: points.length > 0
    readonly property real ictMax: CalibrationController.maxIct > 0
                                     ? CalibrationController.maxIct : 2500

    readonly property var curves: panel.hasData
        ? [{ color: Theme.accent, points: panel.points, style: "scatter" }]
        : []

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
                text: qsTr("Overlap")
                font.bold: true
                font.pixelSize: Theme.fontTitle
                color: Theme.accent
            }

            Label {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredWidth: 0
                text: CalibrationController.busy ? CalibrationController.activity
                    : panel.hasData
                        ? qsTr("%1 pooled points from the enabled rounds").arg(panel.points.length)
                        : qsTr("Nothing pooled yet -- press Update Table, then Update Overlap")
                color: Theme.textCaption
                font.pixelSize: Theme.captionFontSize
                elide: Text.ElideRight
            }

            Label {
                visible: !CalibrationController.seriesFresh && panel.hasData
                text: qsTr("stale -- the table changed")
                color: Theme.statusPartial
                font.pixelSize: Theme.captionFontSize
            }

            ActionButton {
                text: qsTr("Update Overlap")
                tone: "accent"
                enabled: !CalibrationController.busy
                onClicked: CalibrationController.updateSeries()
            }
        }

        HistogramPlotView {
            Layout.fillWidth: true
            Layout.fillHeight: true

            xLabel: qsTr("ICT (pixel)")
            yLabel: qsTr("ALPHA (degree)")
            emptyText: qsTr("No pooled data")

            defaultXMin: -panel.ictMax
            defaultXMax: panel.ictMax
            defaultYMin: 0
            defaultYMax: 90

            curves: panel.curves
        }
    }
}
