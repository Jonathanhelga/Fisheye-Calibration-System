import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FisheyeCaliJojo

RowLayout {
    id: field

    property alias label: caption.text
    property alias from: spin.from
    property alias to: spin.to
    property alias stepSize: spin.stepSize
    property alias value: spin.value
    property alias editable: spin.editable

    readonly property alias hovered: hoverHandler.hovered

    signal edited(int value)

    spacing: Theme.labelSpacing

    HoverHandler {
        id: hoverHandler
    }

    Label {
        id: caption
        color: field.enabled ? Theme.textCaption : Theme.textDisabled
        font.pixelSize: Theme.captionFontSize
    }

    SpinBox {
        id: spin

        readonly property int inset: 2
        readonly property int stepWidth: Theme.controlHeight

        from: 0
        to: 100
        stepSize: 1
        editable: true

        implicitHeight: Theme.controlHeight
        implicitWidth: 2 * (inset + stepWidth)
                       + Math.ceil(widest.advanceWidth)
                       + 2 * Theme.fieldPadding

        onValueModified: field.edited(spin.value)

        TextMetrics {
            id: widest
            font: valueText.font
            text: String(spin.to)
        }

        validator: IntValidator {
            bottom: spin.from
            top: spin.to
        }

        background: Rectangle {
            color: spin.enabled ? Theme.fieldBackground : Theme.fieldDisabledBackground
            border.color: valueText.activeFocus ? Theme.accent : Theme.panelBorder
            radius: Theme.radius

            Behavior on color {
                ColorAnimation { duration: Theme.animFast }
            }
        }

        contentItem: TextInput {
            id: valueText

            text: spin.textFromValue(spin.value, spin.locale)
            font.bold: true
            color: spin.enabled ? Theme.textPrimary : Theme.textDisabled

            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter

            readOnly: !spin.editable || !spin.enabled
            validator: spin.validator
            selectByMouse: spin.editable
        }

        down.indicator: Rectangle {
            x: spin.inset
            y: spin.inset
            width: spin.stepWidth
            height: spin.height - 2 * spin.inset
            radius: Theme.radius - 1

            color: spin.down.pressed ? Theme.accent
                 : spin.down.hovered ? Theme.fieldDisabledBackground
                                     : "transparent"

            Behavior on color {
                ColorAnimation { duration: Theme.animFast }
            }

            Text {
                anchors.centerIn: parent
                text: "−"
                font.bold: true
                color: spin.down.pressed ? Theme.textOnAccent
                     : spin.value > spin.from ? Theme.accent
                                              : Theme.textDisabled
            }
        }

        up.indicator: Rectangle {
            x: spin.width - width - spin.inset
            y: spin.inset
            width: spin.stepWidth
            height: spin.height - 2 * spin.inset
            radius: Theme.radius - 1

            color: spin.up.pressed ? Theme.accent
                 : spin.up.hovered ? Theme.fieldDisabledBackground
                                   : "transparent"

            Behavior on color {
                ColorAnimation { duration: Theme.animFast }
            }

            Text {
                anchors.centerIn: parent
                text: "+"
                font.bold: true
                color: spin.up.pressed ? Theme.textOnAccent
                     : spin.value < spin.to ? Theme.accent
                                            : Theme.textDisabled
            }
        }
    }
}
