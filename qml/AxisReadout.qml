import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FisheyeCaliJojo

RowLayout {
    id: field

    property alias label: caption.text
    property alias value: readout.text
    property alias validator: readout.validator
    property bool editable: false

    signal edited(string value)

    spacing: Theme.labelSpacing

    Label {
        id: caption
        color: Theme.textCaption
        font.pixelSize: Theme.captionFontSize
    }

    Rectangle {
        Layout.fillWidth: true
        implicitWidth: readout.implicitWidth + 2 * Theme.fieldPadding
        implicitHeight: Theme.controlHeight
        color: Theme.fieldBackground
        border.color: readout.activeFocus ? Theme.accent : Theme.panelBorder
        radius: Theme.radius

        TextInput {
            id: readout
            anchors.centerIn: parent
            color: Theme.textPrimary
            font.bold: true
            readOnly: !field.editable
            activeFocusOnPress: field.editable
            selectByMouse: field.editable
            onTextEdited: if (acceptableInput) field.edited(text)
        }
    }
}
