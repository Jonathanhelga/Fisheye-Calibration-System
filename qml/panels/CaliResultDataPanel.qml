pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FisheyeCaliJojo

Rectangle {
    id: panel

    readonly property var directions: ["N", "S", "W", "E", "NW", "SE", "SW", "NE"]

    // 75 layers, matching CaliTableData::kRows.
    readonly property int layerCount: 75
    readonly property int noSideLayer: layerCount

    property alias round: roundSelector.currentIndex

    // The table lives in the controller.
    readonly property var rounds: CalibrationController.rounds
    readonly property var sideLayers: CalibrationController.sideLayers

    readonly property var rows: round < rounds.length ? rounds[round] : []
    readonly property var blankRow: panel.emptyRow()

    // Display only; the controller keeps full values.
    readonly property int valueDecimals: 2
    readonly property int alphaDecimals: 4
    readonly property var shownRows: rows.map((row) => panel.roundedRow(row))

    readonly property int sideLayer: round < sideLayers.length ? sideLayers[round]
                                                              : noSideLayer
    // The rig's fallback when unmarked.
    readonly property int defaultSideLayer: 40
    readonly property int sideStartLayer: sideLayer === noSideLayer ? defaultSideLayer : sideLayer

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

    // A read-out of a measurement, not an input.
    readonly property var posCenter: ComputeController.centers["positive"]
    readonly property var negCenter: ComputeController.centers["negative"]

    readonly property string posICx: posCenter && posCenter.ok ? String(posCenter.x) : ""
    readonly property string posICy: posCenter && posCenter.ok ? String(posCenter.y) : ""
    readonly property string negICx: negCenter && negCenter.ok ? String(negCenter.x) : ""
    readonly property string negICy: negCenter && negCenter.ok ? String(negCenter.y) : ""

    readonly property string aggregation: CalibrationController.aggregationText
    property string distance: String(CalibrationController.baseDistance)

    // readonly: assigning here would destroy the binding.
    readonly property bool singleDistance: CalibrationController.singleDistance

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

    function rounded(text, decimals) {
        const shown = String(text)
        const number = Number(shown)
        const dot = shown.indexOf(".")
        if (shown === "" || !isFinite(number) || dot < 0)
            return shown
        if (shown.length - dot - 1 <= decimals)
            return shown
        return number.toFixed(decimals)
    }

    function roundedRow(row) {
        return {
            ictAvg: panel.rounded(row.ictAvg, panel.valueDecimals),
            pctCal: panel.rounded(row.pctCal, panel.valueDecimals),
            distance: panel.rounded(row.distance, panel.valueDecimals),
            alpha: row.alpha.map((v) => panel.rounded(v, panel.alphaDecimals)),
            zfl: row.zfl.map((v) => panel.rounded(v, panel.valueDecimals)),
            alphaAvg: panel.rounded(row.alphaAvg, panel.alphaDecimals),
            zflAvg: panel.rounded(row.zflAvg, panel.valueDecimals)
        }
    }

    // Edits go to the controller, which owns it.
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
    clip: true

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
        property int fieldAlignment: TextInput.AlignLeft

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
            horizontalAlignment: toolField.fieldAlignment
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
            id: roundGroup

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
        }

        // Without these, round_enabled silently means all.
        Flow {
            Layout.fillWidth: true
            Layout.minimumWidth: Math.max(useGroup.implicitWidth, sideGroup.implicitWidth)
            spacing: Theme.spaceMd

            RowLayout {
                id: useGroup

                spacing: Theme.labelSpacing

                Label {
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
            }

            ToolField {
                id: sideGroup

                caption: qsTr("Side starts at layer:")
                fieldWidth: Math.round(Theme.charUnit * 17)
                fieldAlignment: TextInput.AlignHCenter
                value: panel.sideLayer === panel.noSideLayer
                       ? qsTr("%1 (default)").arg(panel.defaultSideLayer)
                       : panel.sideLayer
                editable: false
            }
        }

        Flow {
            Layout.fillWidth: true
            Layout.minimumWidth: Math.max(centreGroup.implicitWidth,
                                          distanceGroup.implicitWidth,
                                          actionGroup.implicitWidth)
            spacing: Theme.spaceMd

            RowLayout {
                id: centreGroup

                spacing: Theme.spaceSm

                ToolField { caption: qsTr("pos_iCx:"); value: panel.posICx; editable: false }
                ToolField { caption: qsTr("pos_iCy:"); value: panel.posICy; editable: false }
                ToolField { caption: qsTr("neg_iCx:"); value: panel.negICx; editable: false }
                ToolField { caption: qsTr("neg_iCy:"); value: panel.negICy; editable: false }
            }

            RowLayout {
                id: distanceGroup

                spacing: Theme.spaceSm

                ToolField {
                    caption: qsTr("Aggregation:")
                    fieldWidth: Math.round(Theme.charUnit * 12)
                    value: panel.aggregation
                    editable: false
                }
                ToolField {
                    caption: qsTr("Distance:")
                    value: panel.distance
                    onEdited: (value) => {
                        CalibrationController.baseDistance = parseFloat(value)
                    }
                }

                PatternToggleSwitch {
                    text: qsTr("Per-round distance")
                    checked: panel.singleDistance
                    onToggled: (value) => CalibrationController.singleDistance = value
                    tooltip: qsTr("Give each round its own distance instead of deriving every round from one base value.")
                }
            }

            RowLayout {
                id: actionGroup

                spacing: Theme.spaceSm

                ActionButton {
                    text: qsTr("Aggr Round %1").arg(panel.round)
                    enabled: !CalibrationController.busy
                    onClicked: panel.aggrRoundRequested(panel.round)

                    ToolTip.visible: hovered
                    ToolTip.delay: Theme.animSlow
                    ToolTip.text: qsTr("Measure how tightly this round's ZFL values agree with each other.")
                }
                // Not "Clean Noise": this removes bands destructively.
                ActionButton {
                    text: qsTr("Remove Noise Bands")
                    enabled: !CalibrationController.busy && panel.roundHasData
                    onClicked: panel.cleanNoiseRequested(panel.round)

                    ToolTip.visible: hovered
                    ToolTip.delay: Theme.animSlow
                    ToolTip.text: qsTr("Find dense bands of false crossings in round %1 and delete "
                                     + "them from the table. This edits the data and cannot be "
                                     + "undone. It is not the Noise cleaning switch on the "
                                     + "Centering panel, which changes how the rig detects.")
                                      .arg(panel.round)
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
                                      formula: "α = atan(PCT cal / distance)" },
                                    { caption: qsTr("α, side monitors"),
                                      formula: "α = π/2 − atan[(distance − PCT cal − V_Gap) / H_Gap]" },
                                    { caption: qsTr("ZFL"),
                                      formula: "ZFL = ICT / tan(α)" }
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
                    // The engine's own "anything worth computing yet".
                    enabled: !CalibrationController.busy && CalibrationController.hasRawIct
                    onClicked: panel.calculateRequested(panel.round)

                    ToolTip.visible: hovered
                    ToolTip.delay: Theme.animSlow
                    ToolTip.text: qsTr("Fill the computed columns of this round from its measured PCT and ICT values.")
                }
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
                    BandCell { Layout.preferredWidth: cols.perDirBand;   text: qsTr("α (rad) and ZFL (px) per direction") }
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
                readonly property var shown: panel.shownRows[cell.index] || panel.blankRow
                readonly property bool onSide: cell.index >= panel.sideStartLayer
                readonly property bool opensSection: cell.index === 0
                                                     || cell.index === panel.sideStartLayer

                width: cols.totalWidth
                height: sectionBand.height + layerRow.implicitHeight + Theme.spaceXs

                // Names the section this row starts.
                Rectangle {
                    id: sectionBand

                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    height: cell.opensSection ? layerRow.cellHeight : 0
                    visible: cell.opensSection
                    color: Theme.accent

                    Label {
                        anchors.fill: parent
                        leftPadding: Theme.spaceSm
                        verticalAlignment: Text.AlignVCenter
                        text: cell.onSide
                              ? (panel.sideLayer === panel.noSideLayer
                                 ? qsTr("Side monitors, from layer %1 (default)").arg(panel.sideStartLayer)
                                 : qsTr("Side monitors, from layer %1").arg(panel.sideStartLayer))
                              : qsTr("Top monitor")
                        color: Theme.textOnAccent
                        font.pixelSize: Theme.captionFontSize
                        font.bold: true
                    }
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: sectionBand.bottom
                    anchors.bottom: parent.bottom
                    color: cell.onSide
                           ? (cell.index % 2 === 0 ? Theme.sideRowBackground : Theme.sideRowAltBackground)
                           : (cell.index % 2 === 0 ? "transparent" : Theme.fieldDisabledBackground)
                }

                CaliResultRow {
                    id: layerRow

                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: sectionBand.bottom
                    anchors.bottom: parent.bottom
                    anchors.topMargin: Math.round(Theme.spaceXs / 2)
                    anchors.bottomMargin: Math.round(Theme.spaceXs / 2)

                    layerIndex: cell.index
                    values: cell.values
                    shownValues: cell.shown
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
