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

    spacing: 5

    RowLayout {
        Layout.fillWidth: true
        spacing: 5

        Label {
            id: caption
            color: "#8a939c"
            font.pixelSize: 11
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
        color: "#2c3238"
        font.bold: true
        onTextEdited: if (acceptableInput) field.edited(text)

        background: Rectangle {
            implicitHeight: 30
            color: "white"
            border.color: input.activeFocus ? "#2f6fbf" : "#b9c1c8"
            radius: 4
        }
    }
}
