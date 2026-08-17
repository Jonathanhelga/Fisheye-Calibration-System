pragma Singleton

import QtQuick

QtObject {

    readonly property FontMetrics metrics: FontMetrics { font: Qt.application.font }
    readonly property real unit: Math.round(metrics.height)
    readonly property real charUnit: Math.round(metrics.averageCharacterWidth)
    //spacing
    readonly property int spaceXs: Math.round(unit * 0.25)    // 4
    readonly property int spaceSm: Math.round(unit * 0.5)     // 8
    readonly property int spaceMd: Math.round(unit * 0.75)    // 12
    readonly property int spaceLg: Math.round(unit)

    //control sizing
    readonly property int radius:        Math.round(unit * 0.25)
    readonly property int controlHeight: Math.round(unit * 2)
    readonly property int padButtonSize: Math.round(unit * 2.5)
    readonly property int readoutWidth:  Math.round(charUnit * 7)
    readonly property int fieldMinWidth: Math.round(charUnit * 5)

    // text
    readonly property int fontCaption: Math.round(unit * 0.7)
    readonly property int fontTitle:   Math.round(unit * 0.85)

    readonly property int minColumnLeft:        Math.round(unit * 32)
    readonly property int minColumnCenter:      Math.round(unit * 28)
    readonly property int minColumnRight:       Math.round(unit * 12)
    readonly property int minPanelHeight:       Math.round(unit * 35)
    readonly property int minHistogramHeight:   Math.round(unit * 7)

    readonly property real ratioLeft:   0.35
    readonly property real ratioCenter: 0.42
    readonly property real ratioRight:  0.23

    readonly property real ratioWorkRow:   0.60
    readonly property real ratioHistogram: 0.20


    readonly property int animFast: 120
    readonly property int animSlow: 450

    readonly property int panelGap: spaceMd
    readonly property int panelMargin: spaceSm
    readonly property int rowSpacing: spaceSm
    readonly property int labelSpacing: spaceXs
    readonly property int fieldPadding: spaceSm
    readonly property int captionFontSize: fontCaption

    function dpadMinWidth(columns) {
        return columns * padButtonSize + (columns - 1) * spaceXs + 2 * spaceSm
    }


    readonly property color panelBackground: "#f4f6f8"
    readonly property color panelBorder: "#b9c1c8"
    readonly property color fieldBackground: "#ffffff"
    readonly property color fieldDisabledBackground: "#e9edf1"

    readonly property color accent: "#2f6fbf"
    readonly property color accentHover: "#5a93d4"
    readonly property color accentIdle: "#7fb3e6"

    readonly property color danger: "#b3261e"
    readonly property color dangerHover: "#d1453b"
    readonly property color dangerIdle: "#e8817a"

    readonly property color statusOk: "#3aa76d"
    readonly property color statusFailed: "#c0483c"
    readonly property color statusPartial: "#e0a02a"
    readonly property color statusUnknown: "#c6ced5"

    readonly property color textPrimary: "#2c3238"
    readonly property color textCaption: "#8a939c"
    readonly property color textOnAccent: "#ffffff"
    readonly property color textDisabled: "#b6bec6"
}
