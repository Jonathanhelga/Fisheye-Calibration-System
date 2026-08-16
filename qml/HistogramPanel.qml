import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FisheyeCaliJojo

Rectangle {
    id: root
    property int channel: 1

    implicitWidth: Theme.unit * 32
    implicitHeight: Theme.minHistogramHeight

    color: Theme.panelBackground
    border.color: Theme.panelBorder
    radius: Theme.radius

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.panelMargin
        spacing: Theme.rowSpacing

        Label {
            text: "Histogram" + root.channel
            font.bold: true
            font.pixelSize: Theme.fontTitle
            color: Theme.accent
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.rowSpacing

            ColumnLayout {
                Layout.fillWidth: false
                Layout.minimumWidth: Theme.unit * 9
                Layout.alignment: Qt.AlignTop
                spacing: Theme.rowSpacing

                Button { text: "Show Curve";  Layout.fillWidth: true }
                Button { text: "Curve Color"; Layout.fillWidth: true }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: Theme.unit * 12
                Layout.minimumHeight: Theme.unit * 5
                color: Theme.fieldBackground
                border.color: Theme.panelBorder
                radius: Theme.radius

                Label {
                    anchors.centerIn: parent
                    text: "plot goes here"
                    color: Theme.textCaption
                }
            }
        }
    }
}
