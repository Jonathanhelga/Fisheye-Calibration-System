import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import FisheyeCaliJojo
import "../panels/CaliSystems.js" as CaliSystems

Window {
    id: root

    title: qsTr("MOIL Calibration Result")

    readonly property real availableWidth:  Math.min(Screen.width,  Screen.desktopAvailableWidth)
    readonly property real availableHeight: Math.min(Screen.height, Screen.desktopAvailableHeight)

    readonly property real naturalWidth:  Math.min(fit.contentWidth,  root.availableWidth)
    readonly property real naturalHeight: Math.min(fit.contentHeight, root.availableHeight)

    minimumWidth:  Math.round(root.naturalWidth  * Theme.minCanvasScale)
    minimumHeight: Math.round(root.naturalHeight * Theme.minCanvasScale)
    maximumWidth:  root.availableWidth
    maximumHeight: root.availableHeight

    width:  root.naturalWidth
    height: root.naturalHeight

    // All of it belongs to the controller. The window is the form; the table, the
    // load state and the option flags live where the ops that use them live.
    readonly property bool busy: CalibrationController.busy
    readonly property int loadStatus: CalibrationController.loadStatus

    property alias caliFolder: caliFolderField.text
    property alias caliSystem: caliSystemCombo.currentIndex

    readonly property int folderStatus: root.busy ? ProbeStatus.Checking
                                      : root.caliFolder.length === 0 ? ProbeStatus.Unknown
                                                                     : root.loadStatus

    readonly property string folderStatusText: root.busy
                                                 ? (CalibrationController.activity !== ""
                                                        ? CalibrationController.activity
                                                        : qsTr("Loading..."))
                                             : CalibrationController.loadSummary !== ""
                                                 ? CalibrationController.loadSummary
                                             : root.caliFolder.length === 0 ? qsTr("No folder chosen")
                                             : root.loadStatus === ProbeStatus.Ok ? qsTr("Loaded")
                                             : root.loadStatus === ProbeStatus.Partial ? qsTr("Partly loaded")
                                             : root.loadStatus === ProbeStatus.Failed ? qsTr("Load failed")
                                                                                      : qsTr("Not loaded yet")
    readonly property int viewData: 0
    readonly property int viewParameter: 1
    readonly property int viewOverlap: 2
    readonly property int viewAggregation: 3
    readonly property int viewGraphs: 4

    property alias view: viewSelector.currentIndex
    property alias round: dataPanel.round

    // The toggle moved into the Data panel next to the Distance field it
    // qualifies (New-UI branch). The alias keeps `root.singleDistance` reading
    // for anything outside that still asks the window.
    property alias singleDistance: dataPanel.singleDistance


    signal browseRequested()
    signal loadAllExcelRequested()
    signal loadExcelRequested()
    signal saveExcelRequested()
    // Whether a capture can be turned into a round: a pair with a centre on each.
    // Bound from Main.qml, which owns the camera and the Centering panel -- this
    // window can see neither, and the centres are not always ComputeController's
    // to report (in Auto they are the frame's middle, set client-side).
    property bool captureReady: false

    signal updateTableRequested(int round)
    signal loadDatabaseRequested()
    signal stopRequested()
    signal clearTableRequested()
    signal clearAllTablesRequested()

    // The camera parameters, as the .json the rest of the toolchain reads. Built
    // from what was measured and fitted, not from anything typed here.
    function parameterDocument() {
        return {
            "cameraName": parameterPanel.cameraName,
            "cameraFov": parseFloat(parameterPanel.cameraFov) || 0,
            "cameraSensorWidth": parseFloat(parameterPanel.cameraSensorWidth) || 0,
            "cameraSensorHeight": parseFloat(parameterPanel.cameraSensorHeight) || 0,
            "iCx": parseFloat(parameterPanel.iCx) || 0,
            "iCy": parseFloat(parameterPanel.iCy) || 0,
            "ratio": parseFloat(parameterPanel.ratio) || 0,
            "imageWidth": parseFloat(parameterPanel.imageWidth) || 0,
            "imageHeight": parseFloat(parameterPanel.imageHeight) || 0,
            "calibrationRatio": parseFloat(parameterPanel.calibrationRatio) || 0,
            "parameter0": parseFloat(parameterPanel.coefficients[0]) || 0,
            "parameter1": parseFloat(parameterPanel.coefficients[1]) || 0,
            "parameter2": parseFloat(parameterPanel.coefficients[2]) || 0,
            "parameter3": parseFloat(parameterPanel.coefficients[3]) || 0,
            "parameter4": parseFloat(parameterPanel.coefficients[4]) || 0,
            "parameter5": parseFloat(parameterPanel.coefficients[5]) || 0
        }
    }

    function configurationDocument() {
        return {
            "calibration_system": caliSystemCombo.currentText,
            "distance_per_round": parseFloat(parameterPanel.distancePerRound) || 0,
            "base_distance": CalibrationController.baseDistance,
            "single_distance": CalibrationController.singleDistance
        }
    }

    // Push a rig profile into the table fields the pipeline actually reads.
    // Every value goes through CalibrationController.setField, so it travels
    // with the table on the next op rather than living in a QML property the
    // server never sees.
    function applyCaliSystem(index) {
        const fields = CaliSystems.fieldsFor(index)
        if (!fields) {
            toast.show(qsTr("No profile for that system"), true)
            return
        }
        for (const name in fields)
            CalibrationController.setField(name, fields[name])

        const system = CaliSystems.at(index)
        toast.show(qsTr("%1 applied: pixel %2 / %3, gaps from %4")
                   .arg(system.name).arg(system.pixel_top).arg(system.pixel_side)
                   .arg(system.file), false)
    }

    function writeJson(fileUrl, document, label) {
        if (PatternIo.writeText(fileUrl, JSON.stringify(document, null, 2)))
            toast.show(qsTr("Saved %1").arg(label), false)
        else
            toast.show(qsTr("Could not save %1: %2").arg(label).arg(PatternIo.lastError), true)
    }

    ScaledCanvas {
        id: fit
        anchors.fill: parent

        contentWidth:  content.Layout.minimumWidth  + 2 * Theme.spaceMd
        contentHeight: content.Layout.minimumHeight + 2 * Theme.spaceMd

        ColumnLayout {
            id: content

            anchors.fill: parent
            anchors.margins: Theme.spaceMd
            spacing: Theme.spaceMd

            RowLayout {
                id: header

                Layout.fillWidth: true
                spacing: Theme.rowSpacing

                Label {
                    text: qsTr("Calibration Result")
                    font.bold: true
                    color: Theme.accent
                }
                Label {
                    Layout.fillWidth: true
                    text: qsTr("Review the measured PCT / ICT capture of every round, compute α and ZFL from it, and save the result back to Excel.")
                    color: Theme.textCaption
                    font.pixelSize: Theme.captionFontSize
                    elide: Text.ElideRight
                }

                HelpButton {
                    page: "calibration-result"
                }
            }

            RowLayout {
                id: sessionBar

                Layout.fillWidth: true
                spacing: Theme.spaceLg

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.labelSpacing

                    Label {
                        text: qsTr("Calibration Folder:")
                        color: Theme.textCaption
                        font.pixelSize: Theme.captionFontSize
                    }

                    TextField {
                        id: caliFolderField

                        Layout.fillWidth: true
                        color: Theme.textPrimary
                        font.pixelSize: Theme.captionFontSize
                        selectByMouse: true
                        text: CalibrationController.folder
                        onEditingFinished: CalibrationController.folder = text
                        placeholderText: qsTr("Folder holding this camera's round Excel files")

                        background: Rectangle {
                            implicitHeight: Theme.controlHeight
                            color: Theme.fieldBackground
                            border.color: caliFolderField.activeFocus ? Theme.accent : Theme.panelBorder
                            radius: Theme.radius
                        }
                    }

                    ActionButton {
                        text: qsTr("Browse...")
                        onClicked: {
                            folderDialog.loadAfterPick = false
                            folderDialog.open()
                            root.browseRequested()
                        }
                    }

                    StatusDot {
                        Layout.alignment: Qt.AlignVCenter
                        Layout.leftMargin: Theme.spaceSm
                        status: root.folderStatus
                    }

                    Label {
                        text: root.folderStatusText
                        color: Theme.textCaption
                        font.pixelSize: Theme.captionFontSize
                    }
                }

                Item { Layout.fillWidth: true }
        

                RowLayout {
                    spacing: Theme.labelSpacing

                    Label {
                        text: qsTr("Calibration System:")
                        color: Theme.textCaption
                        font.pixelSize: Theme.captionFontSize
                    }

                    ComboBox {
                        id: caliSystemCombo

                        Layout.preferredWidth: Theme.controlWidth
                        Layout.preferredHeight: Theme.controlHeight
                        font.pixelSize: Theme.captionFontSize
                        model: CaliSystems.names()

                        // Picking a system applies that rig's pixel sizes and
                        // screen gaps to the table. Only `activated` -- which
                        // fires on an operator choice, not on the model loading
                        // or a programmatic index change -- so opening the
                        // window never silently overwrites fields that came out
                        // of a loaded Excel file.
                        onActivated: (index) => root.applyCaliSystem(index)

                        ToolTip.visible: hovered
                        ToolTip.delay: Theme.animSlow
                        ToolTip.text: qsTr("Applies this rig's pixel sizes and H/V gaps. "
                                         + "Check them in the Parameter tab before computing.")
                    }
                }
            }

            RowLayout {
                id: actionBar

                Layout.fillWidth: true
                spacing: Theme.spaceSm

                component ActionSeparator: Rectangle {
                    Layout.preferredWidth: 1
                    Layout.preferredHeight: Math.round(Theme.controlHeight * 0.6)
                    Layout.leftMargin: Theme.spaceXs
                    Layout.rightMargin: Theme.spaceXs
                    Layout.alignment: Qt.AlignVCenter
                    color: Theme.panelBorder
                }

                ActionButton {
                    text: qsTr("Load All Excel")
                    tone: "accent"
                    enabled: !root.busy
                    onClicked: {
                        if (CalibrationController.folder === "") {
                            folderDialog.loadAfterPick = true
                            folderDialog.open()
                        } else {
                            CalibrationController.loadAllExcel(
                                PatternIo.toFileUrl(CalibrationController.folder))
                        }
                        root.loadAllExcelRequested()
                    }

                    ToolTip.visible: hovered
                    ToolTip.delay: Theme.animSlow
                    ToolTip.text: qsTr("Read every round's Excel file in the calibration folder into rounds 1 to 10 at once.")
                }
                ActionButton {
                    text: qsTr("Load Excel")
                    enabled: !root.busy
                    onClicked: {
                        loadExcelDialog.open()
                        root.loadExcelRequested()
                    }

                    ToolTip.visible: hovered
                    ToolTip.delay: Theme.animSlow
                    ToolTip.text: qsTr("Read one Excel file into the round that is currently selected.")
                }
                ActionButton {
                    text: qsTr("Load Database")
                    enabled: !root.busy
                    onClicked: {
                        folderDialog.loadAfterPick = true
                        folderDialog.open()
                        root.loadDatabaseRequested()
                    }

                    ToolTip.visible: hovered
                    ToolTip.delay: Theme.animSlow
                    ToolTip.text: qsTr("Browse earlier calibration runs and load one into the tables.")
                }

                ActionSeparator {}

                // Fill the current round from the capture, then recompute -- the
                // old client's btn_update_table, restored 2026-09-09.
                //
                // Between the merge and today this button ran compute_all, which
                // is the old client's "Update All Cali Result" -- a DIFFERENT
                // operation that recomputes from data already in the table. The
                // Parameter tab already carries that one under its correct name,
                // so this was a duplicate wearing the missing function's name, and
                // pressing it on an empty round recomputed nothing and looked
                // dead.
                //
                // The window does not do the work: it has no access to the pattern
                // panels, and the nodes belong to ComputeController. Main.qml owns
                // both, so the request goes there -- which is what updateTableRequested
                // was declared for and never used.
                ActionButton {
                    text: qsTr("Update Table")
                    enabled: !root.busy && root.captureReady
                    onClicked: root.updateTableRequested(root.round)

                    ToolTip.visible: hovered
                    ToolTip.delay: Theme.animSlow
                    ToolTip.text: root.captureReady
                        ? qsTr("Measure the crossings in the current pair and fill round %1 "
                             + "with them, plus the PCT from the pattern, then recompute.")
                              .arg(root.round)
                        : qsTr("Needs a positive and a negative shot with a centre on each. "
                             + "Take a pair first.")
                }
                ActionButton {
                    text: qsTr("Save to Excel")
                    enabled: !root.busy
                    onClicked: {
                        saveExcelDialog.selectedFile = "round_" + root.round + ".xlsx"
                        saveExcelDialog.open()
                        root.saveExcelRequested()
                    }

                    ToolTip.visible: hovered
                    ToolTip.delay: Theme.animSlow
                    ToolTip.text: qsTr("Write the round tables back out to Excel in the calibration folder.")
                }
                ActionButton {
                    text: qsTr("Stop")
                    enabled: root.busy
                    onClicked: {
                        CalibrationController.stop()
                        root.stopRequested()
                    }

                    ToolTip.visible: hovered
                    ToolTip.delay: Theme.animSlow
                    ToolTip.text: qsTr("Cancel the load or search that is currently running.")
                }

                Item { Layout.fillWidth: true }

                ActionButton {
                    text: qsTr("Clear Table")
                    tone: "danger"
                    enabled: !root.busy
                    onClicked: {
                        CalibrationController.clearTable(root.round)
                        root.clearTableRequested()
                    }

                    ToolTip.visible: hovered
                    ToolTip.delay: Theme.animSlow
                    ToolTip.text: qsTr("Erase every value in the round that is currently selected.")
                }
                ActionButton {
                    text: qsTr("Clear All Table")
                    tone: "danger"
                    enabled: !root.busy
                    onClicked: {
                        CalibrationController.clearAllTables()
                        root.clearAllTablesRequested()
                    }

                    ToolTip.visible: hovered
                    ToolTip.delay: Theme.animSlow
                    ToolTip.text: qsTr("Erase every value in all eleven rounds, current and 1 to 10.")
                }
            }

            SegmentedControl {
                id: viewSelector

                Layout.alignment: Qt.AlignHCenter
                model: [qsTr("Data"), qsTr("Parameter"), qsTr("Overlap"), qsTr("Aggregation"), qsTr("Graphs")]
                currentIndex: root.viewData
            }

            CaliResultDataPanel {
                id: dataPanel

                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth:  Theme.minCaliViewWidth
                Layout.minimumHeight: Theme.minCaliViewHeight
                visible: root.view === root.viewData

                onCalculateRequested: (round) => CalibrationController.calculateRound(round)
                onAggrRoundRequested: (round) => CalibrationController.aggregationForRound(round)
                onCleanNoiseRequested: (round) => CalibrationController.cleanNoise(round)
            }

            CaliResultParameterPanel {
                id: parameterPanel

                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth:  Theme.minCaliViewWidth
                Layout.minimumHeight: Theme.minCaliViewHeight
                visible: root.view === root.viewParameter

                // Update All recomputes the pipeline and then re-reads the
                // series; the two plot buttons only re-read, because the numbers
                // behind them have not changed unless the table did.
                onUpdateAllRequested: CalibrationController.computeAll()
                onUpdateIhAlphaRequested: CalibrationController.updateSeries()
                onUpdateIhZflRequested: CalibrationController.updateSeries()

                onSaveParametersRequested: {
                    saveJsonDialog.kind = "parameters"
                    saveJsonDialog.selectedFile = "camera_parameters.json"
                    saveJsonDialog.open()
                }
                onSaveConfigurationRequested: {
                    saveJsonDialog.kind = "configuration"
                    saveJsonDialog.selectedFile = "main.json"
                    saveJsonDialog.open()
                }
            }

            // The three ported tabs are pure views: series in as properties,
            // intent out as signals. The ops they map to are the old client's,
            // one for one -- see the comment at the top of each panel.
            CaliResultOverlapPanel {
                id: overlapPanel

                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth:  Theme.minCaliViewWidth
                Layout.minimumHeight: Theme.minCaliViewHeight
                visible: root.view === root.viewOverlap

                zflRounds: CalibrationController.zflRounds
                roundColors: CalibrationController.roundColors
                inspected: CalibrationController.roundPoints
                busy: CalibrationController.busy

                // The samples the Aggregation tab's searches produced. Redraw
                // only -- `updatePlotDistVsAggr()` in the old controller never
                // started a search, and a button saying "update" must not spend
                // minutes on the rig.
                distAggrSamples: CalibrationController.searchSamples

                onUpdateOverlapRequested: CalibrationController.updateSeries()
                onUpdateDistVsAggrRequested: {
                    if (CalibrationController.searchSamples.length === 0)
                        toast.show(qsTr("No search has run yet -- run one from the Aggregation tab."))
                }

                onShowAloneRequested: (round) => CalibrationController.fetchRoundPoints(round)
                onClearInspectedRequested: CalibrationController.fetchRoundPoints(0)
            }

            CaliResultAggregationPanel {
                id: aggregationPanel

                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth:  Theme.minCaliViewWidth
                Layout.minimumHeight: Theme.minCaliViewHeight
                visible: root.view === root.viewAggregation

                searching: CalibrationController.searchRunning
                busy: CalibrationController.busy
                searchStage: CalibrationController.searchStage
                searchSummary: CalibrationController.searchSummary
                searchDone: CalibrationController.searchDone
                searchTotal: CalibrationController.searchTotal

                // Everything the panel converts from IH percent to pixels hangs
                // off this. Zero disables the search buttons rather than letting
                // them run on a window of 0..100 pixels.
                maxIct: CalibrationController.maxIct

                // Stop has to drop the queue as well as unwind the running
                // probe. Calling cancelSearch() alone would land the rig back
                // on the next round a moment later, which reads as a Stop
                // button that does not stop.
                onStopRequested: root.cancelMinByRound()

                // Ports aggrByRangeAndDistance()'s two branches. A Distance in
                // the box scores that distance; an empty one with a target
                // aggregation searches for the distance that hits it.
                onAggrByRangeAndDistanceRequested: {
                    if (aggregationPanel.hasRequestedDistance) {
                        CalibrationController.baseDistance = aggregationPanel.requestedDistance
                        CalibrationController.aggregationAllRounds(
                            true, aggregationPanel.windowXLo, aggregationPanel.windowXHi)
                    } else {
                        CalibrationController.findDistanceForTarget(
                            aggregationPanel.requestedTarget, true,
                            aggregationPanel.windowXLo, aggregationPanel.windowXHi)
                    }
                }

                // Per ROUND, not one pooled search. See root.startMinByRound()
                // for why the distinction is the whole point of this button.
                onMinAggrByIntervalRequested: root.startMinByRound()

                onRangeWindowRequested: toast.show(qsTr("Filled Range_1 to Range_20 from the interval."))

                // No server op behind either of these yet: the old client wrote
                // both to local files.
                onKeepRoundDataRequested: toast.show(qsTr("Keep Round Data is not wired to the rig yet."))
                onSaveHistoryDistanceRequested: toast.show(qsTr("Save Distance History is not wired to the rig yet."))
            }

            CaliResultGraphsPanel {
                id: graphsPanel

                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth:  Theme.minCaliViewWidth
                Layout.minimumHeight: Theme.minCaliViewHeight
                visible: root.view === root.viewGraphs

                // Entirely client-side arithmetic over the range table, exactly
                // as the old tab_graphs was -- no op, no rig.
                ranges: aggregationPanel.ranges
            }
        }
    }

    StatusToast {
        id: toast

        anchors.centerIn: parent
        z: 100
    }

    // The file dialogs stay on the client: picking a path is the operator's, and
    // the operator is sitting at the client. Everything between the path and the
    // numbers is parsing, and that happens on the rig.

    FolderDialog {
        id: folderDialog

        // Browse just records the folder; Load All / Load Database go on to read
        // it. One dialog, because "which folder" is the same question.
        property bool loadAfterPick: false

        title: qsTr("Calibration folder")

        onAccepted: {
            CalibrationController.folder = PatternIo.toLocalPath(folderDialog.selectedFolder)
            if (folderDialog.loadAfterPick)
                CalibrationController.loadAllExcel(folderDialog.selectedFolder)
        }
    }

    FileDialog {
        id: loadExcelDialog

        title: qsTr("Excel file for round %1").arg(root.round)
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("Excel workbooks (*.xlsx)"), qsTr("All files (*)")]

        onAccepted: CalibrationController.loadExcel(root.round, loadExcelDialog.selectedFile)
    }

    FileDialog {
        id: saveExcelDialog

        title: qsTr("Save round %1 to Excel").arg(root.round)
        fileMode: FileDialog.SaveFile
        defaultSuffix: "xlsx"
        nameFilters: [qsTr("Excel workbooks (*.xlsx)"), qsTr("All files (*)")]

        onAccepted: CalibrationController.saveExcel(root.round, saveExcelDialog.selectedFile)
    }

    FileDialog {
        id: saveJsonDialog

        property string kind: "parameters"

        title: saveJsonDialog.kind === "parameters" ? qsTr("Save camera parameters")
                                                    : qsTr("Save calibration configuration")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "json"
        nameFilters: [qsTr("JSON files (*.json)"), qsTr("All files (*)")]

        onAccepted: {
            if (saveJsonDialog.kind === "parameters")
                root.writeJson(saveJsonDialog.selectedFile, root.parameterDocument(),
                               qsTr("camera parameters"))
            else
                root.writeJson(saveJsonDialog.selectedFile, root.configurationDocument(),
                               qsTr("configuration"))
        }
    }

    // ---- Min Aggregation by Interval: one minimum PER ROUND ----------------
    //
    // Restored 2026-09-09, after the merge with origin/v2.1_2026_New-UI-CPP-ROS
    // dropped it. That merge rebuilt this tab as the 20-range workspace and
    // pointed this button at `findMinInWindow`, which is a different op with a
    // different meaning, and the substitution is invisible in the UI:
    //
    //   findMinInWindow      -> find_min_aggregation_in_window
    //                           ONE minimum, over every enabled round pooled
    //   findMinForRound      -> find_min_aggr_single_round
    //                           each round's OWN minimum, one search per round
    //
    // The pooled number cannot answer the question this button is for. A single
    // bad round -- shot at the wrong distance, or against a stale pattern --
    // barely moves the pooled minimum, and shows up only as its own minimum
    // landing somewhere the others did not. That is the whole reason the old
    // client looped the rounds instead of asking once, and it is why
    // docs/MISSING_CONTROLS.md lists this exact swap as a semantic trap.
    //
    // The distance range is 1..500, matching the old client's
    // find_min_aggr_single_round(i, 1, 500). Deliberately NOT the panel's
    // interval fields: those are an ICT window in PIXELS, converted from IH
    // percent, and feeding an ICT window in as a distance range would be the
    // same class of unit error -- accepted without complaint, wrong by a factor
    // nobody can see in the output.
    readonly property real minByRoundDistMin: 1
    readonly property real minByRoundDistMax: 500

    property var minByRoundQueue: []
    property var minByRoundResults: []
    property bool minByRoundActive: false

    // searchChanged also fires for progress, so "finished" cannot be read from
    // one edge of searchRunning. We only treat a round as done once we have
    // seen it actually running and then stop.
    property bool minByRoundSawRunning: false
    property int minByRoundCurrent: 0

    // A round with no ICT anywhere is not worth a search -- the rig would spend
    // a full ternary descent to report a minimum of nothing. The old client
    // skipped these too.
    function roundHasData(round) {
        const table = CalibrationController.rounds[round]
        if (!table)
            return false
        for (let i = 0; i < table.length; ++i) {
            const v = table[i].ictAvg
            if (v !== undefined && v !== "" && parseFloat(v) !== 0)
                return true
        }
        return false
    }

    function startMinByRound() {
        if (root.minByRoundActive)
            return

        const queue = []
        const skippedOff = []
        const skippedEmpty = []
        for (let r = 1; r <= 10; ++r) {
            if (CalibrationController.roundEnabled[r] === false) {
                skippedOff.push(r)
                continue
            }
            if (!root.roundHasData(r)) {
                skippedEmpty.push(r)
                continue
            }
            queue.push(r)
        }

        if (queue.length === 0) {
            toast.show(qsTr("No round has data to search -- every round is either "
                          + "switched off or empty."), true)
            return
        }

        root.minByRoundQueue = queue
        root.minByRoundResults = []
        root.minByRoundActive = true

        let note = qsTr("Searching %1 round(s), one minimum each.").arg(queue.length)
        if (skippedOff.length > 0)
            note += " " + qsTr("Skipped %1 switched off.").arg(skippedOff.join(", "))
        if (skippedEmpty.length > 0)
            note += " " + qsTr("Skipped %1 empty.").arg(skippedEmpty.join(", "))
        toast.show(note, false)

        root.runNextMinByRound()
    }

    function runNextMinByRound() {
        if (!root.minByRoundActive)
            return

        if (root.minByRoundQueue.length === 0) {
            root.finishMinByRound(false)
            return
        }

        const next = root.minByRoundQueue.slice()
        root.minByRoundCurrent = next.shift()
        root.minByRoundQueue = next
        root.minByRoundSawRunning = false

        CalibrationController.findMinForRound(root.minByRoundCurrent,
                                              root.minByRoundDistMin,
                                              root.minByRoundDistMax)
    }

    function cancelMinByRound() {
        // Always unwind the rig, whether or not a sequence is running -- this is
        // the tab's only Stop and the other two searches go through it as well.
        CalibrationController.cancelSearch()

        if (!root.minByRoundActive)
            return

        root.minByRoundQueue = []
        root.finishMinByRound(true)
    }

    function finishMinByRound(cancelled) {
        root.minByRoundActive = false
        root.minByRoundSawRunning = false

        const done = root.minByRoundResults
        if (done.length === 0) {
            toast.show(cancelled ? qsTr("Stopped before any round finished.")
                                 : qsTr("No round produced a minimum."), true)
            return
        }

        const parts = []
        for (let i = 0; i < done.length; ++i)
            parts.push(qsTr("R%1: %2").arg(done[i].round).arg(done[i].distance.toFixed(1)))

        // Partial is reported as partial. A cancelled search produced a real
        // answer for the rounds it got through; saying otherwise would throw
        // away work the operator paid rig time for.
        toast.show((cancelled ? qsTr("Stopped after %1 round(s) -- ")
                              : qsTr("Best distance per round -- ")).arg(done.length)
                   + parts.join(", "), cancelled)
    }

    Connections {
        target: CalibrationController

        function onErrorRaised(message) {
            toast.show(message, true)
            // A refused or failed op ends the sequence rather than marching the
            // rig through nine more rounds that will fail the same way.
            if (root.minByRoundActive) {
                root.minByRoundQueue = []
                root.finishMinByRound(true)
            }
        }

        function onNotice(message) {
            toast.show(message, false)
        }

        function onSearchChanged() {
            if (!root.minByRoundActive)
                return

            if (CalibrationController.searchRunning) {
                root.minByRoundSawRunning = true
                return
            }

            if (!root.minByRoundSawRunning)
                return

            root.minByRoundSawRunning = false
            root.minByRoundResults = root.minByRoundResults.concat([{
                round: root.minByRoundCurrent,
                distance: CalibrationController.bestDistance,
                aggregation: CalibrationController.bestAggregation
            }])

            root.runNextMinByRound()
        }
    }
}
