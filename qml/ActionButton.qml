import QtQuick
import QtQuick.Controls
import FisheyeCaliJojo

Button {
    id: control

    property string tone: "neutral"

    readonly property color baseColor: tone === "accent" ? Theme.accent
                                     : tone === "danger" ? Theme.danger
                                                         : Theme.fieldBackground
    readonly property color activeColor: tone === "accent" ? Theme.accentHover
                                       : tone === "danger" ? Theme.dangerHover
                                                           : Theme.fieldDisabledBackground
    readonly property color labelColor: tone === "neutral" ? Theme.textPrimary
                                                           : Theme.textOnAccent

    implicitHeight: Theme.controlHeight
    implicitWidth: Math.round(Theme.charUnit * 10)

    background: Rectangle {
        radius: Theme.radius
        border.color: control.tone === "neutral" ? Theme.panelBorder : "transparent"
        color: !control.enabled ? Theme.fieldDisabledBackground
             : control.down || control.hovered ? control.activeColor
                                               : control.baseColor

        Behavior on color {
            ColorAnimation { duration: Theme.animFast }
        }
    }

    contentItem: Text {
        text: control.text
        color: control.enabled ? control.labelColor : Theme.textDisabled
        font.pixelSize: Theme.fontTitle
        font.bold: control.tone !== "neutral"
        elide: Text.ElideRight
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
