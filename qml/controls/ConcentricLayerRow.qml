pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FisheyeCaliJojo

// One row of the concentric table: a ring/layer's shape, radius step, color,
// and center offset. Column widths are passed in from ConcentricPanel so
// the header and every row always share the same values.
RowLayout {
    id: row

    required property int layerNumber
    property string shape: "Circle"
    property real radius: 0
    property color color: "black"
    property real cx: 0
    property real cy: 0

    property int noWidth:     Math.round(Theme.charUnit * 2.5)
    property int shapeWidth:  Math.round(Theme.charUnit * 9)
    property int radiusWidth: Math.round(Theme.charUnit * 6)
    property int colorWidth:  Math.round(Theme.controlHeight * 0.65)
    property int cxWidth:     Math.round(Theme.charUnit * 5)
    property int cyWidth:     Math.round(Theme.charUnit * 5)
    readonly property int cellHeight:  Math.round(Theme.controlHeight * 0.72)

    signal shapeEdited(string shape)
    signal radiusEdited(real radius)
    signal colorEdited(color value)
    signal cxEdited(real cx)
    signal cyEdited(real cy)

    spacing: Theme.rowSpacing

    Label {
        Layout.preferredWidth: row.noWidth
        Layout.fillWidth: true
        text: row.layerNumber
        color: Theme.textCaption
        font.pixelSize: Theme.captionFontSize
        horizontalAlignment: Text.AlignHCenter
    }

    ComboBox {
        Layout.preferredWidth: row.shapeWidth
        Layout.fillWidth: true
        Layout.preferredHeight: row.cellHeight
        font.pixelSize: Theme.captionFontSize
        model: ["Circle", "Square"]
        currentIndex: row.shape === "Square" ? 1 : 0
        onActivated: (index) => row.shapeEdited(index === 1 ? "Square" : "Circle")
    }

    ValueField {
        Layout.preferredWidth: row.radiusWidth
        Layout.fillWidth: true
        Layout.preferredHeight: row.cellHeight
        editable: true
        horizontalAlignment: Text.AlignHCenter
        validator: IntValidator { bottom: 0; top: 9999 }
        value: row.radius
        onEdited: (value) => row.radiusEdited(parseInt(value))
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

    ValueField {
        Layout.preferredWidth: row.cxWidth
        Layout.fillWidth: true
        Layout.preferredHeight: row.cellHeight
        editable: true
        horizontalAlignment: Text.AlignHCenter
        validator: IntValidator { bottom: -9999; top: 9999 }
        value: row.cx
        onEdited: (value) => row.cxEdited(parseInt(value))
    }

    ValueField {
        Layout.preferredWidth: row.cyWidth
        Layout.fillWidth: true
        Layout.preferredHeight: row.cellHeight
        editable: true
        horizontalAlignment: Text.AlignHCenter
        validator: IntValidator { bottom: -9999; top: 9999 }
        value: row.cy
        onEdited: (value) => row.cyEdited(parseInt(value))
    }
}
