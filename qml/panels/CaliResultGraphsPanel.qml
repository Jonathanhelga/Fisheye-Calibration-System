pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FisheyeCaliJojo

Rectangle {
    id: panel

    readonly property int readoutFontSize: Math.round(Theme.unit * 1.1)
    readonly property real rayLength: 300

    property var ranges: []

    property var pupilRays: []
    property var distIhPoints: []
    property var distAlphaPoints: []

    signal updateShiftOfPupilRequested()
    signal updateDistVsIhRangeRequested()
    signal updateDistVsAlphaRequested()

    function roundColor(index) {
        return Theme.curvePalette[index % Theme.curvePalette.length]
    }

    function numberOf(text) {
        const value = Number(String(text).trim())
        return String(text).trim().length > 0 && isFinite(value) ? value : null
    }

    function enabledRanges() {
        const out = []
        for (let i = 1; i < panel.ranges.length; ++i)
            if (panel.ranges[i].enabled)
                out.push({ index: i, values: panel.ranges[i] })
        return out
    }

    function meanPoints(minField, maxField) {
        const rows = panel.enabledRanges()
        const points = []
        for (let i = 0; i < rows.length; ++i) {
            const values = rows[i].values
            const distance = panel.numberOf(values.pctToPupil)
            const low = panel.numberOf(values[minField])
            const high = panel.numberOf(values[maxField])
            if (distance === null || low === null || high === null)
                continue
            points.push({ x: (low + high) / 2, y: distance })
        }
        points.sort((a, b) => a.x - b.x)
        return points
    }

    function rebuildShiftOfPupil() {
        const rows = panel.enabledRanges()
        const rays = []
        for (let i = 0; i < rows.length; ++i) {
            const values = rows[i].values
            const distance = panel.numberOf(values.pctToPupil)
            const low = panel.numberOf(values.alphaMin)
            const high = panel.numberOf(values.alphaMax)
            if (distance === null || low === null || high === null)
                continue
            const theta = (low + high) / 2 * Math.PI / 180
            rays.push({ color: panel.roundColor(rows[i].index - 1),
                        points: [{ x: 0, y: distance },
                                 { x: panel.rayLength * Math.sin(theta),
                                   y: distance + panel.rayLength * Math.cos(theta) }] })
        }
        panel.pupilRays = rays
    }

    function rebuildDistVsIhRange() {
        panel.distIhPoints = panel.meanPoints("ihMin", "ihMax")
    }

    function rebuildDistVsAlpha() {
        panel.distAlphaPoints = panel.meanPoints("alphaMin", "alphaMax")
    }

    readonly property var pupilCurves: {
        const out = []
        for (let i = 0; i < panel.pupilRays.length; ++i) {
            out.push(panel.pupilRays[i])
            out.push({ color: Theme.textPrimary, style: "scatter",
                       points: [panel.pupilRays[i].points[0]] })
        }
        return out
    }

    function traceCurves(points, color) {
        if (points.length === 0)
            return []
        return [{ color: color, points: points },
                { color: Theme.textPrimary, style: "scatter", points: points }]
    }

    readonly property var distIhCurves: panel.traceCurves(panel.distIhPoints, panel.roundColor(1))
    readonly property var distAlphaCurves: panel.traceCurves(panel.distAlphaPoints, panel.roundColor(2))

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
                text: qsTr("Each enabled range contributes one ray and one measured point. Press a plot's Update button to redraw it from the range table.")
                color: Theme.textCaption
                font.pixelSize: Theme.captionFontSize
                elide: Text.ElideRight
            }

            ActionButton {
                id: aboutButton

                text: qsTr("About entrance pupil")
                onClicked: aboutPopup.open()

                ToolTip.visible: hovered
                ToolTip.delay: Theme.animSlow
                ToolTip.text: qsTr("Show what the ray fan and the two distance plots measure.")

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
                            text: qsTr("How to read the graphs")
                            font.bold: true
                            font.pixelSize: Theme.fontTitle
                            color: Theme.accent
                        }

                        Repeater {
                            model: [
                                { caption: qsTr("What the entrance pupil is"),
                                  formula: qsTr("The point the camera looks from. A fisheye lens has no single one: it slides along the optical axis as the off-axis angle grows") },
                                { caption: qsTr("What one ray is"),
                                  formula: qsTr("One enabled range, drawn from (0, PCT to Pupil) at angle (Alpha Min + Alpha Max) / 2") },
                                { caption: qsTr("What the ray fan shows"),
                                  formula: qsTr("Where the rays cross the optical axis. Crossings spread apart means the pupil moved between ranges") },
                                { caption: qsTr("Distance vs IH Range"),
                                  formula: qsTr("x is (IH Min + IH Max) / 2 in percent, y is PCT to Pupil for that range") },
                                { caption: qsTr("Distance vs Alpha"),
                                  formula: qsTr("x is (Alpha Min + Alpha Max) / 2 in degrees, y is PCT to Pupil for that range") },
                                { caption: qsTr("What good looks like"),
                                  formula: qsTr("Both distance plots rise smoothly. A point off the trend marks a range whose distance search did not settle") }
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

                title: qsTr("Shift of Entrance Pupil")
                emptyText: qsTr("No enabled range has an alpha yet")
                readoutFontSize: panel.readoutFontSize

                xLabel: qsTr("Lateral displacement")
                yLabel: qsTr("Optical Axis (distance)")
                xMin: -300
                xMax: 300
                yMin: 0
                yMax: 600

                curves: panel.pupilCurves

                actionText: qsTr("Update Shift of Entrance Pupil")
                onActionTriggered: {
                    panel.rebuildShiftOfPupil()
                    panel.updateShiftOfPupilRequested()
                }
            }

            PlotBlock {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 1

                title: qsTr("Distance vs IH Range")
                emptyText: qsTr("No enabled range has an IH window yet")
                readoutFontSize: panel.readoutFontSize

                xLabel: qsTr("IH Range Mean (%)")
                yLabel: qsTr("Distance")
                xMin: 0
                xMax: 100
                yMin: 0
                yMax: 400

                curves: panel.distIhCurves

                actionText: qsTr("Update Dist vs IH Range")
                onActionTriggered: {
                    panel.rebuildDistVsIhRange()
                    panel.updateDistVsIhRangeRequested()
                }
            }

            PlotBlock {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 1

                title: qsTr("Distance vs Alpha")
                emptyText: qsTr("No enabled range has an alpha yet")
                readoutFontSize: panel.readoutFontSize

                xLabel: qsTr("Alpha Mean (degree)")
                yLabel: qsTr("Distance")
                xMin: 0
                xMax: 90
                yMin: 0
                yMax: 400

                curves: panel.distAlphaCurves

                actionText: qsTr("Update Dist vs Alpha")
                onActionTriggered: {
                    panel.rebuildDistVsAlpha()
                    panel.updateDistVsAlphaRequested()
                }
            }
        }
    }
}
