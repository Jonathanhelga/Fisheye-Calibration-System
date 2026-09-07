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

    readonly property real patternPanelWidth: Math.max(concentricPanel.minimumWidth,
                                                       stripelinePanel.minimumWidth,
                                                       chessboardPanel.minimumWidth)
    readonly property real patternPanelHeight: Math.max(concentricPanel.minimumHeight,
                                                        stripelinePanel.minimumHeight,
                                                        chessboardPanel.minimumHeight)

    signal fourSideBrowseRequested()
    signal showNumbersRequested()
    signal applyMappingRequested(int top, int north, int west, int south, int east)

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

    // Auto Update lives in each panel, as an AutoRefresh -- see that control. It
    // is not driven from here: the panel owns the fingerprint of its own spec,
    // and a window-level scheduler would have to reach into three different
    // panels to build one.

    function showOnMonitor(target, direction) {
        PatternController.showOnMonitor(direction, JSON.stringify(target.specJson()))
    }

    function saveImage(target) {
        saveImageDialog.target = target
        saveImageDialog.selectedFile = target.patternType + ".png"
        saveImageDialog.open()
    }

    function turnOffFourSides() {
        monitorViewer.turnOffFourSides()
    }

    function applyImageToFourSides() {
        monitorViewer.applyImageToFourSides(fourSidePathField.text)
    }

    // Render both polarities once and keep them on the rig, which is what makes a
    // Pos/Neg/Pair shot a lookup and a blit instead of five re-renders per shot.
    //
    // The polarity swap is NOT done here. It is the rule that odd layers take the
    // positive colour and even layers the negative one -- arithmetic over the
    // operator's design, and therefore the server's job. Sending the two colours
    // and letting it invert them is what keeps there from being a second
    // implementation of that rule.
    function preparePatterns() {
        MonitorController.preparePatterns(
            JSON.stringify(concentricPanel.specJson()),
            JSON.stringify(stripelinePanel.specJson()),
            concentricPanel.positiveRgb(),
            concentricPanel.negativeRgb())
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

                        StatusDot {
                            Layout.alignment: Qt.AlignVCenter
                            status: MonitorController.status
                        }

                        Label {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            Layout.preferredWidth: 0
                            text: MonitorController.lastError !== "" ? MonitorController.lastError
                                : MonitorController.screensSummary !== ""
                                    ? MonitorController.screensSummary
                                    : qsTr("Monitor node not queried yet")
                            color: MonitorController.lastError !== "" ? Theme.danger
                                                                     : Theme.textCaption
                            font.pixelSize: Theme.captionFontSize
                            elide: Text.ElideRight
                        }

                        ActionButton {
                            text: qsTr("Prepare Patterns")
                            tone: MonitorController.preparedReady ? "accent" : "danger"
                            enabled: MonitorController.status === ProbeStatus.Ok
                                     && !MonitorController.busy
                            onClicked: root.preparePatterns()

                            ToolTip.visible: hovered
                            ToolTip.delay: Theme.animSlow
                            ToolTip.text: MonitorController.preparedReady
                                ? qsTr("Re-render both polarities on the rig. Do this after any "
                                     + "pattern edit -- a shot taken against a stale prepared "
                                     + "pattern looks exactly like a good one.")
                                : qsTr("Required before Pos / Neg / Pair Shot will work. Renders "
                                     + "the positive and negative patterns once and keeps them "
                                     + "on the rig.")
                        }

                        ActionButton {
                            text: qsTr("Setup Monitor Direction")
                            onClicked: {
                                MonitorController.describeScreens()
                                directionDialog.open()
                            }

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

                        ActionButton {
                            text: qsTr("Reconnect")
                            // describeScreens runs itself once the link comes
                            // up (see MonitorController::applyLink), so asking
                            // for it here would only be answered by an early
                            // return while the status is still Checking.
                            onClicked: MonitorController.reconnect()

                            ToolTip.visible: hovered
                            ToolTip.delay: Theme.animSlow
                            ToolTip.text: qsTr("Tear down and re-open the monitor link on the "
                                             + "same domain, then re-read which screens the "
                                             + "server can see.")
                        }
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
                                text: qsTr("Apply image to N/W/S/E:")
                                color: Theme.textCaption
                                font.pixelSize: Theme.captionFontSize
                            }

                            TextField {
                                id: fourSidePathField
                                Layout.fillWidth: true
                                color: Theme.textPrimary
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
                                onClicked: fourSideDialog.open()
                            }

                            ActionButton {
                                text: qsTr("Apply to 4 Sides")
                                tone: "accent"
                                onClicked: root.applyImageToFourSides()
                            }
                        }

                        DpadMonitorViewer {
                            id: monitorViewer
                            Layout.fillWidth: true
                            Layout.fillHeight: true
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

            const text = PatternIo.readText(source)
            if (text.length === 0) {
                toast.show(qsTr("Import failed: %1").arg(PatternIo.lastError), true)
                return
            }

            let doc = null
            try {
                doc = JSON.parse(text)
            } catch (error) {
                toast.show(qsTr("Import failed: %1 is not valid JSON").arg(name), true)
                return
            }

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
        id: fourSideDialog

        title: qsTr("Image for N / W / S / E")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("Images (*.png *.jpg *.jpeg *.bmp)"), qsTr("All files (*)")]

        Component.onCompleted: fourSideDialog.currentFolder = PatternIo.defaultImageDirectory

        onAccepted: {
            fourSidePathField.text = PatternIo.toLocalPath(fourSideDialog.selectedFile)
            root.fourSideBrowseRequested()
        }
    }

    Connections {
        target: MonitorController

        function onErrorRaised(message) {
            toast.show(message, true)
        }

        function onNotice(message) {
            toast.show(message, false)
        }
    }

    Connections {
        target: PatternController

        function onErrorRaised(message) {
            toast.show(message, true)
        }

        function onPatternShown(direction, width, height) {
            toast.show(width > 0 && height > 0
                       ? qsTr("Pattern shown on %1 at %2 x %3")
                             .arg(direction.toUpperCase()).arg(width).arg(height)
                       : qsTr("Pattern shown on %1").arg(direction.toUpperCase()),
                       false)
        }
    }

    MonitorDirectionDialog {
        id: directionDialog
        anchors.centerIn: parent

        // What the SERVER thinks it has, which is the question this dialog
        // exists to answer -- the operator's own machine's screens are a
        // different set entirely.
        statusText: MonitorController.screensSummary

        onShowNumbersRequested: {
            MonitorController.showDisplayNumbers()
            root.showNumbersRequested()
        }
        // The display numbers travel as TEXT, as the old HTTP API spelled them.
        onApplyMappingRequested: (top, north, west, south, east) => {
            MonitorController.setDisplayDirection(String(top), String(north), String(west),
                                                  String(south), String(east))
            root.applyMappingRequested(top, north, west, south, east)
        }
    }
}
