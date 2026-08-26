import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import FisheyeCaliJojo

// Wireframe of the old mainwindow_main.ui panel layout.
ApplicationWindow {
    id: window

    visible: true
    visibility: Window.Maximized
    title: qsTr("Fisheye Calibration - Jojo Version")

    width:  Math.min(Theme.designWidth,  Screen.desktopAvailableWidth)
    height: Math.min(Theme.designHeight, Screen.desktopAvailableHeight)

    Item {
        id: fitter
        anchors.fill: parent

        readonly property real factor: Math.max(0.01,
                                                Math.min(1,
                                                         fitter.width  / Theme.designWidth,
                                                         fitter.height / Theme.designHeight))

        Item {
            id: canvas
            width:  fitter.width  / fitter.factor
            height: fitter.height / fitter.factor
            transformOrigin: Item.TopLeft
            scale: fitter.factor

            ColumnLayout {
                id: rootColumn
                anchors.fill: parent
                anchors.margins: Theme.spaceMd
                spacing: Theme.spaceMd

                readonly property real free: Math.max(0, canvas.height - 4 * Theme.spaceMd)
                readonly property real histogramWidth: camera.x + camera.width

                RowLayout {
                    id: workRow
                    Layout.fillWidth:   true
                    Layout.fillHeight:  true
                    readonly property real free: Math.max(0, canvas.width - 4 * Theme.spaceMd)
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

                        singlePath: "qrc:/qt/qml/FisheyeCaliJojo/tools/sample_shot.png"
                        positivePath: "qrc:/qt/qml/FisheyeCaliJojo/tools/sample_shot.png"
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
                                text: qsTr("Pattern & Monitor")
                                checked: patternAndMonitor.visible
                                onClicked: patternAndMonitor.visible = !patternAndMonitor.visible
                            }
                            ActionButton {
                                Layout.fillWidth: true
                                tone: "accent"
                                text: qsTr("Moil Calibration Result")
                                checked: moilResult.visible
                                onClicked: moilResult.visible = !moilResult.visible
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
        }
    }

    MoilCalibrationResult {
        id: moilResult
    }

    PatternAndMonitor {
        id: patternAndMonitor

        onApplyMappingRequested: (top, north, west, south, east) => {
            console.log("[Main] Pattern & Monitor apply mapping: top=" + top
                + " n=" + north + " w=" + west + " s=" + south + " e=" + east)
        }
    }
}
