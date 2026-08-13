import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FisheyeCaliJojo

// Wireframe of the old mainwindow_main.ui panel layout.
ApplicationWindow {
    id: window
    Bridge { id: bridge }
    width: 1400
    height: 900
    visible: true
    title: "Fisheye Calibration - Jojo Version"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            ColumnLayout {
                Layout.preferredWidth: 420
                spacing: 12
                
                ServerUrlPanel {
                    Layout.fillWidth: true
                }
                PanelPlaceholder {
                    title: "Axis Control Panel"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }
            }

            PanelPlaceholder {
                title: "Camera Panel"
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 600
            }

            ColumnLayout {
                Layout.preferredWidth: 380
                spacing: 12

                PanelPlaceholder {
                    title: "Centering"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredHeight: 200
                }
                PanelPlaceholder {
                    title: "Calibration Result / 3D Validation"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }
                PanelPlaceholder {
                    title: "Monitor / Pattern"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }
            }
        }

        HistogramPanel {
            channel: 1
            Layout.fillWidth: true
            Layout.preferredHeight: 260
        }
        HistogramPanel {
            channel: 2
            Layout.fillWidth: true
            Layout.preferredHeight: 260
        }

        // Label {
        //     text: "bridge.clickCount (proves C++ <-> QML wiring works): " + bridge.clickCount
        // }
        // Button {
        //     text: "Ping Bridge"
        //     onClicked: bridge.handleClick()
        // }
    }
}
