pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import FisheyeCaliJojo

Rectangle {
    id: root

    property int channel: 1
    property bool showCurves: true

    property var posDirections: []
    property var negDirections: []
    property var colorOverrides: ({})

    readonly property var directionOrder: ["n", "s", "w", "e", "nw", "se", "sw", "ne"]

    readonly property string compareDirection: {
        for (let i = 0; i < directionOrder.length; ++i) {
            const direction = directionOrder[i]
            if (posDirections.indexOf(direction) >= 0 && negDirections.indexOf(direction) >= 0)
                return direction
        }
        return ""
    }

    // Which curves are picked, no color in here. This is the only thing
    // curveShapes depends on, so a color edit never touches sampleCurve.
    readonly property var curveSelection: {
        const out = []
        if (compareDirection !== "") {
            out.push({ side: "pos", direction: compareDirection, auto: Theme.curvePositive })
            out.push({ side: "neg", direction: compareDirection, auto: Theme.curveNegative })
        } else {
            for (let i = 0; i < directionOrder.length; ++i)
                if (posDirections.indexOf(directionOrder[i]) >= 0)
                    out.push({ side: "pos", direction: directionOrder[i], auto: "" })
            for (let j = 0; j < directionOrder.length; ++j)
                if (negDirections.indexOf(directionOrder[j]) >= 0)
                    out.push({ side: "neg", direction: directionOrder[j], auto: "" })
            for (let k = 0; k < out.length; ++k)
                out[k].auto = Theme.curvePalette[k % Theme.curvePalette.length]
        }
        for (let n = 0; n < out.length; ++n)
            out[n].key = out[n].side + ":" + out[n].direction
        return out
    }

    // The expensive part, keyed by "side:direction". Only resamples when the
    // selection or channel changes, never on a color-only edit.
    readonly property var curveShapes: {
        const out = ({})
        for (let i = 0; i < curveSelection.length; ++i) {
            const entry = curveSelection[i]
            out[entry.key] = sampleCurve(entry.side, entry.direction)
        }
        return out
    }

    // Selection plus resolved color. Cheap to rebuild on every colorOverrides
    // edit since it never touches sampleCurve.
    readonly property var curveSet: {
        const out = []
        for (let i = 0; i < curveSelection.length; ++i) {
            const entry = curveSelection[i]
            out.push({
                side: entry.side,
                direction: entry.direction,
                key: entry.key,
                auto: entry.auto,
                color: colorOverrides[entry.key] !== undefined ? colorOverrides[entry.key] : entry.auto
            })
        }
        return out
    }

    readonly property var posColors: colorsFor("pos")
    readonly property var negColors: colorsFor("neg")

    readonly property var plotCurves: {
        if (!showCurves)
            return []
        const out = []
        for (let i = 0; i < curveSet.length; ++i)
            out.push({ color: curveSet[i].color, points: curveShapes[curveSet[i].key] })
        return out
    }

    // The crossings come from the server, not from re-deriving them here.
    //
    // get_list_intersecting_nodes is a calibration measurement -- it is what the
    // ICT columns are built from -- and a second implementation of it in QML
    // would be a second answer to the same question. The server returns the raw
    // crossings of exactly this pair of curves, with no diagonal scaling and no
    // cross-direction reconciliation, so they line up with what is drawn once
    // this panel applies its own scale.
    readonly property var intersections: {
        if (!showCurves || compareDirection === "")
            return []
        const all = ComputeController.histogram["nodes"]
        if (!all)
            return []
        const raw = all[compareDirection]
        if (!raw)
            return []

        const diagonal = compareDirection === "nw" || compareDirection === "se"
                      || compareDirection === "sw" || compareDirection === "ne"
        const scale = diagonal ? Math.SQRT2 : 1

        const out = []
        for (let i = 0; i < raw.length; ++i)
            out.push(raw[i] * scale)
        return out
    }

    readonly property bool hasData: ComputeController.hasHistogram

    readonly property string statusText: ComputeController.busy ? ComputeController.activity
        : !hasData ? qsTr("No measurement yet -- take a pair, then press Direction Diff")
        : !showCurves ? qsTr("Curves hidden")
        : compareDirection !== "" ? qsTr("Comparing %1, %2 crossing(s)")
                                        .arg(compareDirection.toUpperCase())
                                        .arg(intersections.length)
        : curveSet.length === 0 ? qsTr("No direction selected")
        : curveSet.length === 1 ? qsTr("1 curve")
                                : qsTr("%1 curves").arg(curveSet.length)

    function colorsFor(side) {
        const out = ({})
        for (let i = 0; i < curveSet.length; ++i)
            if (curveSet[i].side === side)
                out[curveSet[i].direction] = curveSet[i].color
        return out
    }

    function toggleDirection(side, direction) {
        const list = side === "pos" ? posDirections : negDirections
        const at = list.indexOf(direction)
        const next = at >= 0 ? list.slice(0, at).concat(list.slice(at + 1))
                             : list.concat([direction])
        if (side === "pos")
            posDirections = next
        else
            negDirections = next
    }

    function setCurveColor(key, value) {
        const next = ({})
        for (const existing in colorOverrides)
            next[existing] = colorOverrides[existing]
        next[key] = value
        colorOverrides = next
    }

    function clearCurveColor(key) {
        const next = ({})
        for (const existing in colorOverrides)
            if (existing !== key)
                next[existing] = colorOverrides[existing]
        colorOverrides = next
    }

    // The curve is what the rig measured, or nothing.
    //
    // This used to be a closed-form formula that produced a plausible-looking
    // greyscale trace for any direction, which meant the panel drew a full set of
    // curves whether or not a shot had ever been taken. Reading a node position
    // off one of those was reading a sine wave. An empty list is the honest
    // answer until Direction Diff has run.
    //
    // The sqrt(2) on the diagonals is this panel's own x-axis scaling: a
    // diagonal ray crosses sqrt(2) pixels per step, and the server deliberately
    // does NOT apply it (see histogram_8dir in ComputeDetectOps.cpp) so that the
    // plotted crossings land where the two drawn curves actually meet.
    function sampleCurve(side, direction) {
        const bySide = ComputeController.histogram[side]
        if (!bySide)
            return []
        const values = bySide[direction]
        if (!values || values.length === 0)
            return []

        const diagonal = direction === "nw" || direction === "se"
                      || direction === "sw" || direction === "ne"
        const scale = diagonal ? Math.SQRT2 : 1

        const points = []
        for (let i = 0; i < values.length; ++i)
            points.push({ x: i * scale, y: values[i] })
        return points
    }

    implicitWidth: body.implicitWidth + 2 * Theme.panelMargin
    implicitHeight: body.implicitHeight + 2 * Theme.panelMargin

    color: Theme.panelBackground
    border.color: Theme.panelBorder
    radius: Theme.radius

    ColumnLayout {
        id: body

        anchors.fill: parent
        anchors.margins: Theme.panelMargin
        spacing: Theme.rowSpacing

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.rowSpacing

            Label {
                text: qsTr("Histogram %1").arg(root.channel)
                font.bold: true
                font.pixelSize: Theme.fontTitle
                color: Theme.accent
            }

            Label {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredWidth: 0

                text: root.statusText
                color: root.curveSet.length > 0 && root.showCurves ? Theme.textPrimary
                                                                   : Theme.textCaption
                font.pixelSize: Theme.captionFontSize
                elide: Text.ElideRight
            }

            RangeControl {
                label: plotView.yLabel
                compact: true

                from: plotView.defaultYMin
                to: plotView.defaultYMax
                lowerValue: plotView.yMin
                upperValue: plotView.yMax

                onLowerMoved: (value) => plotView.yMin = value
                onUpperMoved: (value) => plotView.yMax = value
            }

            RangeControl {
                label: plotView.xLabel
                compact: true

                from: plotView.defaultXMin
                to: plotView.defaultXMax
                lowerValue: plotView.xMin
                upperValue: plotView.xMax

                onLowerMoved: (value) => plotView.xMin = value
                onUpperMoved: (value) => plotView.xMax = value
            }

            ActionButton {
                text: qsTr("Show Curve")
                checked: root.showCurves
                enabled: root.curveSet.length > 0
                onClicked: root.showCurves = !root.showCurves
            }

            ActionButton {
                id: colorButton

                text: qsTr("Curve Color")
                checked: colorPopup.opened
                enabled: root.curveSet.length > 0
                onClicked: colorPopup.opened ? colorPopup.close() : colorPopup.open()
            }

            ActionButton {
                text: qsTr("Pop Up")
                checked: popOut.visible
                onClicked: popOut.visible = !popOut.visible
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.rowSpacing

            SectionFrame {
                Layout.alignment: Qt.AlignTop

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.rowSpacing

                    DirectionRosette {
                        label: qsTr("Pos")
                        directions: root.posDirections
                        colors: root.posColors
                        paired: root.negDirections

                        onToggled: (direction) => root.toggleDirection("pos", direction)
                        onCleared: root.posDirections = []
                    }

                    DirectionRosette {
                        label: qsTr("Neg")
                        directions: root.negDirections
                        colors: root.negColors
                        paired: root.posDirections

                        onToggled: (direction) => root.toggleDirection("neg", direction)
                        onCleared: root.negDirections = []
                    }
                }

                Flow {
                    Layout.fillWidth: true
                    Layout.topMargin: Theme.rowSpacing

                    spacing: Theme.labelSpacing
                    visible: root.curveSet.length > 0

                    Repeater {
                        model: root.curveSet

                        RowLayout {
                            id: legendChip

                            required property var modelData

                            spacing: Theme.labelSpacing

                            Rectangle {
                                Layout.alignment: Qt.AlignVCenter

                                implicitWidth: Math.round(Theme.unit * 0.5)
                                implicitHeight: implicitWidth
                                radius: width / 2
                                antialiasing: true
                                color: legendChip.modelData.color
                            }

                            Label {
                                text: qsTr("%1 %2")
                                        .arg(legendChip.modelData.side === "pos" ? qsTr("Pos") : qsTr("Neg"))
                                        .arg(legendChip.modelData.direction.toUpperCase())
                                color: Theme.textPrimary
                                font.pixelSize: Theme.captionFontSize
                            }
                        }
                    }
                }
            }

            HistogramPlotView {
                id: plotView

                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: Theme.unit * 16

                curves: root.plotCurves
                markers: root.intersections
                emptyText: root.curveSet.length === 0 ? qsTr("Pick a direction to plot")
                                                      : qsTr("Curves hidden")
            }
        }
    }

    CurveColorPopup {
        id: colorPopup

        parent: colorButton
        x: colorButton.width - width

        curves: root.curveSet

        onPicked: (key, value) => root.setCurveColor(key, value)
        onCleared: (key) => root.clearCurveColor(key)
        onResetRequested: root.colorOverrides = ({})
    }

    Window {
        id: popOut

        title: qsTr("Histogram %1").arg(root.channel)
        color: Theme.panelBackground

        width: Theme.unit * 68
        height: Theme.unit * 32
        minimumWidth: Theme.unit * 34
        minimumHeight: Theme.unit * 18

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Theme.panelMargin
            spacing: Theme.rowSpacing

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.rowSpacing

                Label {
                    text: root.statusText
                    color: Theme.textCaption
                    font.pixelSize: Theme.captionFontSize
                }

                Repeater {
                    model: root.showCurves ? root.curveSet : []

                    RowLayout {
                        id: chip

                        required property var modelData

                        spacing: Theme.labelSpacing

                        Rectangle {
                            Layout.alignment: Qt.AlignVCenter

                            implicitWidth: Math.round(Theme.unit * 0.5)
                            implicitHeight: implicitWidth
                            radius: width / 2
                            antialiasing: true
                            color: chip.modelData.color
                        }

                        Label {
                            text: qsTr("%1 %2")
                                    .arg(chip.modelData.side === "pos" ? qsTr("Pos") : qsTr("Neg"))
                                    .arg(chip.modelData.direction.toUpperCase())
                            color: Theme.textPrimary
                            font.pixelSize: Theme.captionFontSize
                        }
                    }
                }

                RangeControl {
                    label: popPlotView.yLabel
                    compact: true
                    Layout.fillWidth: true

                    from: popPlotView.defaultYMin
                    to: popPlotView.defaultYMax
                    lowerValue: popPlotView.yMin
                    upperValue: popPlotView.yMax

                    onLowerMoved: (value) => popPlotView.yMin = value
                    onUpperMoved: (value) => popPlotView.yMax = value
                }

                RangeControl {
                    label: popPlotView.xLabel
                    compact: true
                    Layout.fillWidth: true

                    from: popPlotView.defaultXMin
                    to: popPlotView.defaultXMax
                    lowerValue: popPlotView.xMin
                    upperValue: popPlotView.xMax

                    onLowerMoved: (value) => popPlotView.xMin = value
                    onUpperMoved: (value) => popPlotView.xMax = value
                }
            }

            HistogramPlotView {
                id: popPlotView

                Layout.fillWidth: true
                Layout.fillHeight: true

                curves: root.plotCurves
                markers: root.intersections
                emptyText: root.curveSet.length === 0 ? qsTr("Pick a direction to plot")
                                                      : qsTr("Curves hidden")
            }
        }
    }
}
