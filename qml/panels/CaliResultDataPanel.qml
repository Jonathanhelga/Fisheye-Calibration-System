pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FisheyeCaliJojo

Rectangle {
    id: panel

    readonly property var directions: ["N", "S", "W", "E", "NW", "SE", "SW", "NE"]
    readonly property int layerCount: 74
    readonly property int noSideLayer: layerCount

    property alias round: roundSelector.currentIndex

    property var rounds: []
    property var sideLayers: []

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

    property string posICx: ""
    property string posICy: ""
    property string negICx: ""
    property string negICy: ""
    property string aggregation: ""
    property string distance: ""
    property bool singleDistance: false

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

    function buildEmptyTables() {
        const tables = []
        const sides = []
        for (let r = 0; r <= 10; r++) {
            const table = []
            for (let layer = 0; layer < panel.layerCount; layer++)
                table.push(panel.emptyRow())
            tables.push(table)
            sides.push(panel.noSideLayer)
        }
        panel.rounds = tables
        panel.sideLayers = sides
    }

    function updateRow(layer, changes) {
        if (panel.round >= panel.rounds.length)
            return
        const tables = panel.rounds.slice()
        const table = tables[panel.round].slice()
        table[layer] = Object.assign({}, table[layer], changes)
        tables[panel.round] = table
        panel.rounds = tables
    }

    function setPct(layer, value) {
        panel.updateRow(layer, { pct: value })
    }

    function setIct(layer, direction, value) {
        const ict = panel.rows[layer].ict.slice()
        ict[direction] = value
        panel.updateRow(layer, { ict: ict })
    }

    function setSideLayer(layer) {
        const next = panel.sideLayers.slice()
        next[panel.round] = next[panel.round] === layer ? panel.noSideLayer : layer
        panel.sideLayers = next
    }

    Component.onCompleted: buildEmptyTables()

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

            ToolField {
                caption: qsTr("pos_iCx:")
                value: panel.posICx
                onEdited: (value) => panel.posICx = value
            }
            ToolField {
                caption: qsTr("pos_iCy:")
                value: panel.posICy
                onEdited: (value) => panel.posICy = value
            }
            ToolField {
                caption: qsTr("neg_iCx:")
                value: panel.negICx
                onEdited: (value) => panel.negICx = value
            }
            ToolField {
                caption: qsTr("neg_iCy:")
                value: panel.negICy
                onEdited: (value) => panel.negICy = value
            }

            Item { Layout.fillWidth: true }

            ToolField {
                caption: qsTr("Aggregation:")
                value: panel.aggregation
                editable: false
            }
            ToolField {
                caption: qsTr("Distance:")
                value: panel.distance
                onEdited: (value) => panel.distance = value
            }

            PatternToggleSwitch {
                text: qsTr("Per-round distance")
                checked: panel.singleDistance
                onToggled: (value) => panel.singleDistance = value
                tooltip: qsTr("Give each round its own distance instead of deriving every round from one base value.")
            }

            ActionButton {
                text: qsTr("Aggr Round %1").arg(panel.round)
                onClicked: panel.aggrRoundRequested(panel.round)

                ToolTip.visible: hovered
                ToolTip.delay: Theme.animSlow
                ToolTip.text: qsTr("Measure how tightly this round's ZFL values agree with each other.")
            }
            ActionButton {
                text: qsTr("Clean Noise")
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
