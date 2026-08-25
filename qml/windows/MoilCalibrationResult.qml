import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FisheyeCaliJojo

Window {
    id: root

    title: qsTr("MOIL Calibration Result")
    width:  Screen.desktopAvailableWidth
    height: Screen.desktopAvailableHeight

    property bool busy: false
    property int loadStatus: ServerProbe.Unknown
    property bool singleDistance: false

    property alias caliFolder: caliFolderField.text
    property alias caliSystem: caliSystemCombo.currentIndex

    readonly property int folderStatus: root.busy ? ServerProbe.Checking
                                      : root.caliFolder.length === 0 ? ServerProbe.Unknown
                                                                     : root.loadStatus

    readonly property string folderStatusText: root.busy ? qsTr("Loading...")
                                             : root.caliFolder.length === 0 ? qsTr("No folder chosen")
                                             : root.loadStatus === ServerProbe.Ok ? qsTr("Loaded")
                                             : root.loadStatus === ServerProbe.Partial ? qsTr("Partly loaded")
                                             : root.loadStatus === ServerProbe.Failed ? qsTr("Load failed")
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
    signal updateTableRequested()
    signal loadDatabaseRequested()
    signal stopRequested()
    signal clearTableRequested()
    signal clearAllTablesRequested()

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
                        console.log("[Cali Result] Browse calibration folder requested")
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
                    model: ["Yuanman - SIDE (EV2785)",
                            "Yuanman - SIDE (EV2730Q)",
                            "Yinda",
                            "Broland C++"]
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
                onClicked: root.loadAllExcelRequested()

                ToolTip.visible: hovered
                ToolTip.delay: Theme.animSlow
                ToolTip.text: qsTr("Read every round's Excel file in the calibration folder into rounds 1 to 10 at once.")
            }
            ActionButton {
                text: qsTr("Load Excel")
                onClicked: root.loadExcelRequested()

                ToolTip.visible: hovered
                ToolTip.delay: Theme.animSlow
                ToolTip.text: qsTr("Read one Excel file into the round that is currently selected.")
            }
            ActionButton {
                text: qsTr("Load Database")
                onClicked: root.loadDatabaseRequested()

                ToolTip.visible: hovered
                ToolTip.delay: Theme.animSlow
                ToolTip.text: qsTr("Browse earlier calibration runs and load one into the tables.")
            }

            ActionSeparator {}

            ActionButton {
                text: qsTr("Update Table")
                onClicked: root.updateTableRequested()

                ToolTip.visible: hovered
                ToolTip.delay: Theme.animSlow
                ToolTip.text: qsTr("Refill the round tables from the selected calibration system.")
            }
            ActionButton {
                text: qsTr("Save to Excel")
                onClicked: root.saveExcelRequested()

                ToolTip.visible: hovered
                ToolTip.delay: Theme.animSlow
                ToolTip.text: qsTr("Write the round tables back out to Excel in the calibration folder.")
            }
            ActionButton {
                text: qsTr("Stop")
                enabled: root.busy
                onClicked: root.stopRequested()

                ToolTip.visible: hovered
                ToolTip.delay: Theme.animSlow
                ToolTip.text: qsTr("Cancel the load or search that is currently running.")
            }

            Item { Layout.fillWidth: true }

            ActionButton {
                text: qsTr("Clear Table")
                tone: "danger"
                onClicked: root.clearTableRequested()

                ToolTip.visible: hovered
                ToolTip.delay: Theme.animSlow
                ToolTip.text: qsTr("Erase every value in the round that is currently selected.")
            }
            ActionButton {
                text: qsTr("Clear All Table")
                tone: "danger"
                onClicked: root.clearAllTablesRequested()

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

            onCalculateRequested: (round) => console.log("[Cali Result] Calculate result for round " + round)
            onAggrRoundRequested: (round) => console.log("[Cali Result] Aggregation for round " + round)
            onCleanNoiseRequested: (round) => console.log("[Cali Result] Clean noise for round " + round)
        }

        CaliResultParameterPanel {
            id: parameterPanel

            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.view === root.viewParameter

            onUpdateAllRequested: console.log("[Cali Result] Update all calibration results")
            onUpdateIhAlphaRequested: console.log("[Cali Result] Update IH-Alpha plot")
            onUpdateIhZflRequested: console.log("[Cali Result] Update ZFL-IH plot")
            onSaveParametersRequested: console.log("[Cali Result] Save camera parameters")
            onSaveConfigurationRequested: console.log("[Cali Result] Save calibration system configuration")
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.view !== root.viewData && root.view !== root.viewParameter

            color: Theme.panelBackground
            border.color: Theme.panelBorder
            radius: Theme.radius

            Label {
                anchors.centerIn: parent
                color: Theme.textCaption
                font.pixelSize: Theme.captionFontSize
                text: root.view === root.viewOverlap ? qsTr("Overlap plot goes here.")
                    : root.view === root.viewAggregation ? qsTr("Aggregation by distance and IH range goes here.")
                                                         : qsTr("IH-alpha and IH-ZFL graphs go here.")
            }
        }
    }
}
