pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FisheyeCaliJojo

Rectangle {
    id: panel

    readonly property var directions: ["N", "S", "W", "E", "NW", "SE", "SW", "NE"]

    // 75, not 74. A round table is 77 rows: two headers then the layer rows, and
    // the model on the far side (CaliTableData::kRows) is the same table. A
    // shorter count here does not mean a smaller table, it means a truncated one
    // -- a real capture's after-bezel segment runs well past layer 40.
    readonly property int layerCount: 75
    readonly property int noSideLayer: layerCount

    property alias round: roundSelector.currentIndex

    // The table lives in the controller. This panel displays it and sends edits
    // back; it does not hold a second copy, because the copy that travels to the
    // rig must be the copy on screen.
    readonly property var rounds: CalibrationController.rounds
    readonly property var sideLayers: CalibrationController.sideLayers

    readonly property var rows: round < rounds.length ? rounds[round] : []
    readonly property var blankRow: panel.emptyRow()
    readonly property int sideLayer: round < sideLayers.length ? sideLayers[round]
                                                              : noSideLayer

    readonly property bool roundHasData: {
        for (let i = 0; i < rows.length; i++) {
            if (rows[i].pct !== "")
                return true
            for (let d = 0; d < rows[i].ict.length; d++)
                if (rows[i].ict[d] !== "")
                    return true
        }
        return false
    }

    // Where the centres were found, read straight off the detect results. These
    // are a read-out of a measurement, not an input -- typing over them would be
    // claiming a centre nothing measured.
    readonly property var posCenter: ComputeController.centers["positive"]
    readonly property var negCenter: ComputeController.centers["negative"]

    readonly property string posICx: posCenter && posCenter.ok ? String(posCenter.x) : ""
    readonly property string posICy: posCenter && posCenter.ok ? String(posCenter.y) : ""
    readonly property string negICx: negCenter && negCenter.ok ? String(negCenter.x) : ""
    readonly property string negICy: negCenter && negCenter.ok ? String(negCenter.y) : ""

    readonly property string aggregation: CalibrationController.aggregationText
    property string distance: String(CalibrationController.baseDistance)

    signal calculateRequested(int round)
    signal aggrRoundRequested(int round)
    signal cleanNoiseRequested(int round)

    function emptyRow() {
        return {
            pct: "",
            ict: ["", "", "", "", "", "", "", ""],
            ictAvg: "", pctCal: "", distance: "",
            alpha: ["", "", "", "", "", "", "", ""],
            zfl: ["", "", "", "", "", "", "", ""],
            alphaAvg: "", zflAvg: ""
        }
    }

    // Edits go to the controller, which owns the table and bumps its version so
    // every cached series knows it is stale. The display then follows the
    // controller back; nothing is written locally and re-sent later.
    function setPct(layer, value) {
        CalibrationController.setPct(panel.round, layer, value)
    }

    function setIct(layer, direction, value) {
        CalibrationController.setIct(panel.round, layer, direction, value)
    }

    function setSideLayer(layer) {
        CalibrationController.setSideLayer(panel.round, layer)
    }

    color: Theme.panelBackground
    border.color: Theme.panelBorder
    radius: Theme.radius

    QtObject {
        id: cols

        readonly property int spacing:      Theme.spaceXs
        readonly property int layerWidth:   Math.round(Theme.charUnit * 5)
        readonly property int pctWidth:     Math.round(Theme.charUnit * 6)
        readonly property int ictWidth:     Math.round(Theme.charUnit * 6)
        readonly property int coreWidth:    Math.round(Theme.charUnit * 8)
        readonly property int alphaWidth:   Math.round(Theme.charUnit * 6)
        readonly property int zflWidth:     Math.round(Theme.charUnit * 7)
        readonly property int avgWidth:     Math.round(Theme.charUnit * 8)
        readonly property int dividerWidth: 1

        readonly property int identityBand: layerWidth + spacing + pctWidth
        readonly property int ictBand:      8 * ictWidth + 7 * spacing
        readonly property int computedBand: 3 * coreWidth + 2 * spacing
        readonly property int perDirBand:   8 * (alphaWidth + zflWidth) + 15 * spacing
        readonly property int averageBand:  2 * avgWidth + spacing

        readonly property int totalWidth: identityBand + ictBand + computedBand
                                        + perDirBand + averageBand
                                        + 3 * dividerWidth + 7 * spacing
    }

    component BandCell: Rectangle {
        id: band

        property alias text: bandLabel.text

        Layout.preferredHeight: Math.round(Theme.controlHeight * 0.62)
        color: band.text.length > 0 ? Theme.fieldDisabledBackground : "transparent"
        radius: Theme.radius

        Label {
            id: bandLabel

            anchors.fill: parent
            color: Theme.textCaption
            font.pixelSize: Theme.captionFontSize
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }

    component HeaderCell: Label {
        Layout.preferredHeight: Math.round(Theme.controlHeight * 0.7)
        color: Theme.textCaption
        font.pixelSize: Theme.captionFontSize
        font.bold: true
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    component HeaderDivider: Rectangle {
        Layout.preferredWidth: cols.dividerWidth
        Layout.fillHeight: true
        color: "transparent"
    }

    component ToolField: RowLayout {
        id: toolField

        property string caption: ""
        property var value: ""
        property bool editable: true
        property int fieldWidth: Math.round(Theme.charUnit * 7)

        signal edited(string value)

        Layout.fillWidth: false
        spacing: Theme.labelSpacing

        Label {
            text: toolField.caption
            color: Theme.textCaption
            font.pixelSize: Theme.captionFontSize
        }

        ValueField {
            Layout.preferredWidth: toolField.fieldWidth
            Layout.preferredHeight: Theme.controlHeight
            editable: toolField.editable
            value: toolField.value
            onEdited: (value) => toolField.edited(value)
        }
    }

    ColumnLayout {
        id: content

        anchors.fill: parent
        anchors.margins: Theme.panelMargin
        spacing: Theme.rowSpacing

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.labelSpacing

            Label {
                text: qsTr("Round:")
                color: Theme.textCaption
                font.pixelSize: Theme.captionFontSize
            }

            SegmentedControl {
                id: roundSelector

                model: [qsTr("Current"), "1", "2", "3", "4", "5",
                        "6", "7", "8", "9", "10"]
            }

            Label {
                Layout.leftMargin: Theme.spaceSm
                text: panel.roundHasData ? qsTr("%1 layers").arg(panel.rows.length)
                                         : qsTr("Empty, load a file or capture into it first")
                color: Theme.textCaption
                font.pixelSize: Theme.captionFontSize
            }

            // Which rounds count. round_enabled ships with every request and the
            // server uses it for the aggregations, the regression fit and the
            // pooled Overlap -- so without these it silently means "all of them",
            // including a round you know is bad.
            Label {
                Layout.leftMargin: Theme.spaceMd
                text: qsTr("Use:")
                color: Theme.textCaption
                font.pixelSize: Theme.captionFontSize
            }

            Repeater {
                model: 10

                CheckBox {
                    id: roundBox

                    required property int index

                    text: String(roundBox.index + 1)
                    font.pixelSize: Theme.captionFontSize
                    checked: CalibrationController.roundEnabled[roundBox.index] !== false
                    onToggled: CalibrationController.setRoundEnabled(roundBox.index + 1, checked)

                    ToolTip.visible: hovered
                    ToolTip.delay: Theme.animSlow
                    ToolTip.text: qsTr("Include round %1 in the aggregations, the regression "
                                     + "fit and the Overlap plot.").arg(roundBox.index + 1)
                }
            }

            Item { Layout.fillWidth: true }

            ToolField {
                caption: qsTr("Side starts at layer:")
                fieldWidth: Math.round(Theme.charUnit * 10)
                value: panel.sideLayer === panel.noSideLayer ? qsTr("none")
                                                             : panel.sideLayer
                editable: false
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spaceSm

            ToolField { caption: qsTr("pos_iCx:"); value: panel.posICx; editable: false }
            ToolField { caption: qsTr("pos_iCy:"); value: panel.posICy; editable: false }
            ToolField { caption: qsTr("neg_iCx:"); value: panel.negICx; editable: false }
            ToolField { caption: qsTr("neg_iCy:"); value: panel.negICy; editable: false }

            Item { Layout.fillWidth: true }

            ToolField {
                caption: qsTr("Aggregation:")
                value: panel.aggregation
                editable: false
            }
            ToolField {
                caption: qsTr("Distance:")
                value: panel.distance
                onEdited: (value) => {
                    panel.distance = value
                    CalibrationController.baseDistance = parseFloat(value)
                }
            }

            ActionButton {
                text: qsTr("Aggr Round %1").arg(panel.round)
                enabled: !CalibrationController.busy
                onClicked: panel.aggrRoundRequested(panel.round)

                ToolTip.visible: hovered
                ToolTip.delay: Theme.animSlow
                ToolTip.text: qsTr("Measure how tightly this round's ZFL values agree with each other.")
            }
            ActionButton {
                text: qsTr("Clean Noise")
                enabled: !CalibrationController.busy
                onClicked: panel.cleanNoiseRequested(panel.round)

                ToolTip.visible: hovered
                ToolTip.delay: Theme.animSlow
                ToolTip.text: qsTr("Drop measurements that sit too far from their neighbours before computing.")
            }
            ActionButton {
                id: formulaButton

                text: qsTr("Formulas")
                onClicked: formulaPopup.open()

                ToolTip.visible: hovered
                ToolTip.delay: Theme.animSlow
                ToolTip.text: qsTr("Show how α and ZFL are derived from PCT, ICT, and distance.")

                Popup {
                    id: formulaPopup

                    y: formulaButton.height + Theme.spaceXs
                    x: -width + formulaButton.width
                    padding: Theme.panelMargin
                    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

                    background: Rectangle {
                        color: Theme.panelBackground
                        border.color: Theme.panelBorder
                        radius: Theme.radius
                    }

                    contentItem: ColumnLayout {
                        spacing: Theme.rowSpacing

                        Label {
                            text: qsTr("How the computed columns are derived")
                            font.bold: true
                            font.pixelSize: Theme.fontTitle
                            color: Theme.accent
                        }

                        Repeater {
                            model: [
                                { caption: qsTr("α, top monitor"),
                                  formula: "α = atan(PCT / distance)" },
                                { caption: qsTr("α, side monitors"),
                                  formula: "α = π/2 − atan[(distance − PCT − V_Gap) / H_Gap]" },
                                { caption: qsTr("ZFL"),
                                  formula: "ZFL = 1 / tan(α) × image height" }
                            ]

                            ColumnLayout {
                                id: formulaEntry

                                required property var modelData

                                Layout.fillWidth: true
                                spacing: 0

                                Label {
                                    text: formulaEntry.modelData.caption
                                    color: Theme.textCaption
                                    font.pixelSize: Theme.captionFontSize
                                }
                                Label {
                                    text: formulaEntry.modelData.formula
                                    color: Theme.textPrimary
                                    font.bold: true
                                }
                            }
                        }
                    }
                }
            }
            ActionButton {
                text: qsTr("Calculate Result")
                tone: "accent"
                // hasRawIct is the engine's own "is there anything worth
                // computing yet". Running the pipeline over an empty table
                // produced a table of blanks and no explanation.
                enabled: !CalibrationController.busy && CalibrationController.hasRawIct
                onClicked: panel.calculateRequested(panel.round)

                ToolTip.visible: hovered
                ToolTip.delay: Theme.animSlow
                ToolTip.text: qsTr("Fill the computed columns of this round from its measured PCT and ICT values.")
            }
        }

        Item {
            id: headerViewport

            Layout.fillWidth: true
            Layout.preferredHeight: headerColumn.implicitHeight
            clip: true

            ColumnLayout {
                id: headerColumn

                x: -table.contentX
                width: cols.totalWidth
                spacing: Theme.spaceXs

                RowLayout {
                    Layout.fillWidth: false
                    spacing: cols.spacing

                    BandCell { Layout.preferredWidth: cols.identityBand }
                    BandCell { Layout.preferredWidth: cols.ictBand;      text: qsTr("ICT measured (px)") }
                    HeaderDivider {}
                    BandCell { Layout.preferredWidth: cols.computedBand; text: qsTr("Computed") }
                    HeaderDivider {}
                    BandCell { Layout.preferredWidth: cols.perDirBand;   text: qsTr("α (°) and ZFL (px) per direction") }
                    HeaderDivider {}
                    BandCell { Layout.preferredWidth: cols.averageBand;  text: qsTr("Average") }
                }

                RowLayout {
                    Layout.fillWidth: false
                    spacing: cols.spacing

                    HeaderCell { Layout.preferredWidth: cols.layerWidth; text: qsTr("Layer") }
                    HeaderCell { Layout.preferredWidth: cols.pctWidth;   text: qsTr("PCT") }

                    Repeater {
                        model: panel.directions

                        HeaderCell {
                            required property string modelData

                            Layout.preferredWidth: cols.ictWidth
                            text: modelData
                        }
                    }

                    HeaderDivider {}

                    HeaderCell { Layout.preferredWidth: cols.coreWidth; text: qsTr("ICT avg") }
                    HeaderCell { Layout.preferredWidth: cols.coreWidth; text: qsTr("PCT cal") }
                    HeaderCell { Layout.preferredWidth: cols.coreWidth; text: qsTr("Distance") }

                    HeaderDivider {}

                    Repeater {
                        model: panel.directions

                        RowLayout {
                            id: headerPair

                            required property string modelData

                            Layout.fillWidth: false
                            spacing: cols.spacing

                            HeaderCell {
                                Layout.preferredWidth: cols.alphaWidth
                                text: qsTr("α %1").arg(headerPair.modelData)
                            }
                            HeaderCell {
                                Layout.preferredWidth: cols.zflWidth
                                text: qsTr("ZFL %1").arg(headerPair.modelData)
                            }
                        }
                    }

                    HeaderDivider {}

                    HeaderCell { Layout.preferredWidth: cols.avgWidth; text: qsTr("α avg") }
                    HeaderCell { Layout.preferredWidth: cols.avgWidth; text: qsTr("ZFL avg") }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.panelBorder
        }

        ListView {
            id: table

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 0
            Layout.topMargin: Theme.spaceXs

            clip: true
            spacing: Theme.spaceXs
            contentWidth: cols.totalWidth
            flickableDirection: Flickable.HorizontalAndVerticalFlick
            boundsBehavior: Flickable.StopAtBounds
            model: panel.rows.length

            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
            ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded }

            delegate: Item {
                id: cell

                required property int index

                readonly property var values: panel.rows[cell.index] || panel.blankRow

                width: cols.totalWidth
                height: layerRow.implicitHeight + Theme.spaceXs

                Rectangle {
                    anchors.fill: parent
                    color: cell.index % 2 === 0 ? "transparent" : Theme.fieldDisabledBackground
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    height: 1
                    color: Theme.accent
                    visible: cell.index === panel.sideLayer
                }

                CaliResultRow {
                    id: layerRow

                    anchors.fill: parent
                    anchors.topMargin: Math.round(Theme.spaceXs / 2)
                    anchors.bottomMargin: Math.round(Theme.spaceXs / 2)

                    layerIndex: cell.index
                    values: cell.values
                    directions: panel.directions
                    syncToken: panel.round
                    sideStart: cell.index === panel.sideLayer

                    layerWidth: cols.layerWidth
                    pctWidth: cols.pctWidth
                    ictWidth: cols.ictWidth
                    coreWidth: cols.coreWidth
                    alphaWidth: cols.alphaWidth
                    zflWidth: cols.zflWidth
                    avgWidth: cols.avgWidth
                    dividerWidth: cols.dividerWidth
                    columnSpacing: cols.spacing

                    onPctEdited: (value) => panel.setPct(cell.index, value)
                    onIctEdited: (direction, value) => panel.setIct(cell.index, direction, value)
                    onSideStartPicked: panel.setSideLayer(cell.index)
                }
            }
        }
    }
}
