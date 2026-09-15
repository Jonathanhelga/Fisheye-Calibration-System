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

    // All state belongs to the controller.
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

    signal browseRequested()
    signal loadAllExcelRequested()
    signal loadExcelRequested()
    signal saveExcelRequested()
    // Bound from Main.qml, which owns both.
    property bool captureReady: false

    signal updateTableRequested(int round)
    signal loadDatabaseRequested()
    signal stopRequested()
    signal clearTableRequested()
    signal clearAllTablesRequested()

    // The camera parameters as the toolchain's .json.
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
        // Only rounds with their own distance.
        const own = {}
        const distances = CalibrationController.roundDistances
        for (let r = 0; r < distances.length; ++r)
            if (distances[r].own !== "")
                own[String(r)] = parseFloat(distances[r].own)

        return {
            "calibration_system": caliSystemCombo.currentText,
            "distance_per_round": CalibrationController.distanceStep,
            "base_distance": CalibrationController.baseDistance,
            "manual_round_distance": CalibrationController.manualRoundDistance,
            "round_distances": own
        }
    }

    // Push a rig profile through setField.
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

                        // activated only, so opening never overwrites fields.
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

                // Fill from capture, then recompute. Main.qml does it.
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

                // Update All recomputes; the plot buttons only redraw.
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

            // Pure views: series in, signals out.
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

                // Redraw only; never start a search here.
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

                // Zero disables the searches.
                maxIct: CalibrationController.maxIct

                // Stop drops the queue as well.
                onStopRequested: root.cancelMinByRound()

                // Distance scores it; blank searches for it.
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

                // Per round, not one pooled search.
                onMinAggrByIntervalRequested: root.startMinByRound()

                onRangeWindowRequested: toast.show(qsTr("Filled Range_1 to Range_20 from the interval."))

                // No server op behind either yet.
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

                // Client-side arithmetic over the range table.
                ranges: aggregationPanel.ranges
            }
        }
    }

    StatusToast {
        id: toast

        anchors.centerIn: parent
        z: 100
    }

    // The file dialog stays on the client.

    FolderDialog {
        id: folderDialog

        // One dialog; Load All reads the folder.
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

    // ---- Min Aggregation by Interval ----
    // One minimum per round, over 1..500.
    readonly property real minByRoundDistMin: 1
    readonly property real minByRoundDistMax: 500

    property var minByRoundQueue: []
    property var minByRoundResults: []
    property bool minByRoundActive: false

    // searchChanged also fires for progress.
    property bool minByRoundSawRunning: false
    property int minByRoundCurrent: 0

    // Skip a round with no ICT.
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
        // Always unwind the rig.
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

        // Partial is reported as partial.
        // Off, the stored results are not used.
        const unused = CalibrationController.manualRoundDistance
                       ? "" : " " + qsTr("Saved as each round's own distance; turn on "
                                         + "Per-round manual set distance to use them.")
        toast.show((cancelled ? qsTr("Stopped after %1 round(s) -- ")
                              : qsTr("Best distance per round -- ")).arg(done.length)
                   + parts.join(", ") + unused, cancelled)
    }

    Connections {
        target: CalibrationController

        function onErrorRaised(message) {
            toast.show(message, true)
            // A failure ends the sequence.
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
