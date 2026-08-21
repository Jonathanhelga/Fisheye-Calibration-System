pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FisheyeCaliJojo

RowLayout {
    id: control

    property real from: 0
    property real to: 2000
    property real lowerValue: from
    property real upperValue: to
    property real minRange: (to - from) * 0.02
    property bool compact: false

    signal lowerMoved(real value)
    signal upperMoved(real value)

    function clamp(v, lo, hi) { return Math.max(lo, Math.min(hi, v)) }

    spacing: Theme.labelSpacing

    Label {
        visible: !control.compact
        text: qsTr("IH Range")
        color: Theme.textCaption
        font.pixelSize: Theme.captionFontSize
    }

    ValueField {
        implicitWidth: Theme.readoutWidth
        horizontalAlignment: Text.AlignRight
        value: Math.round(control.lowerValue)
    }

    Item {
        id: track

        Layout.fillWidth: true
        Layout.minimumWidth: control.compact ? Theme.unit * 6 : Theme.unit * 10
        implicitHeight: handleSize

        readonly property real handleSize: Theme.spaceLg
        readonly property real span: (control.to - control.from) !== 0 ? control.to - control.from : 1

        function valueToPx(v) {
            const clamped = control.clamp(v, control.from, control.to)
            return handleSize / 2 + (clamped - control.from) / span * (width - handleSize)
        }
        function pxToValue(px) {
            return control.from + (px - handleSize / 2) / (width - handleSize) * span
        }

        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            x: track.handleSize / 2
            width: track.width - track.handleSize
            height: Theme.spaceXs
            radius: height / 2
            color: Theme.fieldDisabledBackground
            border.color: Theme.panelBorder
        }

        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            x: track.valueToPx(control.lowerValue)
            width: Math.max(0, track.valueToPx(control.upperValue) - x)
            height: Theme.spaceXs
            radius: height / 2
            color: Theme.accent
        }

        Rectangle {
            id: lowerHandle

            width: track.handleSize
            height: track.handleSize
            radius: width / 2
            antialiasing: true
            x: track.valueToPx(control.lowerValue) - width / 2
            anchors.verticalCenter: parent.verticalCenter
            color: lowerDrag.pressed ? Theme.accentHover : Theme.accent
            border.color: Theme.plotBackground
            border.width: 2

            MouseArea {
                id: lowerDrag

                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor

                onPositionChanged: (mouse) => {
                    const px = lowerHandle.x + mouse.x
                    const value = control.clamp(track.pxToValue(px),
                                                 control.from,
                                                 control.upperValue - control.minRange)
                    control.lowerValue = value
                    control.lowerMoved(value)
                }
            }
        }

        Rectangle {
            id: upperHandle

            width: track.handleSize
            height: track.handleSize
            radius: width / 2
            antialiasing: true
            x: track.valueToPx(control.upperValue) - width / 2
            anchors.verticalCenter: parent.verticalCenter
            color: upperDrag.pressed ? Theme.accentHover : Theme.accent
            border.color: Theme.plotBackground
            border.width: 2

            MouseArea {
                id: upperDrag

                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor

                onPositionChanged: (mouse) => {
                    const px = upperHandle.x + mouse.x
                    const value = control.clamp(track.pxToValue(px),
                                                 control.lowerValue + control.minRange,
                                                 control.to)
                    control.upperValue = value
                    control.upperMoved(value)
                }
            }
        }
    }

    ValueField {
        implicitWidth: Theme.readoutWidth
        horizontalAlignment: Text.AlignRight
        value: Math.round(control.upperValue)
    }
}
