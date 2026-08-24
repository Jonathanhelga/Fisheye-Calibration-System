pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import FisheyeCaliJojo

Rectangle {
    id: control

    property var model: []
    property int currentIndex: 0
    property font font: Qt.font({ pixelSize: Theme.captionFontSize, bold: true })
    property bool stretch: false

    readonly property int inset: 2

    readonly property real measuredSegmentWidth: {
        let widest = 0
        for (let i = 0; i < measurer.count; i++)
            widest = Math.max(widest, measurer.objectAt(i).advanceWidth)
        return Math.ceil(widest) + 3 * Theme.fieldPadding
    }

    readonly property real segmentWidth: control.stretch
        ? (control.width - 2 * control.inset) / Math.max(1, control.model.length)
        : control.measuredSegmentWidth

    signal activated(int index)

    implicitWidth: 2 * inset + measuredSegmentWidth * model.length
    implicitHeight: Theme.controlHeight

    color: Theme.fieldBackground
    border.color: Theme.panelBorder
    radius: Theme.radius

    Instantiator {
        id: measurer
        model: control.model
        delegate: TextMetrics {
            required property string modelData
            font: control.font
            text: modelData
        }
    }

    Rectangle {
        id: highlight

        width: control.segmentWidth
        height: control.height - 2 * control.inset
        x: control.inset + control.currentIndex * control.segmentWidth
        y: control.inset
        radius: control.radius - 1
        color: Theme.accent

        Behavior on x {
            NumberAnimation { duration: Theme.animFast; easing.type: Easing.OutCubic }
        }
    }

    Row {
        x: control.inset
        y: control.inset

        Repeater {
            model: control.model

            AbstractButton {
                id: segment

                required property int index
                required property string modelData

                width: control.segmentWidth
                height: control.height - 2 * control.inset
                hoverEnabled: true

                readonly property bool selected: control.currentIndex === index

                onClicked: {
                    control.currentIndex = index
                    control.activated(index)
                }

                contentItem: Text {
                    text: segment.modelData
                    font: control.font
                    color: segment.selected ? Theme.textOnAccent
                         : (segment.hovered ? Theme.textPrimary : Theme.textCaption)
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter

                    Behavior on color {
                        ColorAnimation { duration: Theme.animFast }
                    }
                }
            }
        }
    }
}
