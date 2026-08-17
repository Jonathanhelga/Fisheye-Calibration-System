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

    signal edited(string value)

    spacing: Theme.labelSpacing

    Label {
        id: caption
        color: Theme.textCaption
        font.pixelSize: Theme.captionFontSize
    }

    ValueField {
        id: box
        Layout.fillWidth: true
        onEdited: (value) => field.edited(value)
    }
}
