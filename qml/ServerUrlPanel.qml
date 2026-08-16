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

    implicitHeight: content.implicitHeight + 2 * Theme.panelMargin

    color: Theme.panelBackground
    border.color: Theme.panelBorder
    radius: Theme.radius

    ColumnLayout {
        id: content

        anchors.fill: parent
        anchors.margins: Theme.panelMargin
        spacing: Theme.rowSpacing

        Label {
            text: "HTTP Server"
            font.bold: true
            color: Theme.accent
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
            spacing: Theme.rowSpacing

            LabeledField {
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

            LabeledField {
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

            LabeledField {
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
            text: "Update"

            onClicked: ServerProbe.probeAll(root.host, root.axisPort,
                                            root.monitorPort, root.cameraPort)

            background: Rectangle {
                implicitHeight: Theme.controlHeight
                radius: Theme.radius
                color: submitUrl.down ? Theme.accent
                     : (submitUrl.hovered ? Theme.accentHover : Theme.accentIdle)
            }

            contentItem: Text {
                text: submitUrl.text
                color: Theme.textOnAccent
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }
    }
}
