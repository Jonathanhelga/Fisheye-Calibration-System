import QtQuick
import QtQuick.Controls
import FisheyeCaliJojo

// Text-only button for low-emphasis actions in a card header, e.g. Import/Export.
AbstractButton {
    id: control

    hoverEnabled: true
    implicitWidth: label.implicitWidth
    implicitHeight: label.implicitHeight

    background: Item {}

    contentItem: Text {
        id: label
        text: control.text
        color: control.hovered ? Theme.accent : Theme.textCaption
        font.pixelSize: Theme.captionFontSize
        font.bold: true
        font.underline: true

        Behavior on color {
            ColorAnimation { duration: Theme.animFast }
        }
    }
}
