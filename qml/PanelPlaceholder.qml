import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FisheyeCaliJojo

Rectangle {
    property alias title: heading.text

    implicitWidth: Theme.unit * 16
    implicitHeight: Theme.unit * 8

    color: Theme.panelBackground
    border.color: Theme.panelBorder
    radius: Theme.radius

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.panelMargin

        Label {
            id: heading
            font.bold: true
            font.pixelSize: Theme.fontTitle
            color: Theme.accent
        }
        Item { Layout.fillHeight: true }
    }
}
