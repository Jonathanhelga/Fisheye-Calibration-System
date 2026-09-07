import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FisheyeCaliJojo

RowLayout {
    id: field

    property alias label: caption.text
    property alias value: box.value
    property alias validator: box.validator
    property alias editable: box.editable
    property real fieldWidth: -1

    signal edited(string value)

    spacing: Theme.labelSpacing

    Label {
        id: caption
        color: Theme.textCaption
        font.pixelSize: Theme.captionFontSize
    }

    ValueField {
        id: box
        horizontalAlignment: Text.AlignHCenter
        Layout.fillWidth: field.fieldWidth < 0
        Layout.preferredWidth: field.fieldWidth < 0 ? implicitWidth : field.fieldWidth
        onEdited: (value) => field.edited(value)
    }
}
