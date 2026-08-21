import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FisheyeCaliJojo

Window {
    id: root

    title: qsTr("Pattern Generator & Monitor Viewer")
    width:  Screen.desktopAvailableWidth
    height: Screen.desktopAvailableHeight

    property int concentricMode: 0
    property int striplineMode: 1
    property int chessboardMode: 2
    property alias mode: patternSelector.currentIndex

    readonly property real ratioPatternGenerator: 0.3
    readonly property real ratioMonitorViewer: 0.7

    property alias concentric: concentricPanel
    property alias stripline:  striplinePanel
    property alias chessboard: chessboardPanel

    signal fourSideBrowseRequested()
    signal showNumbersRequested()
    signal applyMappingRequested(int top, int north, int west, int south, int east)

    function turnOffFourSides() {
        monitorViewer.turnOffFourSides()
    }

    function applyImageToFourSides() {
        monitorViewer.applyImageToFourSides(fourSidePathField.text)
    }

    RowLayout{
        id: patternGenerator

        anchors.fill: parent
        anchors.margins: Theme.spaceMd
        spacing: Theme.spaceMd

        readonly property real free: Math.max(0, root.width - 2 * Theme.spaceMd)

        ColumnLayout{
            Layout.fillHeight: true
            Layout.preferredWidth: patternGenerator.free * root.ratioPatternGenerator

            RowLayout{
                Layout.fillWidth: true
                spacing: Theme.rowSpacing
                Label {
                    text: "PCT Pattern Generator"
                    font.bold: true
                    color: Theme.accent
                }
                Label {
                    Layout.fillWidth: true
                    text: qsTr("Build, colour, and push the calibration patterns to the TOP / N / W / S / E screens.")
                    color: Theme.textCaption
                    font.pixelSize: Theme.captionFontSize
                    elide: Text.ElideRight
                }
            }
            ColumnLayout{
                Layout.fillWidth: true
                Layout.fillHeight: true

                SegmentedControl {
                    id: patternSelector
                    Layout.fillWidth: true
                    stretch: true
                    model: [qsTr("Concentric"), qsTr("Stripline"), qsTr("Chessboard")]
                    currentIndex: root.concentricMode
                }
                JojoConcentricPanel {
                    id: concentricPanel
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.alignment: Qt.AlignTop
                    visible: root.mode === root.concentricMode

                    onImportRequested: console.log("[Pattern And Monitor] Concentric: Import JSON requested")
                    onExportRequested: console.log("[Pattern And Monitor] Concentric: Export JSON requested")
                    onSaveImageRequested: console.log("[Pattern And Monitor] Concentric: Save Image requested")
                    onUpdateRequested: (direction) => console.log(
                        "[Pattern And Monitor] Concentric: Update requested, direction=" + direction)
                }
                JojoStriplinePanel {
                    id: striplinePanel
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.alignment: Qt.AlignTop
                    visible: root.mode === root.striplineMode

                    onImportRequested: console.log("[Pattern And Monitor] Stripline: Import JSON requested")
                    onExportRequested: console.log("[Pattern And Monitor] Stripline: Export JSON requested")
                    onSaveImageRequested: console.log("[Pattern And Monitor] Stripline: Save Image requested")
                    onUpdateRequested: (direction) => console.log(
                        "[Pattern And Monitor] Stripline: Update requested, direction=" + direction)
                }
                JojoChessboardPanel {
                    id: chessboardPanel
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.alignment: Qt.AlignTop
                    visible: root.mode === root.chessboardMode

                    onGenerateRequested: console.log("[Pattern And Monitor] Chessboard: Generate requested")
                    onSaveImageRequested: console.log("[Pattern And Monitor] Chessboard: Save Image requested")
                    onUpdateRequested: (direction) => console.log(
                        "[Pattern And Monitor] Chessboard: Update requested, direction=" + direction)
                }
            }
        }
        ColumnLayout{
            Layout.fillHeight: true
            Layout.preferredWidth: patternGenerator.free * root.ratioMonitorViewer
            RowLayout{
                Layout.fillWidth: true
                spacing: Theme.rowSpacing
                Label {
                    text: "Monitor Viewer"
                    font.bold: true
                    color: Theme.accent
                }
                Label {
                    Layout.fillWidth: true
                    text: qsTr("Control monitor pattern image and brightness")
                    color: Theme.textCaption
                    font.pixelSize: Theme.captionFontSize
                    elide: Text.ElideRight
                }
            }
            RowLayout{
                Layout.fillWidth: true
                Item {Layout.fillWidth: true}
                RowLayout{
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
                        // tone: "danger"
                        text: qsTr("Reconnect")
                        // onClicked: root.turnOffFourSides()

                        ToolTip.visible: hovered
                        ToolTip.delay: Theme.animSlow
                        ToolTip.text: qsTr("Reconnect")
                    }
                }
                Item {Layout.fillWidth: true}
            }
            RowLayout {
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
            DpadMonitorViewer{
                id: monitorViewer
                Layout.fillWidth: true
                Layout.fillHeight: true
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
            console.log("[Pattern And Monitor] Apply Mapping requested: top=" + top
                + " n=" + north + " w=" + west + " s=" + south + " e=" + east)
            root.applyMappingRequested(top, north, west, south, east)
        }
    }
}