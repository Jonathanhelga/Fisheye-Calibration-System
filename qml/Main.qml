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
        anchors.margins: Theme.panelGap
        spacing: Theme.panelGap

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.panelGap

            ColumnLayout {
                Layout.preferredWidth: 420
                spacing: Theme.panelGap
                
                ServerUrlPanel {
                    Layout.fillWidth: true
                }
                AxisControlPanel {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }
            }

            PanelPlaceholder {
                title: "Camera Panel"
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 500
            }

            ColumnLayout {
                Layout.preferredWidth: 380
                spacing: Theme.panelGap

                PanelPlaceholder {
                    title: "Centering"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
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
