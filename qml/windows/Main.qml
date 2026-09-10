import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import QtQuick.Window
import FisheyeCaliJojo

// Wireframe of the old mainwindow_main.ui layout.
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

    // Direction Diff: both 8-direction ops, one request each.
    function centresReady() {
        if (centering.hasPositiveCenter && centering.hasNegativeCenter)
            return true
        camera.patternError =
            qsTr("Set both centres first -- press Find in the Centering panel, "
               + "or click each shot in Manual mode.")
        return false
    }

    // Curves only. The noise toggle needs nothing more.
    function refreshCurves() {
        if (!centresReady())
            return
        camera.patternError = ""
        ComputeController.histogram8Dir(centering.positiveCpx, centering.positiveCpy,
                                        centering.negativeCpx, centering.negativeCpy,
                                        centering.noiseCleaning)
    }

    // Both, for an explicit Direction Diff press.
    // for the table.
    function runDirectionDiff() {
        if (!centresReady())
            return
        camera.patternError = ""
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

            readonly property real free: Math.max(0, fit.canvasHeight - 5 * Theme.spaceMd
                                                     - header.implicitHeight)
            readonly property real histogramWidth: camera.x + camera.width

            RowLayout {
                id: header

                Layout.fillWidth: true
                spacing: Theme.rowSpacing

                Label {
                    text: qsTr("Fisheye Calibration")
                    font.bold: true
                    color: Theme.accent
                }

                Item { Layout.fillWidth: true }

                HelpButton {
                    page: "main"
                }
            }

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
                        // Connect every ROS controller here, and only here.
                        onRosUpdateRequested: (domainId, axisNamespace, monitorNamespace, cameraTopic) => {
                            RosServerProbe.probeAll(domainId, axisNamespace, monitorNamespace, cameraTopic)
                            AxisController.connectTo(domainId, axisNamespace, true)
                            CameraController.connectTo(domainId)
                            PatternController.connectTo(domainId)
                            ComputeController.connectTo(domainId)
                            CalibrationController.connectTo(domainId)
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
                    property string pairStage: ""
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

                    onPairRequested: {
                        camera.pairStage = "positive"
                        camera.requestCapture("Positive")
                    }

                    onCaptureRequested: (mode) => {
                        camera.patternError = ""
                        patternSettle.stop()
                        if (camera.pairStage !== "" && mode.toLowerCase() !== camera.pairStage)
                            camera.pairStage = ""
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
                                camera.pairStage = ""
                            }
                        }
                    }

                    Connections {
                        target: CameraController

                        function onCaptured(slot, ok, message) {
                            if (!ok) {
                                camera.pairStage = ""
                                return
                            }
                            const stamp = Qt.formatTime(new Date(), "HH:mm:ss")
                            if (slot === "positive") camera.positiveTime = stamp
                            else if (slot === "negative") camera.negativeTime = stamp
                            camera.refreshFold()
                            if (camera.pairStage !== slot) return
                            if (slot === "positive") {
                                camera.pairStage = "negative"
                                camera.requestCapture("Negative")
                            } else {
                                camera.pairStage = ""
                            }
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

                    onOpenImageRequested: (mode) => {
                        openImageDialog.targetMode = mode
                        openImageDialog.open()
                    }

                    // Centres come from the Centering panel.
                    // Info popup, not a measurement control.
                    onDirectionDiffRequested: {
                        if (!window.centresReady())
                            return
                        camera.patternError = ""
                        window.pendingDiff = true
                        ComputeController.nodes8Dir(
                            centering.positiveCpx, centering.positiveCpy,
                            centering.negativeCpx, centering.negativeCpy,
                            centering.noiseCleaning)
                    }
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

                        // Restored 2026-09-08; the merge left this unwired.
                        framePath:  CameraController.liveUrl
                        streaming:  CameraController.streaming
                        receiving:  CameraController.receiving
                        fps:        CameraController.fps
                        linkStatus: CameraController.status

                        onStartRequested: CameraController.startStream()
                        onStopRequested: CameraController.stopStream()
                        onSnapshotRequested: CameraController.snapshot()

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

                        // Re-measure so the toggle has visible effect.
                        onNoiseCleaningChanged: {
                            if (ComputeController.hasHistogram || ComputeController.hasNodes)
                                window.refreshCurves()
                        }

                        // Only a capture's centre triggers a refresh.
                        onCenterChanged: (target, x, y) => {
                            const slot = target === "Positive" ? "positive" : "negative"
                            if (!window.autoCentrePending[slot])
                                return
                            delete window.autoCentrePending[slot]

                            // Both halves, and nothing still being centred.
                            if (Object.keys(window.autoCentrePending).length === 0
                                    && centering.hasPositiveCenter
                                    && centering.hasNegativeCenter)
                                window.runDirectionDiff()
                        }
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

                        onFetchRequested: window.runDirectionDiff()
                    }
                    HistogramPanel {
                        channel: 2
                        Layout.fillWidth:  true
                        Layout.fillHeight: true
                        Layout.minimumHeight: Theme.minHistogramHeight

                        onFetchRequested: window.runDirectionDiff()
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

        captureReady: centering.hasPositiveCenter && centering.hasNegativeCenter
                      && (CameraController.frameUrls["positive"] || "") !== ""
                      && (CameraController.frameUrls["negative"] || "") !== ""

        // Update Table: only this window sees all three.
        onUpdateTableRequested: (round) => {
            // Re-measure; the noise toggle refreshes only curves.
            if (!centering.hasPositiveCenter || !centering.hasNegativeCenter) {
                toast.show(qsTr("Set both centres first -- the crossings are measured from "
                              + "them."), true)
                return
            }

            const pct = patternAndMonitor.pctList()

            // Refuse on a missing PCT; pct_cal compounds.
            let usable = 0
            for (let i = 0; i < pct.length; ++i)
                if (parseInt(pct[i]) > 0) ++usable
            if (usable === 0) {
                toast.show(qsTr("The pattern has no layer sizes to use as PCT. Open Pattern & "
                              + "Monitor and load or build the pattern this pair was shot "
                              + "against."), true)
                return
            }

            // Capture round and PCT at press time.
            window.pendingFill = { round: round, pct: pct }
            ComputeController.nodes8Dir(centering.positiveCpx, centering.positiveCpy,
                                        centering.negativeCpx, centering.negativeCpy,
                                        centering.noiseCleaning)
        }
    }

    PatternAndMonitor {
        id: patternAndMonitor
    }

    // This window's own error line.
    StatusToast {
        id: toast

        anchors.centerIn: parent
        z: 100
    }

    // Nobody listened to ComputeController before 2026-09-09.
    // Set by Update Table; consumed once.
    property var pendingFill: null

    // Set while Direction Diff is in flight.
    property bool pendingDiff: false

    Connections {
        target: ComputeController

        function onErrorRaised(message) {
            // A failure must not leave a fill pending.
            window.pendingFill = null
            window.pendingDiff = false
            toast.show(message, true)
        }

        function onNotice(message) {
            toast.show(message, false)
        }

        function onNodesChanged() {
            if (window.pendingDiff) {
                window.pendingDiff = false
                if (!ComputeController.hasNodes) {
                    toast.show(qsTr("No nodes were detected in this pair."), true)
                } else {
                    directionDiffDialog.nodes = ComputeController.nodes
                    directionDiffDialog.open()
                }
            }

            if (!window.pendingFill)
                return
            const job = window.pendingFill
            window.pendingFill = null

            if (!ComputeController.hasNodes) {
                toast.show(qsTr("The detection found no crossings, so there is nothing to "
                              + "put in the table."), true)
                return
            }
            CalibrationController.updateFromCapture(job.round, job.pct, ComputeController.nodes)
        }
    }

    // A shot centres itself; Auto only.
    property var autoCentrePending: ({})

    Connections {
        target: CameraController

        function onCaptured(slot, ok, message) {
            if (!ok) return
            if (slot !== "positive" && slot !== "negative") return
            if (centering.mode !== centering.modeAuto) return

            // Auto is the frame's middle, not a fit.
            // imageSizes, not frameSizes: the picture's own pixels.
            const size = CameraController.imageSizes[slot]
            if (!size || size.width <= 0 || size.height <= 0) {
                toast.show(qsTr("The %1 image has no size yet, so there is no middle to take.")
                               .arg(slot), true)
                return
            }

            window.autoCentrePending[slot] = true
            centering.setCenter(slot === "positive" ? "Positive" : "Negative",
                                Math.round(size.width / 2), Math.round(size.height / 2),
                                qsTr("frame centre"))
        }
    }

    DirectionDiffDialog {
        id: directionDiffDialog
        anchors.centerIn: parent
    }

    // Offline path: a saved capture, uploaded per op.
    FileDialog {
        id: openImageDialog

        property string targetMode: ""

        title: qsTr("Open a capture into the %1 slot")
                   .arg(openImageDialog.targetMode === "" ? qsTr("Single")
                                                          : openImageDialog.targetMode)
        nameFilters: [qsTr("Images (*.png *.jpg *.jpeg)"), qsTr("All files (*)")]

        onAccepted: {
            if (!CameraController.openImage(openImageDialog.selectedFile,
                                            openImageDialog.targetMode))
                return
            // Follow the slot that was just filled.
            camera.showView(openImageDialog.targetMode)
        }
    }
}
