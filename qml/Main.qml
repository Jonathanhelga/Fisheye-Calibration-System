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
                + 8 * Theme.spaceMd

    minimumHeight: workRow.Layout.minimumHeight
                 + bottomRow.Layout.minimumHeight
                 + 5 * Theme.spaceMd

    ColumnLayout {
        id: rootColumn
        anchors.fill: parent
        anchors.margins: Theme.spaceMd
        spacing: Theme.spaceMd

        readonly property real free: Math.max(0, window.height - 4 * Theme.spaceMd)
        readonly property real histogramWidth: camera.x + camera.width

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

                ServerConfigPanel {  Layout.fillWidth: true }
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
                Layout.minimumWidth: Math.max(Theme.minColumnRight,
                                             centering.implicitWidth,
                                             live.implicitWidth)
                Layout.minimumHeight: rightColumn.implicitHeight
                spacing: Theme.spaceMd

                LiveCameraPanel {
                    id: live

                    framePath: camera.singlePath

                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    roiRadius: centering.centerRoi
                    centerX: centering.hasPositiveCenter ? centering.positiveCpx
                                                         : centering.negativeCpx
                    centerY: centering.hasPositiveCenter ? centering.positiveCpy
                                                         : centering.negativeCpy
                }

                CenteringPanel {
                    id: centering
                    Layout.fillWidth: true
                    Layout.minimumHeight: centering.implicitHeight
                }
            }
        }
        RowLayout {
            id: bottomRow
            Layout.fillWidth:  true
            Layout.fillHeight: true
            spacing: Theme.spaceMd

            Layout.preferredHeight: rootColumn.free * 2 * Theme.ratioHistogram
            Layout.minimumHeight: Math.max(2 * Theme.minHistogramHeight + Theme.spaceMd,
                                           toolColumn.implicitHeight)

            ColumnLayout {
                Layout.fillHeight: true
                Layout.fillWidth: false
                Layout.preferredWidth: rootColumn.histogramWidth
                Layout.maximumWidth: rootColumn.histogramWidth
                spacing: Theme.spaceMd

                HistogramPanel {
                    channel: 1
                    Layout.fillWidth:  true
                    Layout.fillHeight: true
                    Layout.minimumHeight: Theme.minHistogramHeight
                }
                HistogramPanel {
                    channel: 2
                    Layout.fillWidth:  true
                    Layout.fillHeight: true
                    Layout.minimumHeight: Theme.minHistogramHeight
                }
            }

            ColumnLayout {
                id: toolColumn
                Layout.fillWidth:  true
                Layout.fillHeight: true
                Layout.minimumWidth: Math.max(Theme.minColumnRight,
                                              measurement.implicitWidth,
                                              validation.implicitWidth)
                spacing: Theme.spaceMd

                readonly property real panelHeight: Math.max(measurement.implicitHeight,
                                                             validation.implicitHeight)

                PanelPlaceholder {
                    id: measurement
                    title: qsTr("Measurement")

                    Layout.fillWidth:  true
                    Layout.fillHeight: true
                    Layout.preferredHeight: toolColumn.panelHeight
                    Layout.minimumHeight:   toolColumn.panelHeight

                    ActionButton {
                        Layout.fillWidth: true
                        tone: "accent"
                        text: qsTr("Monitor Viewer")
                        checked: monitorViewer.visible
                        onClicked: monitorViewer.visible = !monitorViewer.visible
                    }
                    ActionButton {
                        Layout.fillWidth: true
                        tone: "accent"
                        text: qsTr("PCT (Pattern Generator)")
                    }
                    ActionButton {
                        Layout.fillWidth: true
                        tone: "accent"
                        text: qsTr("Moil Calibration Result")
                    }
                }

                PanelPlaceholder {
                    id: validation
                    title: qsTr("Validation")

                    Layout.fillWidth:  true
                    Layout.fillHeight: true
                    Layout.preferredHeight: toolColumn.panelHeight
                    Layout.minimumHeight:   toolColumn.panelHeight

                    ActionButton {
                        Layout.fillWidth: true
                        tone: "accent"
                        text: qsTr("3D Verification")
                    }
                    ActionButton {
                        Layout.fillWidth: true
                        tone: "accent"
                        text: qsTr("Setup Center")
                    }
                }
            }
        }
    }

    MonitorViewerWindow {
        id: monitorViewer

        onApplyMappingRequested: (top, north, west, south, east) => {
            console.log("[Main] Monitor Viewer apply mapping: top=" + top
                + " n=" + north + " w=" + west + " s=" + south + " e=" + east)
        }
    }
}
