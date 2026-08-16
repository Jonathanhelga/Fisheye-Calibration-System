import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FisheyeCaliJojo

ColumnLayout {
    id: field

    property alias label: caption.text
    property alias text: input.text
    property alias placeholderText: input.placeholderText
    property alias validator: input.validator

    property bool showStatus: false
    property int status: ServerProbe.Unknown

    signal edited(string value)

    spacing: Theme.labelSpacing

    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.labelSpacing

        Label {
            id: caption
            color: field.enabled ? Theme.textCaption : Theme.textDisabled
            font.pixelSize: Theme.captionFontSize
        }

        StatusDot {
            visible: field.showStatus
            status: field.status
        }
        Item { Layout.fillWidth: true }
    }

    TextField {
        id: input
        Layout.fillWidth: true
        Layout.minimumWidth: Theme.fieldMinWidth
        color: input.enabled ? Theme.textPrimary : Theme.textDisabled
        font.bold: true
        onTextEdited: if (acceptableInput) field.edited(text)

        background: Rectangle {
            implicitHeight: Theme.controlHeight
            color: input.enabled ? Theme.fieldBackground : Theme.fieldDisabledBackground
            border.color: input.activeFocus ? Theme.accent : Theme.panelBorder
            radius: Theme.radius

            Behavior on color {
                ColorAnimation { duration: Theme.animFast }
            }
        }
    }
}
