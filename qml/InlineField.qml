import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FisheyeCaliJojo

RowLayout {
    id: field

    property alias label: caption.text
    property alias text: input.text
    property alias placeholderText: input.placeholderText
    property alias validator: input.validator
    property int labelWidth: Theme.inlineLabelWidth

    signal edited(string value)

    spacing: Theme.labelSpacing

    Label {
        id: caption
        Layout.preferredWidth: field.labelWidth
        color: Theme.textCaption
        font.pixelSize: Theme.captionFontSize
    }

    TextField {
        id: input
        Layout.fillWidth: true
        color: Theme.textPrimary
        font.bold: true
        horizontalAlignment: TextInput.AlignRight
        onTextEdited: if (acceptableInput) field.edited(text)

        background: Rectangle {
            implicitHeight: Theme.controlHeight
            color: Theme.fieldBackground
            border.color: input.activeFocus ? Theme.accent : Theme.panelBorder
            radius: Theme.radius
        }
    }
}
