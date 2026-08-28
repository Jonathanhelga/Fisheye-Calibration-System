pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FisheyeCaliJojo

ColumnLayout {
    id: field

    property AxisState axis: null
    property string label: ""

    readonly property string value: axis ? axis.coordinate : "?"
    readonly property bool provisional: axis ? !axis.hasZero : true
    readonly property string caption: axis && axis.unit.length > 0
        ? label + " (" + axis.unit + ")"
        : label

    spacing: Theme.labelSpacing

    component Lamp: Rectangle {
        required property int tri

        implicitWidth: Math.round(Theme.unit * 0.35)
        implicitHeight: implicitWidth
        radius: width / 2

        color: tri === 1 ? Theme.statusOk
             : tri === 0 ? Theme.statusFailed
                         : Theme.statusUnknown

        Behavior on color {
            ColorAnimation { duration: Theme.animFast }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.labelSpacing

        Label {
            text: field.caption
            color: field.provisional ? Theme.statusPartial : Theme.textCaption
            font.pixelSize: Theme.captionFontSize
        }

        Item { Layout.fillWidth: true }

        Lamp { tri: field.axis ? field.axis.sensorLow : -1 }
        Lamp { tri: field.axis ? field.axis.sensorOrg : -1 }
        Lamp { tri: field.axis ? field.axis.sensorHigh : -1 }

        Lamp {
            id: moveLamp

            tri: field.axis ? field.axis.sensorMoving : -1
            color: tri === 1 ? Theme.statusOk
                 : tri === 0 ? Theme.statusPartial
                             : Theme.statusUnknown

            SequentialAnimation on opacity {
                running: field.axis ? field.axis.moving : false
                loops: Animation.Infinite
                alwaysRunToEnd: true
                NumberAnimation { to: 0.3; duration: Theme.animSlow }
                NumberAnimation { to: 1.0; duration: Theme.animSlow }
            }
        }
    }

    ValueField {
        Layout.fillWidth: true
        value: field.value
        editable: false
        horizontalAlignment: Text.AlignHCenter
    }
}
