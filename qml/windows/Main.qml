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
    }

    // Direction Diff: the two 8-direction ops, against the two centres.
    //
    // ONE request each, covering all eight directions and both polarities. The
    // reply is cached in ComputeController.histogram and the panels filter it
    // locally, so ticking a direction costs nothing. The old Widgets client asked
    // for only the ticked directions and therefore had to re-fetch whenever one
    // was added; collecting all eight once is why this app does not.
    //
    // Lives on the window rather than inside the CameraPanel handler because two
    // controls now start it: Direction Diff, and a histogram panel's Show Curve
    // when it has nothing to draw yet.
    //
    // Refused rather than defaulted when a centre is missing. A centre of (-1,-1)
    // is what "no centre" looks like here, and running the detection against it
    // returns curves measured from the image corner -- plausible-looking numbers
    // that are not a measurement of anything.
    function centresReady() {
        if (centering.hasPositiveCenter && centering.hasNegativeCenter)
            return true
        camera.patternError =
            qsTr("Set both centres first -- press Find in the Centering panel, "
               + "or click each shot in Manual mode.")
        return false
    }

    // The curves alone. Cheap by comparison, and the only thing a noise-cleaning
    // toggle needs.
    //
    // Split out from runDirectionDiff on 2026-09-09 because the toggle felt slow
    // next to the Widgets client. It was: every detect op takes computeMutex on
    // the compute node, so histogram_8dir and nodes_8dir SERIALISE -- one toggle
    // was two full sweeps of two 3040x3040 frames, one after the other, when only
    // the first changes anything the operator is looking at.
    //
    // The old client's toggle ran showCurve(1) and showCurve(2) and nothing else.
    // It fetched nodes inside Update Table instead, which is where they are
    // actually consumed.
    function refreshCurves() {
        if (!centresReady())
            return
        camera.patternError = ""
        ComputeController.histogram8Dir(centering.positiveCpx, centering.positiveCpy,
                                        centering.negativeCpx, centering.negativeCpy,
                                        centering.noiseCleaning)
    }

    // Both, for the explicit Direction Diff press: curves to look at, and nodes
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
                        // Every controller that owns a ROS context gets connected
                        // here, and only here -- the app deliberately does not
                        // connect on startup, so pressing Update is what creates
                        // them. A controller missing from this list does not fail;
                        // it sits idle for ever with no message, which reads as a
                        // dead rig rather than an unwired button. That is how the
                        // last three went missing in the 2026-09-08 merge.
                        //
                        // MonitorController is deliberately NOT here. After that
                        // merge the monitor work is PatternController's, and nothing
                        // in QML calls MonitorController any more -- connecting it
                        // would build a context, node and executor to serve no
                        // caller, and each one adds to the XTYPES wall Windows
                        // prints on Update. Re-add the line the moment something
                        // needs it; do not add it back speculatively.
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

                    // The centres come from the Centering panel because that is
                    // where they were established -- either by the auto_center
                    // cascade or by hand in Manual mode. Refusals are reported
                    // through patternError, which this panel shows as errorText;
                    // this window has no toast.
                    // Not a measurement control. The Widgets client's
                    // btn_direction_diff is a "?" that opens a popup of the
                    // pairwise ICT differences -- N-S, W-E, NW-SE, SW-NE -- and
                    // that is what this restores.
                    //
                    // It re-measures first, exactly as the old one did, so the
                    // popup shows what Update Table would write rather than
                    // whatever the last shot left behind. Noise cleaning may have
                    // been toggled since, and that now refreshes only the curves.
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

                        // Restored 2026-09-08. The merge from
                        // v2.1_2026_New-UI-CPP-ROS brought a Main.qml that binds
                        // only this panel's geometry, so Start, Stop and Snapshot
                        // emitted into nothing and no frame ever arrived -- a panel
                        // that looked idle rather than unwired.
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

                        // Re-measure when noise cleaning is toggled, so the effect
                        // is visible immediately -- which is the whole reason the
                        // operator touched it.
                        //
                        // This is what the Widgets client does: its Clean Noise
                        // toggle sets the flag and then calls showCurve(1) and
                        // showCurve(2) on the spot. Here the flag travelled with
                        // the next request and nothing re-ran, so toggling it
                        // looked like a switch wired to nothing until you happened
                        // to press Direction Diff again.
                        //
                        // Only once there is already a measurement to redo. Before
                        // that the toggle is just a setting, and firing two
                        // 8-direction sweeps at an empty pair would fail noisily
                        // for no reason.
                        onNoiseCleaningChanged: {
                            if (ComputeController.hasHistogram || ComputeController.hasNodes)
                                window.refreshCurves()
                        }

                        // The second half of the shot-centres-itself path. Fires
                        // only for a centre a CAPTURE asked for -- a manual click
                        // in the image sets a centre too, and re-measuring both
                        // 8-direction sweeps on every click would make placing a
                        // centre by hand unusable.
                        //
                        // centerChanged was declared on this panel and handled
                        // nowhere until now; the audit on 2026-09-08 listed it as
                        // a signal emitted into an empty room.
                        onCenterChanged: (target, x, y) => {
                            const slot = target === "Positive" ? "positive" : "negative"
                            if (!window.autoCentrePending[slot])
                                return
                            delete window.autoCentrePending[slot]

                            // Both halves, and nothing still being centred:
                            // histogram_8dir measures the pair against the two
                            // centres, so running it half-centred measures the
                            // second image from wherever the last pair left it.
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

        // The join between the two halves of the app, and the reason this lives
        // here rather than in either window: the crossings belong to
        // ComputeController, the PCT to the pattern panels, and the table to the
        // Cali Result window. Only Main.qml can see all three.
        //
        // Direction Diff has always fetched these nodes and thrown them away --
        // ComputeController.nodes had no reader at all until now. So the rig was
        // already paying for the 8-direction detection and discarding the answer.
        onUpdateTableRequested: (round) => {
            // The nodes are re-measured here, not reused from the last Direction
            // Diff, because noise cleaning may have been toggled since. The
            // toggle deliberately refreshes only the curves -- see refreshCurves
            // -- so cached nodes can be from the other setting, and a table filled
            // from those is a measurement of a configuration nobody chose.
            //
            // The Widgets client did the same: its Update Table called nodes8dir
            // itself rather than trusting whatever the curve panels last fetched.
            if (!centering.hasPositiveCenter || !centering.hasNegativeCenter) {
                toast.show(qsTr("Set both centres first -- the crossings are measured from "
                              + "them."), true)
                return
            }

            const pct = patternAndMonitor.pctList()

            // A pattern window that has never been opened, or whose layers are all
            // zero, would hand over 75 zeros and the table would fill with a
            // plausible, wrong PCT column. The old client degraded exactly that
            // way, silently. Refuse instead: pct_cal is a running sum, so a wrong
            // PCT is not a wrong cell, it is a wrong curve.
            let usable = 0
            for (let i = 0; i < pct.length; ++i)
                if (parseInt(pct[i]) > 0) ++usable
            if (usable === 0) {
                toast.show(qsTr("The pattern has no layer sizes to use as PCT. Open Pattern & "
                              + "Monitor and load or build the pattern this pair was shot "
                              + "against."), true)
                return
            }

            // Fetch, then fill when the crossings land. Held here rather than
            // chained inside the controller so the round and the PCT are the ones
            // the operator saw when they pressed the button, not whatever is
            // selected by the time the reply arrives.
            window.pendingFill = { round: round, pct: pct }
            ComputeController.nodes8Dir(centering.positiveCpx, centering.positiveCpy,
                                        centering.negativeCpx, centering.negativeCpy,
                                        centering.noiseCleaning)
        }
    }

    PatternAndMonitor {
        id: patternAndMonitor
    }

    // This window had no way to say anything until now, which is why the Direction
    // Diff refusal had to borrow the camera panel's error line. Update Table can
    // refuse for two unrelated reasons and neither belongs on the camera.
    StatusToast {
        id: toast

        anchors.centerIn: parent
        z: 100
    }

    // ComputeController runs every detect op in the app -- auto_center, roi_exact,
    // histogram_8dir, nodes_8dir -- and until 2026-09-09 NOTHING listened to it.
    // It set lastError correctly, emitted errorRaised faithfully, and every word
    // went nowhere: CalibrationController is heard in the Cali Result window and
    // PatternController in Pattern & Monitor, but this one had no window at all.
    //
    // Found when Direction Diff appeared to do nothing. It fires histogram_8dir
    // AND nodes_8dir; the nodes arrived and the histogram did not, and the reason
    // the server gave was discarded on the way in. A control that reaches its
    // controller and a controller that reports its failure still add up to
    // silence if no one is connected to the other end.
    // Set by Update Table while its nodes are being measured; consumed once, when
    // they arrive. Null the rest of the time, so an ordinary Direction Diff does
    // not fill a table nobody asked to fill.
    property var pendingFill: null

    // Set while Direction Diff's measurement is in flight, so the popup opens on
    // the nodes it asked for rather than on a shot's own refresh arriving first.
    property bool pendingDiff: false

    Connections {
        target: ComputeController

        function onErrorRaised(message) {
            // A failed measurement must not leave a fill waiting: the next
            // Direction Diff would then write a table the operator asked for
            // minutes ago, against whatever is on the glass now.
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

    // A shot centres itself, and a centred pair redraws the curves. Restored
    // 2026-09-09 from the Widgets client, which has no Find buttons at all:
    //
    //     if (mode == "Positive" || mode == "Negative") {
    //         if (radiobutton_auto->isChecked()) ctr = patternCenter(currentSlot_);
    //         ...
    //         showCurve(1); showCurve(2);
    //     }
    //
    // Everything an operator did there was: shoot the pair, read the curves. This
    // app had turned that into three separate presses -- Find Pos, Find Neg,
    // Direction Diff -- none of which existed before, and the first two of which
    // an operator had no reason to expect.
    //
    // Only in Auto. Manual means the operator is placing the centre by hand and a
    // fit that overwrote it a second later would be worse than useless; Locked
    // means leave it alone.
    property var autoCentrePending: ({})

    Connections {
        target: CameraController

        function onCaptured(slot, ok, message) {
            if (!ok) return
            if (slot !== "positive" && slot !== "negative") return
            if (centering.mode !== centering.modeAuto) return

            // AUTO IS THE MIDDLE OF THE FRAME, not a fit.
            //
            // Deliberately not auto_center. The cascade is still there and Find
            // Pos / Find Neg still run it, but it is no longer on the path a shot
            // takes: it can refuse -- correctly -- on a shot it does not trust,
            // and then nothing happens and the operator is stuck with no centre
            // and no obvious next move.
            //
            // The frame centre cannot refuse. It is exact, it is instant, and it
            // is honest in a way a doubtful fit is not: nobody can mistake
            // "half the width" for a measurement of where the lens actually is.
            // On a rig aimed even roughly it is also close, which is the whole
            // reason it works as a starting point.
            //
            // Note this is NOT what the Widgets client does -- its Auto runs
            // pattern_center, and its boxes start at hardcoded 1003/836. This is
            // a deliberate improvement on it, asked for on 2026-09-09.
            // imageSizes, not frameSizes: the middle of the PICTURE, in the same
            // pixel coordinates the detect ops will measure. frameSizes prefers
            // the size the rig reported, and when the two disagree the centre
            // would be off by exactly that difference with nothing to show for it.
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

    // Offline path: a saved capture read into a slot, so the detect ops can run
    // against it with no camera. The bytes go to ImageStore and are uploaded with
    // each /compute/detect call exactly as a fresh capture would be -- an
    // imported file has no slot on the rig to name, so uploading is not a
    // fallback here, it is the only correct transport.
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
            // Follow the slot that was just filled, so the operator sees what
            // they loaded rather than whatever the view happened to be on.
            camera.showView(openImageDialog.targetMode)
        }
    }
}
