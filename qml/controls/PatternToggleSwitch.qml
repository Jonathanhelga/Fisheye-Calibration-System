import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FisheyeCaliJojo

// Small pill switch with a trailing label, e.g. the CrossLine toggle.
RowLayout {
    id: control

    property bool checked: false
    property alias text: caption.text

    signal toggled(bool checked)


    spacing: Theme.spaceXs
    Label {
        id: caption
        color: Theme.textCaption
        font.pixelSize: Theme.captionFontSize
    }

    AbstractButton {
        id: pill

        implicitWidth: Math.round(Theme.controlHeight * 1.5)
        implicitHeight: Math.round(Theme.controlHeight * 0.65)
        hoverEnabled: true

        onClicked: {
            control.checked = !control.checked
            control.toggled(control.checked)
        }

        background: Rectangle {
            radius: height / 2
            color: control.checked ? Theme.accent : Theme.fieldDisabledBackground
            border.color: control.checked ? Theme.accent : Theme.panelBorder

            Behavior on color {
                ColorAnimation { duration: Theme.animFast }
            }

            Rectangle {
                width: parent.height - 4
                height: width
                y: 2
                x: control.checked ? parent.width - width - 2 : 2
                radius: width / 2
                color: "#ffffff"
                border.color: Theme.panelBorder
                antialiasing: true

                Behavior on x {
                    NumberAnimation { duration: Theme.animFast; easing.type: Easing.OutCubic }
                }
            }
        }
    }
}
