import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FisheyeCaliJojo

RowLayout {
    property alias label: caption.text
    property alias value: readout.text

    spacing: Theme.labelSpacing

    Label {
        id: caption
        color: Theme.textCaption
        font.pixelSize: Theme.captionFontSize
    }

    Rectangle {
        Layout.fillWidth: true
        implicitWidth: 2 * Theme.fieldPadding
        implicitHeight: Theme.controlHeight
        color: Theme.fieldBackground
        border.color: Theme.panelBorder
        radius: Theme.radius

        Text {
            id: readout
            anchors.centerIn: parent
            color: Theme.textPrimary
            font.bold: true
        }
    }
}
