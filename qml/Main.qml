import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FisheyeCaliJojo

// Wireframe of the old mainwindow_main.ui panel layout.
ApplicationWindow {
    id: window
    width:  Screen.desktopAvailableWidth
    height: Screen.desktopAvailableHeight
    minimumWidth:   (Theme.minColumnLeft + Theme.minColumnCenter + Theme.minColumnRight + 4 * Theme.spaceMd) * 0.5
    minimumHeight:  (Theme.minPanelHeight + 2 * Theme.minHistogramHeight + 4 * Theme.spaceMd) * 0.5

    visible: true
    title: "Fisheye Calibration - Jojo Version"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spaceMd
        spacing: Theme.spaceMd

        RowLayout {
            id: workRow
            Layout.fillWidth:   true
            Layout.fillHeight:  true
            readonly property real free: Math.max(0, window.width - 4 * Theme.spaceMd)
            spacing: Theme.spaceMd
            
            Layout.preferredHeight: 1000
            Layout.minimumHeight: Theme.minPanelHeight

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: workRow.free * Theme.ratioLeft
                Layout.minimumWidth: Theme.minColumnLeft
                spacing: Theme.spaceMd

                ServerUrlPanel {  Layout.fillWidth: true }
                AxisControlPanel { Layout.fillWidth: true; Layout.fillHeight: true }
            }

            PanelPlaceholder {
                title: "Camera Panel"
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: workRow.free * Theme.ratioCenter
                Layout.minimumWidth: Theme.minColumnCenter
            }

            ColumnLayout {
                Layout.fillWidth:   true
                Layout.fillHeight:  true
                Layout.preferredWidth: workRow.free * Theme.ratioRight
                Layout.minimumWidth: Theme.minColumnRight
                spacing: Theme.spaceMd

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
            Layout.fillHeight: true
            Layout.preferredHeight: 380
            Layout.minimumHeight: Theme.minHistogramHeight
        }
        HistogramPanel {
            channel: 2
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: 380
            Layout.minimumHeight: Theme.minHistogramHeight
        }
    }
}
