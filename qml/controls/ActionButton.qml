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

    leftPadding:   Theme.spaceMd
    rightPadding:  Theme.spaceMd
    topPadding:    Theme.spaceXs
    bottomPadding: Theme.spaceXs

    implicitHeight: Math.max(Theme.controlHeight,
                             implicitContentHeight + topPadding + bottomPadding)
    implicitWidth: Math.max(Math.round(Theme.charUnit * 10),
                            implicitContentWidth + leftPadding + rightPadding)

    background: Rectangle {
        radius: Theme.radius
        border.color: control.tone === "neutral" && !control.checked ? Theme.panelBorder
                                                                    : "transparent"
        color: !control.enabled ? Theme.fieldDisabledBackground
             : control.checked ? (control.down || control.hovered ? Theme.accentHover : Theme.accent)
             : control.down || control.hovered ? control.activeColor
                                               : control.baseColor

        Behavior on color {
            ColorAnimation { duration: Theme.animFast }
        }
    }

    contentItem: Text {
        text: control.text
        color: !control.enabled ? Theme.textDisabled
             : control.checked ? Theme.textOnAccent
                               : control.labelColor
        font.pixelSize: Theme.fontTitle
        font.bold: control.tone !== "neutral" || control.checked
        elide: Text.ElideRight
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
