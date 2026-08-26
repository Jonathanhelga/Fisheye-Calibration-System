import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FisheyeCaliJojo


Rectangle {
    id: root

    readonly property int modeRos: 0
    readonly property int modeHttp: 1
    property alias mode: modeSelector.currentIndex

    property string host: "192.168.103.56"
    property int axisPort: 8000
    property int monitorPort: 8001
    property int cameraPort: 8002

    property int domainId: 42
    property string axisNamespace: "/axis"
    property string monitorNamespace: "/monitor"
    property string cameraTopic: "/camera/image_raw/compressed"

    readonly property int rosAxisStatus: RosServerProbe.axisStatus
    readonly property int rosMonitorStatus: RosServerProbe.monitorStatus
    readonly property int rosCameraStatus: RosServerProbe.cameraStatus

    readonly property int rosDomainStatus: {
        const statuses = [rosAxisStatus, rosMonitorStatus, rosCameraStatus]
        if (statuses.some(s => s === ServerProbe.Checking)) return ServerProbe.Checking
        if (statuses.some(s => s === ServerProbe.Unknown)) return ServerProbe.Unknown
        const okCount = statuses.filter(s => s === ServerProbe.Ok).length
        if (okCount === statuses.length) return ServerProbe.Ok
        return okCount === 0 ? ServerProbe.Failed : ServerProbe.Partial
    }

    signal rosUpdateRequested(int domainId, string axisNamespace, string monitorNamespace, string cameraTopic)

    implicitHeight: content.implicitHeight + 2 * Theme.panelMargin

    color: Theme.panelBackground
    border.color: Theme.panelBorder
    radius: Theme.radius

    ColumnLayout {
        id: content

        anchors.fill: parent
        anchors.margins: Theme.panelMargin
        spacing: Theme.rowSpacing

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.rowSpacing

            Label {
                text: "Server"
                font.bold: true
                color: Theme.accent
            }

            Item { Layout.fillWidth: true }

            SegmentedControl {
                id: modeSelector
                model: [qsTr("ROS"), qsTr("HTTP")]
                currentIndex: root.modeRos
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.rowSpacing
            visible: root.mode === root.modeRos

            LabeledField {
                Layout.fillWidth: true
                label: "ROS Domain ID"
                placeholderText: "42"
                text: root.domainId
                validator: IntValidator { bottom: 0; top: 232 }
                showStatus: true
                status: root.rosDomainStatus
                onEdited: (value) => {
                    root.domainId = parseInt(value)
                    RosServerProbe.resetAll()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.rowSpacing

                LabeledField {
                    Layout.fillWidth: true
                    label: "Axis Namespace"
                    placeholderText: "/axis"
                    text: root.axisNamespace
                    showStatus: true
                    status: root.rosAxisStatus
                    onEdited: (value) => {
                        root.axisNamespace = value
                        RosServerProbe.resetAll()
                    }
                }

                LabeledField {
                    Layout.fillWidth: true
                    label: "Monitor Namespace"
                    placeholderText: "/monitor"
                    text: root.monitorNamespace
                    showStatus: true
                    status: root.rosMonitorStatus
                    onEdited: (value) => {
                        root.monitorNamespace = value
                        RosServerProbe.resetAll()
                    }
                }

                LabeledField {
                    Layout.fillWidth: true
                    label: "Camera Topic"
                    placeholderText: "/camera/image_raw/compressed"
                    text: root.cameraTopic
                    showStatus: true
                    status: root.rosCameraStatus
                    onEdited: (value) => {
                        root.cameraTopic = value
                        RosServerProbe.resetAll()
                    }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.rowSpacing
            visible: root.mode === root.modeHttp

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
        }

        Button {
            id: submitUrl

            Layout.fillWidth: true
            text: "Update"

            onClicked: {
                if (root.mode === root.modeHttp) {
                    ServerProbe.probeAll(root.host, root.axisPort,
                                        root.monitorPort, root.cameraPort)
                } else {
                    root.rosUpdateRequested(root.domainId, root.axisNamespace,
                                            root.monitorNamespace, root.cameraTopic)
                }
            }

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
