import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Wireframe of the old mainwindow_main.ui panel layout, ported one box at
// a time. Each PanelPlaceholder below is where a real qml/panels/*.qml
// component takes over; nothing here is wired to Bridge yet.
ApplicationWindow {
    id: window
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

                PanelPlaceholder {
                    title: "HTTP Server URL"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 140
                }
                PanelPlaceholder {
                    title: "Axis Control Panel"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 260
                }
                PanelPlaceholder {
                    title: "Monitor / Pattern"
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
                }
                PanelPlaceholder {
                    title: "Calibration Result / 3D Validation"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 100
                }
            }
        }

        // One reusable Histogram panel, instantiated twice with a channel
        // number, instead of the two hand-duplicated blocks in the old UI.
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

        Label {
            text: "bridge.clickCount (proves C++ <-> QML wiring works): " + bridge.clickCount
        }
        Button {
            text: "Ping Bridge"
            onClicked: bridge.handleClick()
        }
    }
}
