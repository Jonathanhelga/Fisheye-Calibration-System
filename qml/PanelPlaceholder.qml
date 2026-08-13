import QtQuick
import QtQuick.Controls
import QtQuick.Layouts


Rectangle {
    property alias title: heading.text

    // A bare Rectangle has an implicit size of 0x0, which makes a layout
    // collapse it to nothing when it only says Layout.fillWidth/fillHeight.
    // Give every placeholder a real intrinsic size to grow from.
    implicitWidth: 240
    implicitHeight: 120

    color: "#f4f6f8"
    border.color: "#c7cfd6"
    radius: 4

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8

        Label {
            id: heading
            font.bold: true
            color: "#2f6fbf"
        }
        Item { Layout.fillHeight: true }
    }
}
