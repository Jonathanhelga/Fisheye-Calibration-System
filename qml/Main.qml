import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FisheyeCaliJojo

// Wireframe of the old mainwindow_main.ui panel layout.
ApplicationWindow {
    id: window
    width:  Screen.desktopAvailableWidth
    height: Screen.desktopAvailableHeight

    visible: true
    title: "Fisheye Calibration - Jojo Version"

    ColumnLayout {
        id: rootColumn
        anchors.fill: parent
        anchors.margins: Theme.spaceMd
        spacing: Theme.spaceMd

        readonly property real free: Math.max(0, window.height - 4 * Theme.spaceMd)

        RowLayout {
            id: workRow
            Layout.fillWidth:   true
            Layout.fillHeight:  true
            readonly property real free: Math.max(0, window.width - 4 * Theme.spaceMd)
            spacing: Theme.spaceMd

            Layout.preferredHeight: rootColumn.free * Theme.ratioWorkRow
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
            }

            ColumnLayout {
                Layout.fillWidth:   true
                Layout.fillHeight:  true
                Layout.preferredWidth: workRow.free * Theme.ratioRight
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
            Layout.preferredHeight: rootColumn.free * Theme.ratioHistogram
            Layout.minimumHeight: Theme.minHistogramHeight
        }
        HistogramPanel {
            channel: 2
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: rootColumn.free * Theme.ratioHistogram
            Layout.minimumHeight: Theme.minHistogramHeight
        }
    }
}
