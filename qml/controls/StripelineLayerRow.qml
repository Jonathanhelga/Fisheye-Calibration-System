pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FisheyeCaliJojo

RowLayout {
    id: row

    required property int layerNumber
    property real interval: 0
    property color color: "black"

    property int noWidth:     Math.round(Theme.charUnit * 2.5)
    property int heightWidth: Math.round(Theme.charUnit * 15)
    property int colorWidth:  Math.round(Theme.controlHeight * 0.65)
    readonly property int cellHeight:  Math.round(Theme.controlHeight * 0.72)

    signal intervalEdited(real interval)
    signal colorEdited(color value)
    signal moveFocusRequested(string column, int delta)

    function focusColumn(column) {
        if (column === "interval")
            intervalField.takeFocus()
    }

    spacing: Theme.rowSpacing

    Label {
        Layout.preferredWidth: row.noWidth
        Layout.fillWidth: true
        text: row.layerNumber
        color: Theme.textCaption
        font.pixelSize: Theme.captionFontSize
        horizontalAlignment: Text.AlignHCenter
    }

    Item {
        Layout.preferredWidth: row.heightWidth
        Layout.fillWidth: true
        Layout.preferredHeight: row.cellHeight

        ValueField {
            id: intervalField

            anchors.centerIn: parent
            width: row.heightWidth
            height: row.cellHeight
            editable: true
            horizontalAlignment: Text.AlignHCenter
            validator: IntValidator { bottom: 0; top: 9999 }
            value: row.interval
            onEdited: (value) => row.intervalEdited(parseInt(value))
            onMoveFocusRequested: (delta) => row.moveFocusRequested("interval", delta)
        }
    }

    Item {
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
