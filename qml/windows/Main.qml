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

    width:  Theme.designWidth
    height: Theme.designHeight

    Component.onCompleted: {
        width  = Math.min(Theme.designWidth,  Screen.desktopAvailableWidth)
        height = Math.min(Theme.designHeight, Screen.desktopAvailableHeight)
    }

    ScaledCanvas {
        id: fit
        anchors.fill: parent

        contentWidth:  rootColumn.Layout.minimumWidth  + 2 * Theme.spaceMd
        contentHeight: rootColumn.Layout.minimumHeight + 2 * Theme.spaceMd

        ColumnLayout {
            id: rootColumn
            anchors.fill: parent
            anchors.margins: Theme.spaceMd
            spacing: Theme.spaceMd

            readonly property real free: Math.max(0, fit.canvasHeight - 4 * Theme.spaceMd)
            readonly property real histogramWidth: camera.x + camera.width

            RowLayout {
                id: workRow
                Layout.fillWidth:   true
                Layout.fillHeight:  true
                readonly property real free: Math.max(0, fit.canvasWidth - 4 * Theme.spaceMd)
                spacing: Theme.spaceMd

                Layout.preferredHeight: rootColumn.free * Theme.ratioWorkRow
                Layout.minimumHeight: Math.max(Theme.minPanelHeight, rightColumn.Layout.minimumHeight)

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredWidth: workRow.free * Theme.ratioLeft
                    Layout.minimumWidth: Theme.minColumnLeft
                    spacing: Theme.spaceMd

                    ServerConfigPanel {
                        Layout.fillWidth: true
                        onRosUpdateRequested: (domainId, axisNamespace, monitorNamespace, cameraTopic) => {
                            RosServerProbe.probeAll(domainId, axisNamespace, monitorNamespace, cameraTopic)
                            AxisController.connectTo(domainId, axisNamespace, true)
                            CameraController.connectTo(domainId)
                            PatternController.connectTo(domainId)
                        }
                    }
                    AxisControlPanel { Layout.fillWidth: true; Layout.fillHeight: true }
                }

                CameraPanel {
                    id: camera

                    readonly property string slotKey: camera.patternMode === "Positive" ? "positive"
                                                   : camera.patternMode === "Negative" ? "negative"
                                                                                       : "single"

                    property string awaitingPolarity: ""
                    property string patternError: ""

                    readonly property var frameSize: CameraController.frameSizes[camera.slotKey]

                    frameWidth: camera.frameSize ? camera.frameSize.width : 0
                    frameHeight: camera.frameSize ? camera.frameSize.height : 0

                    singlePath:   CameraController.frameUrls["single"]   || ""
                    positivePath: CameraController.frameUrls["positive"] || ""
                    negativePath: CameraController.frameUrls["negative"] || ""

                    imageLabel: CameraController.frameLabels[camera.slotKey] || ""
                    errorText: camera.patternError !== "" ? camera.patternError
                                                          : CameraController.lastError
                    busy: CameraController.busy || camera.awaitingPolarity !== ""

                    foldPath: CameraController.foldUrls[camera.slotKey] || ""
                    foldScore: CameraController.foldScores[camera.slotKey] !== undefined
                             ? CameraController.foldScores[camera.slotKey] : -1

                    function refreshFold() {
                        if (!camera.checkMode) return
                        CameraController.foldCheck(camera.slotKey, camera.centerX,
                                                   camera.centerY, camera.checkRadius)
                    }

                    onCheckModeChanged: camera.refreshFold()
                    onCheckRadiusChanged: camera.refreshFold()
                    onCenterXChanged: camera.refreshFold()
                    onCenterYChanged: camera.refreshFold()
                    onSlotKeyChanged: camera.refreshFold()

                    onCaptureRequested: (mode) => {
                        camera.patternError = ""
                        patternSettle.stop()
                        if (mode === "") {
                            camera.awaitingPolarity = ""
                            CameraController.capture("")
                            return
                        }
                        camera.awaitingPolarity = mode.toLowerCase()
                        PatternController.showPrepared(camera.awaitingPolarity)
                    }

                    Timer {
                        id: patternSettle

                        interval: Theme.patternSettleDelay
                        repeat: false

                        onTriggered: {
                            const polarity = camera.awaitingPolarity
                            camera.awaitingPolarity = ""
                            CameraController.capture(polarity)
                        }
                    }

                    Connections {
                        target: PatternController

                        function onPreparedShown(ok, polarity, shown, message) {
                            if (camera.awaitingPolarity !== polarity) return
                            if (ok) {
                                patternSettle.restart()
                            } else {
                                camera.patternError = message
                                camera.awaitingPolarity = ""
                            }
                        }
                    }

                    Connections {
                        target: CameraController

                        function onCaptured(slot, ok, message) {
                            if (!ok) return
                            const stamp = Qt.formatTime(new Date(), "HH:mm:ss")
                            if (slot === "positive") camera.positiveTime = stamp
                            else if (slot === "negative") camera.negativeTime = stamp
                            camera.refreshFold()
                        }
                    }

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredWidth: workRow.free * Theme.ratioCenter
                    Layout.minimumWidth: Math.max(Theme.minColumnCenter, camera.implicitWidth)

                    roiRadius: centering.centerRoi
                    centerLocked: centering.locked
                    manualCenter: centering.mode === centering.modeManual
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
                    Layout.minimumWidth: Theme.minColumnLeft + Theme.minColumnCenter + Theme.spaceMd
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
                            onClicked: SubAppWindows.openMeasure3d()
                        }
                        ActionButton {
                            Layout.fillWidth: true
                            tone: "accent"
                            text: qsTr("Setup Center")
                            onClicked: SubAppWindows.openCenterSetup()
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
    }
}
