pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FisheyeCaliJojo

ColumnLayout {
    id: block

    property string title: ""
    property var legend: []
    property string xLabel: ""
    property string yLabel: ""
    property real xMin: 0
    property real xMax: 1
    property real yMin: 0
    property real yMax: 1
    property var curves: []
    property var regions: []
    property string emptyText: qsTr("No rounds loaded")
    property string actionText: ""
    property string readoutXLabel: ""
    property string readoutYLabel: ""
    property int readoutFontSize: Theme.captionFontSize

    signal actionTriggered()

    spacing: Theme.labelSpacing

    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.spaceSm

        Label {
            text: block.title
            color: Theme.textPrimary
            font.bold: true
            font.pixelSize: Theme.fontTitle
        }

        Item { Layout.fillWidth: true }

        Repeater {
            model: block.legend

            RowLayout {
                id: legendEntry

                required property var modelData

                spacing: Theme.labelSpacing

                ColorSwatch {
                    Layout.alignment: Qt.AlignVCenter
                    implicitWidth: Math.round(Theme.unit * 0.5)
                    implicitHeight: Math.round(Theme.unit * 0.5)
                    radius: width / 2
                    color: legendEntry.modelData.color
                }

                Label {
                    text: legendEntry.modelData.label
                    color: Theme.textCaption
                    font.pixelSize: Theme.captionFontSize
                }
            }
        }
    }

    HistogramPlotView {
        id: plot

        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: Theme.minHistogramHeight

        xLabel: block.xLabel
        yLabel: block.yLabel
        emptyText: block.emptyText
        readoutFontSize: block.readoutFontSize

        defaultXMin: block.xMin
        defaultXMax: block.xMax
        defaultYMin: block.yMin
        defaultYMax: block.yMax

        curves: block.curves
        regions: block.regions
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.spaceSm

        ActionButton {
            visible: block.actionText.length > 0
            text: block.actionText
            onClicked: block.actionTriggered()
        }

        Item { Layout.fillWidth: true }

        AxisReadout {
            Layout.preferredWidth: Math.round(Theme.charUnit * 16)
            label: block.readoutXLabel
            editable: false
            value: plot.cursorInside ? plot.cursorX.toFixed(2) : ""
        }

        AxisReadout {
            Layout.preferredWidth: Math.round(Theme.charUnit * 16)
            label: block.readoutYLabel
            editable: false
            value: plot.cursorInside ? plot.cursorY.toFixed(2) : ""
        }
    }
}
