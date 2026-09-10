pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import FisheyeCaliJojo

Rectangle {
    id: root

    property string xLabel: qsTr("IH")
    property string yLabel: qsTr("Gray Scale")

    // Off gives their margin back to the canvas.
    property bool axisTitles: true

    readonly property int marginLeft: axisTitles ? Theme.plotMarginLeft
                                                 : Theme.plotTickLabelWidth
    readonly property int marginBottom: axisTitles ? Theme.plotMarginBottom
                                                   : Theme.plotTickLabelHeight
    property string emptyText: qsTr("No curve selected")

    property real defaultXMin: 0
    property real defaultXMax: 2000
    property real defaultYMin: 0
    property real defaultYMax: 200

    property real xMin: defaultXMin
    property real xMax: defaultXMax
    property real yMin: defaultYMin
    property real yMax: defaultYMax

    property int xDivisions: Theme.plotXDivisions
    property int yDivisions: Theme.plotYDivisions

    property var curves: []
    property var markers: []
    property var regions: []

    property bool interactive: true

    property int readoutFontSize: Theme.captionFontSize

    property real scatterRadius: Math.max(1.5, Theme.unit * 0.14)

    readonly property real xSpan: (xMax - xMin) !== 0 ? xMax - xMin : 1
    readonly property real ySpan: (yMax - yMin) !== 0 ? yMax - yMin : 1

    readonly property var dataExtent: {
        let xLo = Infinity, xHi = -Infinity, yLo = Infinity, yHi = -Infinity

        for (let ci = 0; ci < curves.length; ++ci) {
            const points = curves[ci].points
            if (!points)
                continue
            for (let i = 0; i < points.length; ++i) {
                const p = points[i]
                if (p.x < xLo) xLo = p.x
                if (p.x > xHi) xHi = p.x
                if (p.y < yLo) yLo = p.y
                if (p.y > yHi) yHi = p.y
            }
        }

        for (let mi = 0; mi < markers.length; ++mi) {
            const m = markers[mi]
            if (m < xLo) xLo = m
            if (m > xHi) xHi = m
        }

        for (let ri = 0; ri < regions.length; ++ri) {
            const lo = Math.min(regions[ri].min, regions[ri].max)
            const hi = Math.max(regions[ri].min, regions[ri].max)
            if (lo < xLo) xLo = lo
            if (hi > xHi) xHi = hi
        }

        const padX = xHi > xLo ? (xHi - xLo) * Theme.plotBoundsPadding : 0
        const padY = yHi > yLo ? (yHi - yLo) * Theme.plotBoundsPadding : 0
        return { xMin: xLo - padX, xMax: xHi + padX,
                 yMin: yLo - padY, yMax: yHi + padY }
    }

    readonly property real boundsXMin: Math.min(defaultXMin, dataExtent.xMin)
    readonly property real boundsXMax: Math.max(defaultXMax, dataExtent.xMax)
    readonly property real boundsYMin: Math.min(defaultYMin, dataExtent.yMin)
    readonly property real boundsYMax: Math.max(defaultYMax, dataExtent.yMax)

    readonly property bool canPan: (xMax - xMin) < (boundsXMax - boundsXMin) - 1e-6
                                   || (yMax - yMin) < (boundsYMax - boundsYMin) - 1e-6

    readonly property bool empty: curves.length === 0

    readonly property bool cursorInside: cursor.tracking
    readonly property real cursorX: cursor.tracking ? toDataX(cursor.mouseX) : 0
    readonly property real cursorY: cursor.tracking ? toDataY(cursor.mouseY) : 0

    function toPxX(v) { return (v - xMin) / xSpan * area.width }
    function toPxY(v) { return area.height - (v - yMin) / ySpan * area.height }
    function toDataX(px) { return xMin + px / area.width * xSpan }
    function toDataY(py) { return yMin + (area.height - py) / area.height * ySpan }

    function resetView() {
        xMin = defaultXMin
        xMax = defaultXMax
        yMin = defaultYMin
        yMax = defaultYMax
    }

    function clampRange(min, max, lo, hi) {
        const limit = hi - lo
        const span = Math.min(max - min, limit)
        let clampedMin = Math.max(lo, min)
        let clampedMax = clampedMin + span
        if (clampedMax > hi) {
            clampedMax = hi
            clampedMin = clampedMax - span
        }
        return { min: clampedMin, max: clampedMax }
    }

    function zoomAt(px, py, factor) {
        const ax = toDataX(px)
        const ay = toDataY(py)
        const x = clampRange(ax + (xMin - ax) * factor, ax + (xMax - ax) * factor,
                             boundsXMin, boundsXMax)
        const y = clampRange(ay + (yMin - ay) * factor, ay + (yMax - ay) * factor,
                             boundsYMin, boundsYMax)
        xMin = x.min
        xMax = x.max
        yMin = y.min
        yMax = y.max
    }

    function panBy(dxPx, dyPx) {
        const dx = dxPx / area.width * xSpan
        const dy = dyPx / area.height * ySpan
        const x = clampRange(xMin - dx, xMax - dx, boundsXMin, boundsXMax)
        const y = clampRange(yMin + dy, yMax + dy, boundsYMin, boundsYMax)
        xMin = x.min
        xMax = x.max
        yMin = y.min
        yMax = y.max
    }

    function nearestPoint(px, py) {
        const reach = Theme.plotHoverRadius
        let best = reach * reach
        let found = null
        for (let ci = 0; ci < curves.length; ++ci) {
            const curve = curves[ci]
            const points = curve.points
            for (let i = 0; i < points.length; ++i) {
                const qx = toPxX(points[i].x)
                if (Math.abs(qx - px) > reach)
                    continue
                const qy = toPxY(points[i].y)
                const distance = (qx - px) * (qx - px) + (qy - py) * (qy - py)
                if (distance < best) {
                    best = distance
                    found = { x: points[i].x, y: points[i].y, color: curve.color }
                }
            }
        }
        return found
    }

    implicitWidth: Theme.unit * 24
    implicitHeight: Theme.unit * 10

    color: Theme.plotBackground
    border.color: Theme.panelBorder
    radius: Theme.radius
    clip: true

    onCurvesChanged: canvas.requestPaint()
    onMarkersChanged: canvas.requestPaint()
    onRegionsChanged: canvas.requestPaint()
    onXMinChanged: canvas.requestPaint()
    onXMaxChanged: canvas.requestPaint()
    onYMinChanged: canvas.requestPaint()
    onYMaxChanged: canvas.requestPaint()

    Item {
        id: area

        x: root.marginLeft
        y: Theme.plotMarginTop
        width: Math.max(1, root.width - root.marginLeft - Theme.plotMarginRight)
        height: Math.max(1, root.height - Theme.plotMarginTop - root.marginBottom)

        Repeater {
            model: root.xDivisions + 1

            Item {
                id: xTick

                required property int index
                readonly property real px: Math.round(area.width * index / root.xDivisions)

                Rectangle {
                    x: xTick.px
                    width: 1
                    height: area.height
                    color: Theme.plotGrid
                }

                Label {
                    x: xTick.px - width / 2
                    y: area.height + Theme.spaceXs
                    text: Math.round(root.xMin + root.xSpan * xTick.index / root.xDivisions)
                    color: Theme.textCaption
                    font.pixelSize: Theme.captionFontSize
                }
            }
        }

        Repeater {
            model: root.yDivisions + 1

            Item {
                id: yTick

                required property int index
                readonly property real px: Math.round(area.height - area.height * index / root.yDivisions)

                Rectangle {
                    y: yTick.px
                    width: area.width
                    height: 1
                    color: Theme.plotGrid
                }

                Label {
                    x: -width - Theme.spaceXs
                    y: yTick.px - height / 2
                    text: Math.round(root.yMin + root.ySpan * yTick.index / root.yDivisions)
                    color: Theme.textCaption
                    font.pixelSize: Theme.captionFontSize
                }
            }
        }

        Item {
            anchors.fill: parent
            clip: true

            Repeater {
                model: root.markers

                Rectangle {
                    required property real modelData

                    x: Math.round(root.toPxX(modelData))
                    width: 1
                    height: area.height
                    color: Theme.plotMarker
                    opacity: 0.7
                }
            }

            Canvas {
                id: canvas

                anchors.fill: parent
                renderStrategy: Canvas.Cooperative

                onPaint: {
                    const ctx = getContext("2d")
                    ctx.reset()
                    ctx.lineWidth = Theme.plotCurveWidth
                    ctx.lineJoin = "round"

                    for (let ri = 0; ri < root.regions.length; ++ri) {
                        const region = root.regions[ri]
                        const left = root.toPxX(Math.min(region.min, region.max))
                        const right = root.toPxX(Math.max(region.min, region.max))
                        ctx.fillStyle = region.color
                        ctx.fillRect(left, 0, Math.max(1, right - left), height)
                    }

                    for (let ci = 0; ci < root.curves.length; ++ci) {
                        const curve = root.curves[ci]
                        const points = curve.points
                        if (!points || points.length === 0)
                            continue

                        if (curve.style === "scatter") {
                            ctx.fillStyle = curve.color
                            for (let s = 0; s < points.length; ++s) {
                                ctx.beginPath()
                                ctx.ellipse(root.toPxX(points[s].x) - root.scatterRadius,
                                            root.toPxY(points[s].y) - root.scatterRadius,
                                            2 * root.scatterRadius, 2 * root.scatterRadius)
                                ctx.fill()
                            }
                            continue
                        }

                        if (points.length < 2)
                            continue

                        const step = Math.max(1, Math.floor(points.length / (width * 2)))
                        ctx.strokeStyle = curve.color
                        ctx.beginPath()
                        ctx.moveTo(root.toPxX(points[0].x), root.toPxY(points[0].y))
                        for (let i = step; i < points.length; i += step)
                            ctx.lineTo(root.toPxX(points[i].x), root.toPxY(points[i].y))
                        ctx.stroke()
                    }
                }
            }

            Rectangle {
                id: verticalHair

                visible: cursor.tracking
                x: Math.round(cursor.mouseX)
                width: 1
                height: area.height
                color: Theme.plotCrosshair
            }

            Rectangle {
                id: horizontalHair

                visible: cursor.tracking
                y: Math.round(cursor.mouseY)
                width: area.width
                height: 1
                color: Theme.plotCrosshair
            }

            Rectangle {
                id: marker

                visible: cursor.tracking && cursor.point !== null

                readonly property real size: Math.round(Theme.unit * 0.6)

                x: (cursor.point ? root.toPxX(cursor.point.x) : 0) - size / 2
                y: (cursor.point ? root.toPxY(cursor.point.y) : 0) - size / 2
                width: size
                height: size
                radius: size / 2
                antialiasing: true

                color: cursor.point ? cursor.point.color : Theme.accent
                border.color: Theme.plotBackground
                border.width: 2
            }
        }

        Label {
            anchors.centerIn: parent
            visible: root.empty
            text: root.emptyText
            color: Theme.textCaption
            font.pixelSize: Theme.fontTitle
        }

        Rectangle {
            anchors.fill: parent
            color: "transparent"
            border.color: Theme.plotFrame
        }

        Rectangle {
            id: tooltip

            readonly property real gap: Theme.spaceSm

            visible: cursor.tracking && cursor.point !== null

            width: tooltipText.implicitWidth + 2 * Theme.fieldPadding
            height: tooltipText.implicitHeight + 2 * Theme.spaceXs
            radius: Theme.radius
            color: Theme.previewOverlay

            x: {
                const anchor = cursor.point ? root.toPxX(cursor.point.x) : 0
                return anchor + gap + width > area.width ? anchor - gap - width : anchor + gap
            }
            y: {
                const anchor = cursor.point ? root.toPxY(cursor.point.y) : 0
                return anchor - gap - height < 0 ? anchor + gap : anchor - gap - height
            }

            Label {
                id: tooltipText

                anchors.centerIn: parent
                horizontalAlignment: Text.AlignHCenter
                text: cursor.point ? qsTr("%1  %2\n%3  %4")
                                        .arg(root.xLabel).arg(cursor.point.x.toFixed(1))
                                        .arg(root.yLabel).arg(cursor.point.y.toFixed(1))
                                   : ""
                color: Theme.textOnPreview
                font.pixelSize: Theme.captionFontSize
                font.bold: true
            }
        }

        Rectangle {
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            anchors.margins: Theme.spaceXs

            visible: cursor.tracking
            width: readout.implicitWidth + 2 * Theme.fieldPadding
            height: readout.implicitHeight + 2 * Theme.spaceXs
            radius: Theme.radius
            color: Theme.previewOverlay

            Label {
                id: readout

                anchors.centerIn: parent
                text: qsTr("%1 %2   %3 %4")
                        .arg(root.xLabel).arg(Math.round(root.toDataX(cursor.mouseX)))
                        .arg(root.yLabel).arg(Math.round(root.toDataY(cursor.mouseY)))
                color: Theme.textOnPreview
                font.pixelSize: root.readoutFontSize
            }
        }

        MouseArea {
            id: cursor

            property bool panning: false
            property real lastX: 0
            property real lastY: 0
            property var point: null

            readonly property bool tracking: root.interactive && containsMouse && !panning

            anchors.fill: parent
            enabled: root.interactive
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton
            cursorShape: panning ? Qt.ClosedHandCursor : Qt.CrossCursor

            onPressed: (mouse) => {
                if (!root.canPan)
                    return
                panning = true
                lastX = mouse.x
                lastY = mouse.y
            }

            onReleased: panning = false

            onPositionChanged: (mouse) => {
                if (panning) {
                    root.panBy(mouse.x - lastX, mouse.y - lastY)
                    lastX = mouse.x
                    lastY = mouse.y
                    return
                }
                point = root.nearestPoint(mouse.x, mouse.y)
            }

            onExited: point = null

            onDoubleClicked: {
                panning = false
                root.resetView()
            }

            WheelHandler {
                enabled: root.interactive
                onWheel: (event) => root.zoomAt(event.x, event.y,
                                                event.angleDelta.y > 0 ? 0.83 : 1.2)
            }
        }
    }

    Label {
        anchors.horizontalCenter: area.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.spaceXs

        visible: root.axisTitles
        text: root.xLabel
        color: Theme.accent
        font.pixelSize: Theme.captionFontSize
        font.bold: true
    }

    Label {
        x: Theme.spaceXs - (width - height) / 2
        y: area.y + (area.height - height) / 2

        rotation: -90
        transformOrigin: Item.Center

        visible: root.axisTitles
        text: root.yLabel
        color: Theme.accent
        font.pixelSize: Theme.captionFontSize
        font.bold: true
    }
}
