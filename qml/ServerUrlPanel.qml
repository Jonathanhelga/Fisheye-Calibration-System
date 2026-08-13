import QtQuick
import QtQuick.Controls
import QtQuick.Layouts


Rectangle {
    id: root

    property string host: "192.168.103.56"
    property int axisPort: 8000
    property int monitorPort: 8001
    property int cameraPort: 8002


    function urlFor(port) { return "http://" + host + ":" + port + "/" }

    implicitWidth: 240
    implicitHeight: 175
    color: "#f4f6f8"
    border.color: "#b9c1c8"
    radius: 4

    ColumnLayout { 
        anchors.fill: parent
        anchors.margins: 6
        spacing: 5

        Label {
            text: "HTTP Server"
            font.bold: true
            color: "#2f6fbf"
        }

        Label {
            text: "Host URL"
            color: "#8a939c"
            font.pixelSize: 11
        }

        TextField {
            Layout.fillWidth: true
            color: "#2c3238"
            font.bold: true
            placeholderText: "192.168.103.56"
            text: root.host
            onTextEdited: root.host = text
            background: Rectangle {  
                implicitHeight: 30
                color: "white"
                border.color: parent.activeFocus ? "#2f6fbf" : "#b9c1c8"
                radius: 4
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 5

            LabeledField{
                Layout.fillWidth: true
                label: "Axis Port"
                text: root.axisPort
                validator: IntValidator { bottom: 1; top: 65535 }
                onEdited: root.axisPort = parseInt(value)
            }

            LabeledField{
                Layout.fillWidth: true
                label: "Monitor Port"
                text: root.monitorPort
                validator: IntValidator { bottom: 1; top: 65535 }
                onEdited: root.axisPort = parseInt(value)
            }

            LabeledField{
                Layout.fillWidth: true
                label: "Camera Port"
                text: root.cameraPort
                validator: IntValidator { bottom: 1; top: 65535 }
                onEdited: root.axisPort = parseInt(value)
            }
        }

        Button {
            id: submitUrl
            Layout.fillWidth: true
            Layout.topMargin: 4
            text: "Update"

            background: Rectangle {
                Layout.fillHeight: true
                radius: 4
                color: submitUrl.down ? "#2f6fbf"
                : (submitUrl.hovered ? "#5a93d4" : "#7fb3e6")
            }

            contentItem: Text {
                text: submitUrl.text
                color: "white"
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }
        Item { Layout.fillHeight: true }
    }
}
