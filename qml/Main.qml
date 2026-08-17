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

    minimumWidth: Theme.minColumnLeft
                + Theme.minColumnCenter
                + rightColumn.Layout.minimumWidth
                + 4 * Theme.spaceMd

    minimumHeight: workRow.Layout.minimumHeight
                 + 2 * Theme.minHistogramHeight
                 + 4 * Theme.spaceMd

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
            Layout.minimumHeight: Math.max(Theme.minPanelHeight, rightColumn.Layout.minimumHeight)

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: workRow.free * Theme.ratioLeft
                Layout.minimumWidth: Theme.minColumnLeft
                spacing: Theme.spaceMd

                ServerUrlPanel {  Layout.fillWidth: true }
                AxisControlPanel { Layout.fillWidth: true; Layout.fillHeight: true }
            }

            CameraPanel {
                id: camera

                singlePath: "/Users/jonathanhelga/Desktop/Fisheye_Calibration-Jojo_Version/tools/sample_shot.png"

                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: workRow.free * Theme.ratioCenter
                Layout.minimumWidth: Math.max(Theme.minColumnCenter, camera.implicitWidth)

                roiRadius: centering.centerRoi
                centerLocked: centering.locked
                centerX: camera.patternMode === "Positive" ? centering.positiveCpx
                       : camera.patternMode === "Negative" ? centering.negativeCpx
                                                           : -1
                centerY: camera.patternMode === "Positive" ? centering.positiveCpy
                       : camera.patternMode === "Negative" ? centering.negativeCpy
                                                           : -1

                onCenterPicked: (mode, x, y) => centering.setCenter(mode, x, y)
            }

            ColumnLayout {
                id: rightColumn
                Layout.fillWidth:   true
                Layout.fillHeight:  true
                Layout.preferredWidth: workRow.free * Theme.ratioRight
                Layout.minimumWidth: Math.max(Theme.minColumnRight, centering.implicitWidth)
                Layout.minimumHeight: rightColumn.implicitHeight
                spacing: Theme.spaceMd

                CenteringPanel {
                    id: centering
                    Layout.fillWidth: true
                    Layout.minimumHeight: centering.implicitHeight
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
