import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import FisheyeCaliJojo

Window {
    id: root

    title: qsTr("PCT Control Panel")

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

    property int concentricMode: 0
    property int stripelineMode: 1
    property int chessboardMode: 2
    property alias mode: patternSelector.currentIndex

    property alias concentric: concentricPanel
    property alias stripeline:  stripelinePanel
    property alias chessboard: chessboardPanel

    property url fourSideUrl
    property string fourSideType: ""

    property var preparedSpecs: ({})

    readonly property real patternPanelWidth: Math.max(concentricPanel.minimumWidth,
                                                       stripelinePanel.minimumWidth,
                                                       chessboardPanel.minimumWidth)
    readonly property real patternPanelHeight: Math.max(concentricPanel.minimumHeight,
                                                        stripelinePanel.minimumHeight,
                                                        chessboardPanel.minimumHeight)

    function importPattern(target) {
        importDialog.target = target
        importDialog.open()
    }

    function exportPattern(target) {
        exportDialog.target = target
        exportDialog.selectedFile = target.patternType + ".json"
        exportDialog.open()
    }

    function renderPreview(target) {
        PatternController.renderPreview(target.patternType,
                                        JSON.stringify(target.specJson()),
                                        target.resolutionW,
                                        target.resolutionH)
    }

    function preparePattern(target) {
        const concentric = target === concentricPanel
        if (!concentric && target !== stripelinePanel) return

        const spec = target.specJson()
        if (!spec.layers || spec.layers.length === 0) return

        const specText = JSON.stringify(spec)
        const payload = [specText, String(target.positiveColor), String(target.negativeColor)].join("|")
        if (root.preparedSpecs[target.patternType] === payload) return

        root.preparedSpecs[target.patternType] = payload
        PatternController.preparePatterns(concentric ? specText : "",
                                          concentric ? "" : specText,
                                          target.positiveColor,
                                          target.negativeColor)
    }

    function showOnMonitor(target, direction) {
        root.preparePattern(target)
        PatternController.showOnMonitor(direction, JSON.stringify(target.specJson()))
    }

    function readPatternDoc(fileUrl) {
        const name = decodeURIComponent(String(fileUrl).split("/").pop())

        const text = PatternIo.readText(fileUrl)
        if (text.length === 0) {
            toast.show(qsTr("Load failed: %1").arg(PatternIo.lastError), true)
            return null
        }

        try {
            return JSON.parse(text)
        } catch (error) {
            toast.show(qsTr("Load failed: %1 is not valid JSON").arg(name), true)
            return null
        }
    }

    function panelForType(patternType) {
        if (patternType === concentricPanel.patternType) return concentricPanel
        if (patternType === stripelinePanel.patternType) return stripelinePanel
        return null
    }

    function loadPatternFile(fileUrl) {
        const name = decodeURIComponent(String(fileUrl).split("/").pop())

        const doc = root.readPatternDoc(fileUrl)
        if (!doc) return null

        const panel = root.panelForType(String(doc["pattern type"]))
        if (!panel || !panel.loadConfig(doc)) {
            toast.show(qsTr("%1 is not a concentric or stripeline pattern file").arg(name), true)
            return null
        }

        root.mode = panel === concentricPanel ? root.concentricMode : root.stripelineMode
        toast.show(qsTr("Loaded %1").arg(name), false)
        return panel
    }

    function saveImage(target) {
        saveImageDialog.target = target
        saveImageDialog.selectedFile = target.patternType + ".png"
        saveImageDialog.open()
    }

    function turnOffFourSides() {
        monitorViewer.turnOffFourSides()
    }

    function applyPatternToFourSides() {
        if (!root.fourSideType) {
            toast.show(qsTr("Browse a pattern JSON for N / W / S / E first"), true)
            return
        }

        monitorViewer.applyPatternToFourSides(root.fourSideUrl, root.fourSideType)
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
                Layout.minimumHeight: header.implicitHeight
                spacing: Theme.rowSpacing

                Label {
                    text: qsTr("PCT Control Panel")
                    font.bold: true
                    color: Theme.accent
                }
                Label {
                    Layout.fillWidth: true
                    text: qsTr("Build, colour, and push calibration patterns to the TOP / N / W / S / E screens, and control monitor image and brightness.")
                    color: Theme.textCaption
                    font.pixelSize: Theme.captionFontSize
                    elide: Text.ElideRight
                }
            }

            RowLayout {
                id: workRow

                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: Theme.spaceMd

                readonly property real free: Math.max(0, fit.canvasWidth - 3 * Theme.spaceMd)

                ColumnLayout {
                    id: patternColumn

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredWidth: workRow.free * Theme.ratioPatternColumn
                    Layout.minimumWidth: Math.max(Theme.minPatternColumn, root.patternPanelWidth)
                    Layout.minimumHeight: patternSelector.implicitHeight
                                          + Theme.spaceMd
                                          + root.patternPanelHeight
                    spacing: Theme.spaceMd

                    SegmentedControl {
                        id: patternSelector
                        Layout.fillWidth: true
                        stretch: true
                        model: [qsTr("Concentric"), qsTr("Stripeline"), qsTr("Chessboard")]
                        currentIndex: root.concentricMode
                    }
                    ConcentricPanel {
                        id: concentricPanel
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        visible: root.mode === root.concentricMode

                        connected: PatternController.status === ProbeStatus.Ok
                        previewSource: PatternController.previewUrls[concentricPanel.patternType] || ""

                        onImportRequested: root.importPattern(concentricPanel)
                        onExportRequested: root.exportPattern(concentricPanel)
                        onSaveImageRequested: root.saveImage(concentricPanel)
                        onUpdateRequested: root.renderPreview(concentricPanel)
                        onShowRequested: (direction) => root.showOnMonitor(concentricPanel, direction)
                    }
                    StripelinePanel {
                        id: stripelinePanel
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        visible: root.mode === root.stripelineMode

                        connected: PatternController.status === ProbeStatus.Ok
                        previewSource: PatternController.previewUrls[stripelinePanel.patternType] || ""

                        onImportRequested: root.importPattern(stripelinePanel)
                        onExportRequested: root.exportPattern(stripelinePanel)
                        onSaveImageRequested: root.saveImage(stripelinePanel)
                        onUpdateRequested: root.renderPreview(stripelinePanel)
                        onShowRequested: (direction) => root.showOnMonitor(stripelinePanel, direction)
                    }
                    ChessboardPanel {
                        id: chessboardPanel
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        visible: root.mode === root.chessboardMode

                        connected: PatternController.status === ProbeStatus.Ok
                        previewSource: PatternController.previewUrls[chessboardPanel.patternType] || ""

                        onSaveImageRequested: root.saveImage(chessboardPanel)
                        onUpdateRequested: root.renderPreview(chessboardPanel)
                        onShowRequested: (direction) => root.showOnMonitor(chessboardPanel, direction)
                    }
                }

                ColumnLayout {
                    id: monitorColumn

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredWidth: workRow.free * Theme.ratioMonitorColumn
                    Layout.minimumWidth: Math.max(Theme.minMonitorColumn,
                                                  monitorToolRow.implicitWidth,
                                                  monitorFrame.Layout.minimumWidth)
                    Layout.minimumHeight: monitorToolRow.implicitHeight
                                          + Theme.spaceMd
                                          + monitorFrame.Layout.minimumHeight
                    spacing: Theme.spaceMd

                    RowLayout {
                        id: monitorToolRow

                        Layout.fillWidth: true
                        spacing: Theme.rowSpacing

                        Item { Layout.fillWidth: true }

                        ActionButton {
                            text: qsTr("Setup Monitor Direction")
                            onClicked: directionDialog.open()

                            ToolTip.visible: hovered
                            ToolTip.delay: Theme.animSlow
                            ToolTip.text: qsTr("Map each physical display to TOP / N / W / S / E. Required once before patterns will show.")
                        }

                        ActionButton {
                            tone: "danger"
                            text: qsTr("4-Side Off (N/W/S/E)")
                            onClicked: root.turnOffFourSides()

                            ToolTip.visible: hovered
                            ToolTip.delay: Theme.animSlow
                            ToolTip.text: qsTr("Turns off North, West, South, and East together. TOP is left as is.")
                        }

                        Item { Layout.fillWidth: true }
                    }

                    SectionFrame {
                        id: monitorFrame

                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.minimumWidth: monitorViewer.Layout.minimumWidth
                                             + monitorFrame.leftPadding + monitorFrame.rightPadding
                        Layout.minimumHeight: pathRow.implicitHeight
                                              + monitorFrame.spacing
                                              + monitorViewer.Layout.minimumHeight
                                              + monitorFrame.topPadding + monitorFrame.bottomPadding

                        RowLayout {
                            id: pathRow

                            Layout.fillWidth: true
                            spacing: Theme.labelSpacing

                            Label {
                                text: qsTr("Apply pattern to N/W/S/E:")
                                color: Theme.textCaption
                                font.pixelSize: Theme.captionFontSize
                            }

                            TextField {
                                id: fourSidePathField
                                Layout.fillWidth: true
                                readOnly: true
                                text: PatternIo.localPath(root.fourSideUrl)
                                placeholderText: qsTr("No pattern file loaded")
                                color: Theme.textPrimary
                                placeholderTextColor: Theme.textDisabled
                                font.pixelSize: Theme.captionFontSize
                                selectByMouse: true

                                background: Rectangle {
                                    implicitHeight: Theme.controlHeight
                                    color: Theme.fieldBackground
                                    border.color: fourSidePathField.activeFocus ? Theme.accent : Theme.panelBorder
                                    radius: Theme.radius
                                }
                            }

                            ActionButton {
                                text: qsTr("Browse...")
                                onClicked: fourSidePatternDialog.open()
                            }

                            ActionButton {
                                text: qsTr("Apply to 4 Sides")
                                tone: "accent"
                                onClicked: root.applyPatternToFourSides()
                            }
                        }

                        DpadMonitorViewer {
                            id: monitorViewer
                            Layout.fillWidth: true
                            Layout.fillHeight: true

                            onPatternFileChosen: (slot, fileUrl) => {
                                const panel = root.loadPatternFile(fileUrl)
                                if (!panel) return

                                slot.configUrl = fileUrl
                                slot.patternType = panel.patternType
                            }

                            onPatternPushRequested: (slot) => {
                                const panel = root.panelForType(slot.patternType)
                                if (panel) root.showOnMonitor(panel, slot.direction)
                            }
                        }
                    }
                }
            }
        }
    }

    StatusToast {
        id: toast


        anchors.centerIn: parent
        z: 100
    }

    FileDialog {
        id: importDialog

        property var target: null

        title: qsTr("Import %1 JSON").arg(importDialog.target ? importDialog.target.patternType : "")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("JSON files (*.json)"), qsTr("All files (*)")]

        Component.onCompleted: importDialog.currentFolder = PatternIo.defaultDirectory

        onAccepted: {
            const source = importDialog.selectedFile
            const name = decodeURIComponent(String(source).split("/").pop())

            const doc = root.readPatternDoc(source)
            if (!doc) return

            if (!importDialog.target.loadConfig(doc)) {
                toast.show(qsTr("Import failed: %1 is not a %2 pattern file")
                           .arg(name).arg(importDialog.target.patternType), true)
                return
            }

            console.log("[Pattern And Monitor] loaded " + source)
            toast.show(qsTr("Imported %1").arg(name), false)
        }
    }

    FileDialog {
        id: exportDialog

        property var target: null

        title: qsTr("Export %1 JSON").arg(exportDialog.target ? exportDialog.target.patternType : "")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "json"
        nameFilters: [qsTr("JSON files (*.json)"), qsTr("All files (*)")]

        Component.onCompleted: exportDialog.currentFolder = PatternIo.defaultDirectory

        onAccepted: {
            const destination = exportDialog.selectedFile
            const name = decodeURIComponent(String(destination).split("/").pop())
            const text = JSON.stringify(exportDialog.target.configJson(), null, 2)

            if (PatternIo.writeText(destination, text)) {
                console.log("[Pattern And Monitor] wrote " + destination)
                toast.show(qsTr("Exported to %1").arg(name), false)
            } else {
                console.log("[Pattern And Monitor] export failed, " + PatternIo.lastError)
                toast.show(qsTr("Export failed: %1").arg(PatternIo.lastError), true)
            }
        }
    }

    FileDialog {
        id: saveImageDialog

        property var target: null

        title: qsTr("Save %1 image").arg(saveImageDialog.target ? saveImageDialog.target.patternType : "")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "png"
        nameFilters: [qsTr("PNG images (*.png)"), qsTr("All files (*)")]

        Component.onCompleted: saveImageDialog.currentFolder = PatternIo.defaultImageDirectory

        onAccepted: {
            const destination = saveImageDialog.selectedFile
            const name = decodeURIComponent(String(destination).split("/").pop())

            if (PatternController.savePreview(saveImageDialog.target.patternType, destination)) {
                console.log("[Pattern And Monitor] wrote " + destination)
                toast.show(qsTr("Saved image to %1").arg(name), false)
            } else {
                console.log("[Pattern And Monitor] save image failed, " + PatternController.lastError)
                toast.show(qsTr("Save Image failed: %1").arg(PatternController.lastError), true)
            }
        }
    }

    FileDialog {
        id: fourSidePatternDialog

        title: qsTr("Choose pattern JSON for N / W / S / E")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("Pattern JSON (*.json)"), qsTr("All files (*)")]

        Component.onCompleted: fourSidePatternDialog.currentFolder = PatternIo.defaultDirectory

        onAccepted: {
            const panel = root.loadPatternFile(fourSidePatternDialog.selectedFile)
            if (!panel) return

            root.fourSideUrl = fourSidePatternDialog.selectedFile
            root.fourSideType = panel.patternType
            console.log("[Pattern And Monitor] 4-Side pattern picked " + root.fourSideUrl)
        }
    }

    Connections {
        target: PatternController

        function onErrorRaised(message) {
            toast.show(message, true)
        }

        function onStatusChanged() {
            root.preparedSpecs = ({})
        }

        function onPatternsPrepared(ok, prepared, directory, message) {
            if (ok) return

            root.preparedSpecs = ({})
            toast.show(qsTr("Pos Shot and Neg Shot will refuse to fire: %1").arg(message), true)
        }

        function onPatternShown(direction, width, height) {
            toast.show(width > 0 && height > 0
                       ? qsTr("Image sent to %1 at %2 x %3")
                             .arg(direction.toUpperCase()).arg(width).arg(height)
                       : qsTr("Image sent to %1").arg(direction.toUpperCase()),
                       false)
        }
    }

    MonitorDirectionDialog {
        id: directionDialog
        anchors.centerIn: parent
    }
}
