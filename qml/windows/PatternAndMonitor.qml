import QtQuick
import QtQuick.Controls
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
    property int striplineMode: 1
    property int chessboardMode: 2
    property alias mode: patternSelector.currentIndex

    property alias concentric: concentricPanel
    property alias stripline:  striplinePanel
    property alias chessboard: chessboardPanel

    readonly property real patternPanelWidth: Math.max(concentricPanel.minimumWidth,
                                                       striplinePanel.minimumWidth,
                                                       chessboardPanel.minimumWidth)
    readonly property real patternPanelHeight: Math.max(concentricPanel.minimumHeight,
                                                        striplinePanel.minimumHeight,
                                                        chessboardPanel.minimumHeight)

    signal fourSideBrowseRequested()
    signal showNumbersRequested()
    signal applyMappingRequested(int top, int north, int west, int south, int east)

    function turnOffFourSides() {
        monitorViewer.turnOffFourSides()
    }

    function applyImageToFourSides() {
        monitorViewer.applyImageToFourSides(fourSidePathField.text)
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
                        model: [qsTr("Concentric"), qsTr("Stripline"), qsTr("Chessboard")]
                        currentIndex: root.concentricMode
                    }
                    ConcentricPanel {
                        id: concentricPanel
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        visible: root.mode === root.concentricMode

                        onImportRequested: console.log("[Pattern And Monitor] Concentric: Import JSON requested")
                        onExportRequested: console.log("[Pattern And Monitor] Concentric: Export JSON requested")
                        onSaveImageRequested: console.log("[Pattern And Monitor] Concentric: Save Image requested")
                        onUpdateRequested: (direction) => console.log(
                            "[Pattern And Monitor] Concentric: Update requested, direction=" + direction)
                    }
                    StriplinePanel {
                        id: striplinePanel
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        visible: root.mode === root.striplineMode

                        onImportRequested: console.log("[Pattern And Monitor] Stripline: Import JSON requested")
                        onExportRequested: console.log("[Pattern And Monitor] Stripline: Export JSON requested")
                        onSaveImageRequested: console.log("[Pattern And Monitor] Stripline: Save Image requested")
                        onUpdateRequested: (direction) => console.log(
                            "[Pattern And Monitor] Stripline: Update requested, direction=" + direction)
                    }
                    ChessboardPanel {
                        id: chessboardPanel
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        visible: root.mode === root.chessboardMode

                        onGenerateRequested: console.log("[Pattern And Monitor] Chessboard: Generate requested")
                        onSaveImageRequested: console.log("[Pattern And Monitor] Chessboard: Save Image requested")
                        onUpdateRequested: (direction) => console.log(
                            "[Pattern And Monitor] Chessboard: Update requested, direction=" + direction)
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

                        ActionButton {
                            text: qsTr("Reconnect")

                            ToolTip.visible: hovered
                            ToolTip.delay: Theme.animSlow
                            ToolTip.text: qsTr("Reconnect")
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
                                onClicked: {
                                    console.log("[Monitor Viewer] 4-Side: Browse requested")
                                    root.fourSideBrowseRequested()
                                }
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

    MonitorDirectionDialog {
        id: directionDialog
        anchors.centerIn: parent

        onShowNumbersRequested: {
            console.log("[Pattern And Monitor] Show Numbers on Screens requested")
            root.showNumbersRequested()
        }
        onApplyMappingRequested: (top, north, west, south, east) => {
            console.log("[Pattern And Monitor] Apply Mapping requested: top=" + top + " n=" + north + " w=" + west + " s=" + south + " e=" + east)
            root.applyMappingRequested(top, north, west, south, east)
        }
    }
}
