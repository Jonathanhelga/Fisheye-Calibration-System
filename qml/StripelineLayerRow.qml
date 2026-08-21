pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FisheyeCaliJojo

// One row of the stripline table: a stripe's height and color. The value is
// labeled "Height" in the UI, but named "interval" internally to match the
// reference app, since Item already has a builtin "height" property.
// Column widths are passed in from JojoStriplinePanel so the header and
// every row always share the same values.
RowLayout {
    id: row

    required property int layerNumber
    property real interval: 0
    property color color: "black"

    property int noWidth:     Math.round(Theme.charUnit * 2.5)
    property int heightWidth: Math.round(Theme.charUnit * 6)
    property int colorWidth:  Math.round(Theme.controlHeight * 0.65)
    readonly property int cellHeight:  Math.round(Theme.controlHeight * 0.72)

    signal intervalEdited(real interval)
    signal colorEdited(color value)

    spacing: Theme.rowSpacing

    Label {
        Layout.preferredWidth: row.noWidth
        Layout.fillWidth: true
        text: row.layerNumber
        color: Theme.textCaption
        font.pixelSize: Theme.captionFontSize
        horizontalAlignment: Text.AlignHCenter
    }

    ValueField {
        Layout.preferredWidth: row.heightWidth
        Layout.fillWidth: true
        Layout.preferredHeight: row.cellHeight
        editable: true
        horizontalAlignment: Text.AlignHCenter
        validator: IntValidator { bottom: 0; top: 9999 }
        value: row.interval
        onEdited: (value) => row.intervalEdited(parseInt(value))
    }

    Item {
        // Stretches with the column like every other cell, but keeps the
        // swatch itself a fixed square instead of letting it turn into a
        // rectangle when the window is wider than the minimum.
        Layout.preferredWidth: row.colorWidth
        Layout.fillWidth: true
        Layout.preferredHeight: row.cellHeight

        PatternColorButton {
            anchors.centerIn: parent
            width: row.colorWidth
            height: row.colorWidth
            value: row.color
            onPicked: (value) => row.colorEdited(value)
        }
    }
}
