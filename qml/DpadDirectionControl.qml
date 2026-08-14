import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FisheyeCaliJojo

GridLayout {
    id: pad

    property bool horizontalEnabled: true
    property string centerText: ""

    signal directionClicked(string direction)

    component PadButton: Button {
        id: control

        Layout.preferredWidth: Theme.padButtonSize
        Layout.preferredHeight: Theme.padButtonSize

        background: Rectangle {
            radius: Theme.radius
            color: control.down ? Theme.accent
                 : (control.hovered ? Theme.accentHover : Theme.accentIdle)
        }

        contentItem: Text {
            text: control.text
            color: Theme.textOnAccent
            font.pixelSize: 15
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    columns: horizontalEnabled ? 3 : 1
    rowSpacing: Theme.labelSpacing
    columnSpacing: Theme.labelSpacing

    Item {
        visible: pad.horizontalEnabled
        Layout.preferredWidth: Theme.padButtonSize
        Layout.preferredHeight: Theme.padButtonSize
    }

    PadButton {
        text: "▲"
        onClicked: pad.directionClicked("up")
    }

    Item {
        visible: pad.horizontalEnabled
        Layout.preferredWidth: Theme.padButtonSize
        Layout.preferredHeight: Theme.padButtonSize
    }

    PadButton {
        visible: pad.horizontalEnabled
        text: "◀"
        onClicked: pad.directionClicked("left")
    }

    Label {
        Layout.preferredWidth: Theme.padButtonSize
        Layout.preferredHeight: Theme.padButtonSize
        text: pad.centerText
        color: Theme.textCaption
        font.pixelSize: Theme.captionFontSize
        font.bold: true
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    PadButton {
        visible: pad.horizontalEnabled
        text: "▶"
        onClicked: pad.directionClicked("right")
    }

    Item {
        visible: pad.horizontalEnabled
        Layout.preferredWidth: Theme.padButtonSize
        Layout.preferredHeight: Theme.padButtonSize
    }

    PadButton {
        text: "▼"
        onClicked: pad.directionClicked("down")
    }

    Item {
        visible: pad.horizontalEnabled
        Layout.preferredWidth: Theme.padButtonSize
        Layout.preferredHeight: Theme.padButtonSize
    }
}
