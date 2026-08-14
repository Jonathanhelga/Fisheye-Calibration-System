pragma Singleton

import QtQuick

QtObject {
    readonly property int panelGap: 12
    readonly property int panelMargin: 8
    readonly property int rowSpacing: 8
    readonly property int labelSpacing: 4

    readonly property int radius: 4
    readonly property int controlHeight: 30
    readonly property int fieldPadding: 8
    readonly property int readoutWidth: 44
    readonly property int inlineLabelWidth: 52
    readonly property int padButtonSize: 38
    readonly property int captionFontSize: 11

    readonly property color panelBackground: "#f4f6f8"
    readonly property color panelBorder: "#b9c1c8"
    readonly property color fieldBackground: "#ffffff"

    readonly property color accent: "#2f6fbf"
    readonly property color accentHover: "#5a93d4"
    readonly property color accentIdle: "#7fb3e6"

    readonly property color textPrimary: "#2c3238"
    readonly property color textCaption: "#8a939c"
    readonly property color textOnAccent: "#ffffff"
}
