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

    readonly property var curveSet: {
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
        for (let n = 0; n < out.length; ++n) {
            const key = out[n].side + ":" + out[n].direction
            out[n].key = key
            out[n].color = colorOverrides[key] !== undefined ? colorOverrides[key] : out[n].auto
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
            out.push({ color: curveSet[i].color,
                       points: sampleCurve(curveSet[i].side, curveSet[i].direction) })
        return out
    }

    readonly property var intersections: {
        if (!showCurves || compareDirection === "")
            return []
        const positive = sampleCurve("pos", compareDirection)
        const negative = sampleCurve("neg", compareDirection)
        const count = Math.min(positive.length, negative.length)
        const out = []
        for (let i = 1; i < count; ++i) {
            const before = positive[i - 1].y - negative[i - 1].y
            const after = positive[i].y - negative[i].y
            if ((before < 0) !== (after < 0))
                out.push(positive[i].x)
        }
        return out
    }

    readonly property string statusText: !showCurves ? qsTr("Curves hidden")
        : compareDirection !== "" ? qsTr("Comparing %1").arg(compareDirection.toUpperCase())
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

    function sampleCurve(side, direction) {
        const diagonal = direction === "nw" || direction === "se"
                      || direction === "sw" || direction === "ne"
        const scale = diagonal ? Math.SQRT2 : 1
        const seed = directionOrder.indexOf(direction) + (side === "neg" ? 8 : 0) + channel * 3
        const rate = 0.5 + 0.02 * (seed % 5)
        const base = side === "pos" ? 148 : 126
        const count = 880
        const points = []
        for (let i = 0; i < count; ++i) {
            const travelled = i / count
            const contrast = 96 * Math.pow(1 - travelled, 0.6)
            const value = base - 34 * travelled
                        + contrast * Math.sin(rate * Math.pow(i, 0.82) + seed)
            points.push({ x: i * scale, y: Math.max(0, Math.min(255, value)) })
        }
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
            }

            HistogramPlotView {
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

                Item { Layout.fillWidth: true }
            }

            HistogramPlotView {
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
