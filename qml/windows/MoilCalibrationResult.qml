import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import FisheyeCaliJojo
import "../panels/CaliSystems.js" as CaliSystems

Window {
    id: root

    title: qsTr("MOIL Calibration Result")
    width:  Theme.designWidth
    height: Theme.designHeight

    Component.onCompleted: {
        width  = Math.min(Theme.designWidth,  Screen.desktopAvailableWidth)
        height = Math.min(Theme.designHeight, Screen.desktopAvailableHeight)
    }

    // All of it belongs to the controller. The window is the form; the table, the
    // load state and the option flags live where the ops that use them live.
    readonly property bool busy: CalibrationController.busy
    readonly property int loadStatus: CalibrationController.loadStatus

    property bool singleDistance: CalibrationController.singleDistance
    onSingleDistanceChanged: CalibrationController.singleDistance = root.singleDistance

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
        
                PatternToggleSwitch {
                    text: qsTr("Single Distance")
                    checked: root.singleDistance
                    onToggled: (value) => root.singleDistance = value
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
                visible: root.view === root.viewData

                onCalculateRequested: (round) => CalibrationController.calculateRound(round)
                onAggrRoundRequested: (round) => CalibrationController.aggregationForRound(round)
                onCleanNoiseRequested: (round) => CalibrationController.cleanNoise(round)
            }

            CaliResultParameterPanel {
                id: parameterPanel

                Layout.fillWidth: true
                Layout.fillHeight: true
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

            CaliResultOverlapPanel {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: root.view === root.viewOverlap
            }

            CaliResultAggregationPanel {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: root.view === root.viewAggregation
            }

            CaliResultGraphsPanel {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: root.view === root.viewGraphs
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

    Connections {
        target: CalibrationController

        function onErrorRaised(message) {
            toast.show(message, true)
        }

        function onNotice(message) {
            toast.show(message, false)
        }
    }
}
