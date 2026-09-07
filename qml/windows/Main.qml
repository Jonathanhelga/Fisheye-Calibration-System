import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
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
        camera.cameraFov = CameraController.fov
    }

    // Both histogram panels and the ICT extraction need the two centres. They are
    // taken from the Centering panel, which is where they were established --
    // either by the auto_center cascade or by hand.
    function runDirectionDiff() {
        if (!centering.hasPositiveCenter || !centering.hasNegativeCenter) {
            toast.show(qsTr("Set both centres first -- press Find in the Centering panel, "
                          + "or click each shot in Manual mode."), true)
            return
        }
        ComputeController.histogram8Dir(centering.positiveCpx, centering.positiveCpy,
                                        centering.negativeCpx, centering.negativeCpy,
                                        centering.noiseCleaning)
        ComputeController.nodes8Dir(centering.positiveCpx, centering.positiveCpy,
                                    centering.negativeCpx, centering.negativeCpy,
                                    centering.noiseCleaning)
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
                        // One press opens every link. They are separate contexts
                        // and separate nodes on purpose -- a slow camera capture
                        // must not block a stop command -- but they all live or
                        // die with the same Update, so the operator never has a
                        // half-connected rig to reason about.
                        onRosUpdateRequested: (domainId, axisNamespace, monitorNamespace, cameraTopic) => {
                            RosServerProbe.probeAll(domainId, axisNamespace, monitorNamespace, cameraTopic)
                            AxisController.connectTo(domainId, axisNamespace, true)
                            CameraController.connectTo(domainId)
                            PatternController.connectTo(domainId)
                            MonitorController.connectTo(domainId)
                            ComputeController.connectTo(domainId)
                            CalibrationController.connectTo(domainId)
                        }
                    }
                    AxisControlPanel { Layout.fillWidth: true; Layout.fillHeight: true }
                }

                CameraPanel {
                    id: camera

                    singlePath:   CameraController.frameUrl
                    positivePath: CameraController.positiveUrl
                    negativePath: CameraController.negativeUrl
                    positiveTime: CameraController.positiveTime
                    negativeTime: CameraController.negativeTime

                    imageLabel: CameraController.frameLabel
                    errorText: CameraController.lastError
                    busy: CameraController.busy || CameraController.pairing
                    pendingMode: CameraController.pendingSlot
                    pairing: CameraController.pairing

                    // The slot name travels through unchanged: "" is the plain
                    // Capture button, "Positive" / "Negative" are the retakes.
                    // The old handler dropped everything but "", which is why
                    // Pos and Neg looked enabled and did nothing.
                    onCaptureRequested: (mode) => CameraController.capture(mode)
                    onPairRequested: CameraController.capturePair()
                    onBrowseRequested: openImageDialog.open()
                    onDirectionDiffRequested: window.runDirectionDiff()
                    onFovEdited: (value) => CameraController.fov = value

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

                    // The edge ring follows whichever polarity the view is on, so
                    // the positive radius is never drawn over a negative shot.
                    edgeVisible: centering.edgeShown(camera.patternMode)
                    edgeRadius: centering.edgeRadius(camera.patternMode)
                    edgeThickness: centering.edgeThickness(camera.patternMode)
                    edgeColor: centering.edgeColor(camera.patternMode)

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

                        framePath:  CameraController.liveUrl
                        streaming:  CameraController.streaming
                        receiving:  CameraController.receiving
                        fps:        CameraController.fps
                        linkStatus: CameraController.status

                        onStartRequested: CameraController.startStream()
                        onStopRequested: CameraController.stopStream()
                        // A preview frame kept as the Camera panel image. It came
                        // off a BEST_EFFORT topic and is labelled as such, so it
                        // is never mistaken for a measurement.
                        onSnapshotRequested: CameraController.snapshot()

                        roiRadius: centering.centerRoi
                        centerX: centering.hasPositiveCenter ? centering.positiveCpx
                                                             : centering.negativeCpx
                        centerY: centering.hasPositiveCenter ? centering.positiveCpy
                                                             : centering.negativeCpy

                        // Matched to the centre being drawn, positive preferred,
                        // so the ring and the cross always describe one polarity.
                        readonly property string edgeSide: centering.hasPositiveCenter
                                                             ? "Positive" : "Negative"
                        edgeVisible: centering.edgeShown(live.edgeSide)
                        edgeRadius: centering.edgeRadius(live.edgeSide)
                        edgeThickness: centering.edgeThickness(live.edgeSide)
                        edgeColor: centering.edgeColor(live.edgeSide)
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

        // The mapping itself is applied inside that window, which owns the
        // monitor link. This is the record for anything in the main window that
        // needs to know the screens were remapped.
        onApplyMappingRequested: (top, north, west, south, east) => {
            toast.show(qsTr("Display mapping sent: TOP=%1 N=%2 W=%3 S=%4 E=%5")
                       .arg(top).arg(north).arg(west).arg(south).arg(east), false)
        }
    }

    StatusToast {
        id: toast

        anchors.centerIn: parent
        z: 100
    }

    FileDialog {
        id: openImageDialog

        title: qsTr("Open an image into the %1 slot")
                   .arg(camera.patternMode === "" ? qsTr("single") : camera.patternMode.toLowerCase())
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("Images (*.png *.jpg *.jpeg *.bmp)"), qsTr("All files (*)")]

        Component.onCompleted: openImageDialog.currentFolder = PatternIo.defaultImageDirectory

        // The file lands in whichever slot the view is showing, so an existing
        // pair can be re-analysed with no rig attached.
        onAccepted: CameraController.openImage(openImageDialog.selectedFile, camera.patternMode)
    }

    Connections {
        target: CameraController

        function onErrorRaised(message) { toast.show(message, true) }
        function onNotice(message) { toast.show(message, false) }
        function onPairFailed(reason) { toast.show(qsTr("Pair shot: %1").arg(reason), true) }

        // A finished pair is the moment both centres can be established, so the
        // fits are offered straight away rather than waiting to be asked.
        function onPairComplete() {
            if (centering.mode !== centering.modeLocked) {
                centering.findCenter("Positive")
                centering.findCenter("Negative")
            }
        }
    }

    Connections {
        target: ComputeController

        function onErrorRaised(message) { toast.show(message, true) }
        function onNotice(message) { toast.show(message, false) }
    }
}
