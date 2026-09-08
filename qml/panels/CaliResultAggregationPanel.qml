pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FisheyeCaliJojo

// Aggregation: solve for the distance the rig was actually at.
//
// These are the three CaliJob searches -- the only cancellable work in this
// window. find_distance_for_target_aggregation is 282 probes, each recomputing
// all eleven rounds, which is why it is an action and why Stop is real here and
// advisory everywhere else.
//
// The samples plot is the search's own probe history: distance against the
// aggregation it scored. A clean run is a bowl with one minimum. Two minima, or
// a floor that never rises, means the window is wrong rather than the data.
Rectangle {
    id: panel

    property alias round: roundSelector.currentIndex

    readonly property bool running: CalibrationController.searchRunning
    readonly property var samples: CalibrationController.searchSamples

    readonly property var curves: samples.length > 0
        ? [{ color: Theme.accent, points: samples, style: "scatter" }]
        : []

    // The best distance, drawn as a vertical marker so it can be read against
    // the bowl rather than only as a number in a label.
    readonly property var markers: CalibrationController.bestDistance > 0 && samples.length > 0
        ? [CalibrationController.bestDistance]
        : []

    color: Theme.panelBackground
    border.color: Theme.panelBorder
    radius: Theme.radius

    component NumberField: RowLayout {
        id: field

        property string caption: ""
        property alias value: input.value
        property real from: -100000
        property real to: 100000

        spacing: Theme.labelSpacing

        Label {
            text: field.caption
            color: Theme.textCaption
            font.pixelSize: Theme.captionFontSize
        }

        ValueField {
            id: input

            Layout.preferredWidth: Math.round(Theme.charUnit * 8)
            editable: !panel.running
            validator: DecimalValidator {}
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.panelMargin
        spacing: Theme.rowSpacing

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spaceSm

            Label {
                text: qsTr("Aggregation")
                font.bold: true
                font.pixelSize: Theme.fontTitle
                color: Theme.accent
            }

            Label {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredWidth: 0
                text: panel.running
                        ? qsTr("%1 ...").arg(CalibrationController.searchStage)
                        : CalibrationController.searchSummary !== ""
                            ? CalibrationController.searchSummary
                            : qsTr("Pick a search. These run for minutes and can be stopped.")
                color: panel.running ? Theme.textPrimary : Theme.textCaption
                font.pixelSize: Theme.captionFontSize
                elide: Text.ElideRight
            }

            ActionButton {
                text: qsTr("Stop")
                tone: "danger"
                enabled: panel.running
                onClicked: CalibrationController.cancelSearch()

                ToolTip.visible: hovered
                ToolTip.delay: Theme.animSlow
                ToolTip.text: qsTr("Unwinds the search on the rig. The table comes back at "
                                 + "whatever the last probe wrote, and the answer is marked "
                                 + "partial.")
            }
        }

        // Indeterminate when the op cannot report a total, rather than a 0% bar
        // that looks stuck.
        ProgressBar {
            Layout.fillWidth: true
            visible: panel.running
            indeterminate: CalibrationController.searchTotal <= 0
            from: 0
            to: Math.max(1, CalibrationController.searchTotal)
            value: CalibrationController.searchDone
        }

        SectionFrame {
            Layout.fillWidth: true

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.rowSpacing

                Label {
                    text: qsTr("Round:")
                    color: Theme.textCaption
                    font.pixelSize: Theme.captionFontSize
                }

                SegmentedControl {
                    id: roundSelector
                    model: ["1", "2", "3", "4", "5", "6", "7", "8", "9", "10"]
                }

                NumberField { id: distMin; caption: qsTr("from"); value: "1" }
                NumberField { id: distMax; caption: qsTr("to");   value: "500" }

                ActionButton {
                    text: qsTr("Find Min (this round)")
                    tone: "accent"
                    enabled: !panel.running && CalibrationController.hasRawIct
                    onClicked: CalibrationController.findMinForRound(
                                   panel.round + 1,
                                   parseFloat(distMin.value), parseFloat(distMax.value))

                    ToolTip.visible: hovered
                    ToolTip.delay: Theme.animSlow
                    ToolTip.text: qsTr("Ternary search over one round's distance range.")
                }

                Item { Layout.fillWidth: true }

                // Scores every ENABLED round together at the base distance. The
                // per-round number says how tight one round is with itself; this
                // is the one you judge a whole run on.
                Label {
                    text: qsTr("All rounds: %1").arg(CalibrationController.aggregationText !== ""
                                                     ? CalibrationController.aggregationText
                                                     : qsTr("-"))
                    color: Theme.textCaption
                    font.pixelSize: Theme.captionFontSize
                }

                ActionButton {
                    text: qsTr("Aggregate All Rounds")
                    enabled: !panel.running && !CalibrationController.busy
                             && CalibrationController.hasRawIct
                    onClicked: CalibrationController.aggregationAllRounds(
                                   useWindow.checked,
                                   parseFloat(xLo.value), parseFloat(xHi.value))

                    ToolTip.visible: hovered
                    ToolTip.delay: Theme.animSlow
                    ToolTip.text: qsTr("Scores every enabled round at the base distance, "
                                     + "honouring the ICT window below.")
                }
            }
        }

        SectionFrame {
            Layout.fillWidth: true

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.rowSpacing

                PatternToggleSwitch {
                    id: useWindow
                    text: qsTr("Limit to ICT window")
                    checked: false
                }

                NumberField { id: xLo; caption: qsTr("ICT from"); value: "-2000" }
                NumberField { id: xHi; caption: qsTr("to");       value: "2000" }

                ActionButton {
                    text: qsTr("Find Min (all rounds)")
                    tone: "accent"
                    enabled: !panel.running
                    onClicked: CalibrationController.findMinInWindow(
                                   useWindow.checked,
                                   parseFloat(xLo.value), parseFloat(xHi.value))

                    ToolTip.visible: hovered
                    ToolTip.delay: Theme.animSlow
                    ToolTip.text: qsTr("Coarse sweep then refine, over every enabled round.")
                }

                Item { Layout.fillWidth: true }

                NumberField { id: target; caption: qsTr("Target aggr"); value: "0.1" }

                ActionButton {
                    text: qsTr("Find Distance for Target")
                    enabled: !panel.running
                    onClicked: CalibrationController.findDistanceForTarget(
                                   parseFloat(target.value), useWindow.checked,
                                   parseFloat(xLo.value), parseFloat(xHi.value))

                    ToolTip.visible: hovered
                    ToolTip.delay: Theme.animSlow
                    ToolTip.text: qsTr("282 probes, each recomputing all eleven rounds. "
                                     + "Minutes, not seconds.")
                }
            }
        }

        HistogramPlotView {
            Layout.fillWidth: true
            Layout.fillHeight: true

            xLabel: qsTr("Distance (mm)")
            yLabel: qsTr("Aggregation")
            emptyText: qsTr("No search has run yet")

            defaultXMin: 0
            defaultXMax: 500
            defaultYMin: 0
            defaultYMax: 1

            curves: panel.curves
            markers: panel.markers
        }
    }
}
