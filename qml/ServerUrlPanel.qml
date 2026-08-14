import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FisheyeCaliJojo


Rectangle {
    id: root

    property string host: "192.168.103.56"
    property int axisPort: 8000
    property int monitorPort: 8001
    property int cameraPort: 8002

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

        LabeledField {
            Layout.fillWidth: true
            label: "Host URL"
            placeholderText: "192.168.103.56"
            text: root.host
            showStatus: true
            status: ServerProbe.hostStatus
            onEdited: (value) => {
                root.host = value
                ServerProbe.resetAll()
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
                showStatus: true
                status: ServerProbe.axisStatus
                onEdited: (value) => {
                    root.axisPort = parseInt(value)
                    ServerProbe.markUnknown(ServerProbe.Axis)
                }
            }

            LabeledField{
                Layout.fillWidth: true
                label: "Monitor Port"
                text: root.monitorPort
                validator: IntValidator { bottom: 1; top: 65535 }
                showStatus: true
                status: ServerProbe.monitorStatus
                onEdited: (value) => {
                    root.monitorPort = parseInt(value)
                    ServerProbe.markUnknown(ServerProbe.Monitor)
                }
            }

            LabeledField{
                Layout.fillWidth: true
                label: "Camera Port"
                text: root.cameraPort
                validator: IntValidator { bottom: 1; top: 65535 }
                showStatus: true
                status: ServerProbe.cameraStatus
                onEdited: (value) => {
                    root.cameraPort = parseInt(value)
                    ServerProbe.markUnknown(ServerProbe.Camera)
                }
            }
        }

        Button {
            id: submitUrl
            Layout.fillWidth: true
            Layout.topMargin: 4
            text: "Update"

            // Never disabled while probing: probeAll() supersedes its own
            // in-flight requests, so a double press is already safe.
            onClicked: ServerProbe.probeAll(root.host, root.axisPort,
                                            root.monitorPort, root.cameraPort)

            background: Rectangle {
                implicitHeight: 30
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
