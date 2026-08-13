import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: field

    property alias label: caption.text
    property alias text: input.text
    property alias placeholderText: input.placeholderText
    property alias validator: input.validator

    signal edited(string value)

    spacing: 5
    
    Label{
        id: caption
        color: "#8a939c"
        font.pixelSize: 11
    }
    TextField{
        id: input
        Layout.fillWidth: true
        color: "#2c3238"
        font.bold: true
        onTextEdited: if (acceptableInput) field.edited(text)

        background: Rectangle {
            color: "white"
            border.color: input.activeFocus ? "#2f6fbf" : "#b9c1c8"
            radius: 4
        }
    }
}