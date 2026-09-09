pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FisheyeCaliJojo

Rectangle {
    id: panel

    readonly property int rangeCount: 20
    readonly property int resultRowCount: 10

    readonly property var rowCaptions: [qsTr("IH Min"), qsTr("IH Max"),
                                        qsTr("Dist Min"), qsTr("Dist Max"),
                                        qsTr("Aggregation"), qsTr("PCT to Pupil"),
                                        qsTr("Sampling Number"),
                                        qsTr("Alpha Min"), qsTr("Alpha Max")]

    property var ranges: []
    property var rangeResults: []

    property var gaps: ({ vN: "", vS: "", vE: "", vW: "",
                          hN: "", hS: "", hE: "", hW: "" })

    property string pixelSizeTop: ""
    property string pixelSizeSide: ""
    property string disPerRound: ""
    property string roundLabel: ""

    property string ihMinInterval: ""
    property string ihMaxInterval: ""
    property string stepInterval: ""
    property string windowInterval: ""

    property string targetIhMin: ""
    property string targetIhMax: ""
    property string targetAggregation: ""
    property string targetDistance: ""

    property bool historyDistance: false

    readonly property var blankRange: panel.emptyRange("")
    readonly property var blankResult: panel.emptyResult()

    readonly property bool allRangesEnabled: {
        for (let i = 0; i < panel.ranges.length; ++i)
            if (!panel.ranges[i].enabled)
                return false
        return panel.ranges.length > 0
    }

    signal rangeWindowRequested()
    signal aggrByRangeAndDistanceRequested()
    signal minAggrByIntervalRequested()
    signal saveHistoryDistanceRequested()
    signal keepRoundDataRequested()
    signal gapEdited(string key, string value)
    signal rangeEnabledChanged(int index, bool enabled)
    signal rangeEdited(int index, string field, string value)
    signal zflIhGraphRequested(int index)

    function emptyRange(label) {
        return {
            label: label,
            enabled: false,
            ihMin: "", ihMax: "",
            distMin: "", distMax: "",
            aggregation: "", pctToPupil: "", samplingNumber: "",
            alphaMin: "", alphaMax: ""
        }
    }

    function emptyResult() {
        return { ihRange: "", minAggregation: "", distance: "", totalSampling: "" }
    }

    function buildEmptyTables() {
        const rows = [panel.emptyRange(qsTr("Global"))]
        for (let i = 1; i <= panel.rangeCount; ++i)
            rows.push(panel.emptyRange(qsTr("Range_%1").arg(i)))
        panel.ranges = rows

        const results = []
        for (let j = 0; j < panel.resultRowCount; ++j)
            results.push(panel.emptyResult())
        panel.rangeResults = results
    }

    function updateRange(index, changes) {
        const rows = panel.ranges.slice()
        rows[index] = Object.assign({}, rows[index], changes)
        panel.ranges = rows
    }

    function setRangeEnabled(index, enabled) {
        panel.updateRange(index, { enabled: enabled })
        panel.rangeEnabledChanged(index, enabled)
    }

    function setAllRangesEnabled(enabled) {
        const rows = panel.ranges.slice()
        for (let i = 0; i < rows.length; ++i)
            rows[i] = Object.assign({}, rows[i], { enabled: enabled })
        panel.ranges = rows
        for (let j = 0; j < rows.length; ++j)
            panel.rangeEnabledChanged(j, enabled)
    }

    function setRangeField(index, field, value) {
        const changes = ({})
        changes[field] = value
        panel.updateRange(index, changes)
        panel.rangeEdited(index, field, value)
    }

    function setGap(key, value) {
        const next = Object.assign({}, panel.gaps)
        next[key] = value
        panel.gaps = next
        panel.gapEdited(key, value)
    }

    Component.onCompleted: buildEmptyTables()

    color: Theme.panelBackground
    border.color: Theme.panelBorder
    radius: Theme.radius

    QtObject {
        id: metrics

        readonly property int fieldWidth:    Math.round(Theme.charUnit * 8)
        readonly property int dirWidth:      Math.round(Theme.charUnit * 3)
        readonly property int captionWidth:  Math.round(Theme.charUnit * 16)

        readonly property int headerHeight:  Math.round(Theme.controlHeight * 0.8)
        readonly property int cellHeight:    Math.round(Theme.controlHeight * 0.8)
        readonly property int cellSpacing:   Theme.spaceXs
        readonly property int minCellWidth:  Math.round(Theme.charUnit * 8)

        readonly property int columnCount:   Math.max(1, panel.ranges.length)
        readonly property int cellWidth: {
            const avail = grid.width - (metrics.columnCount - 1) * metrics.cellSpacing
            return Math.max(metrics.minCellWidth, Math.floor(avail / metrics.columnCount))
        }
    }

    QtObject {
        id: resultCols

        readonly property int indexWidth:   Math.round(Theme.charUnit * 3)
        readonly property int rangeWidth:   Math.round(Theme.charUnit * 12)
        readonly property int aggrWidth:    Math.round(Theme.charUnit * 14)
        readonly property int distWidth:    Math.round(Theme.charUnit * 11)
        readonly property int samplesWidth: Math.round(Theme.charUnit * 12)
    }

    component SectionCaption: Label {
        Layout.fillWidth: true
        Layout.bottomMargin: Theme.spaceXs
        color: Theme.accent
        font.pixelSize: Theme.captionFontSize
        font.bold: true
        elide: Text.ElideRight
    }

    component HSeparator: Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 1
        Layout.topMargin: Theme.spaceXs
        Layout.bottomMargin: Theme.spaceXs
        color: Theme.panelBorder
    }

    component FieldCaption: Label {
        color: Theme.textCaption
        font.pixelSize: Theme.captionFontSize
        horizontalAlignment: Text.AlignRight
        verticalAlignment: Text.AlignVCenter
    }

    component ColumnCaption: Label {
        Layout.preferredWidth: metrics.fieldWidth
        color: Theme.textCaption
        font.pixelSize: Theme.captionFontSize
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    component NumberField: ValueField {
        Layout.preferredWidth: metrics.fieldWidth
        Layout.preferredHeight: Theme.controlHeight
        editable: true
        horizontalAlignment: TextInput.AlignHCenter
        validator: DecimalValidator {
            bottom: 0
            decimals: 4
            notation: DoubleValidator.StandardNotation
        }
    }

    component StackedField: ColumnLayout {
        id: stackedField

        property alias caption: stackedCaption.text
        property alias value: stackedInput.value
        property alias validator: stackedInput.validator

        signal edited(string value)

        Layout.fillWidth: true
        spacing: Theme.labelSpacing

        Label {
            id: stackedCaption
            Layout.fillWidth: true
            color: Theme.textCaption
            font.pixelSize: Theme.captionFontSize
            elide: Text.ElideRight
        }

        NumberField {
            id: stackedInput
            Layout.fillWidth: true
            onEdited: (value) => stackedField.edited(value)
        }
    }

    component ResultHeader: Label {
        Layout.fillWidth: true
        Layout.preferredHeight: metrics.headerHeight
        color: Theme.textCaption
        font.pixelSize: Theme.captionFontSize
        font.bold: true
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    component ResultCell: Rectangle {
        property alias text: resultText.text

        Layout.fillWidth: true
        Layout.preferredHeight: metrics.cellHeight

        color: Theme.fieldDisabledBackground
        radius: Theme.radius

        Label {
            id: resultText

            anchors.fill: parent
            anchors.leftMargin: Theme.spaceXs
            anchors.rightMargin: Theme.spaceXs
            color: Theme.textPrimary
            font.pixelSize: Theme.captionFontSize
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }

    ColumnLayout {
        id: content

        anchors.fill: parent
        anchors.margins: Theme.panelMargin
        spacing: Theme.panelGap

        RowLayout {
            id: topRow

            Layout.fillWidth: true
            spacing: Theme.panelGap

            SectionFrame {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                Layout.minimumWidth: resultCols.indexWidth + resultCols.rangeWidth
                                     + resultCols.aggrWidth + resultCols.distWidth
                                     + resultCols.samplesWidth
                                     + 4 * metrics.cellSpacing + 2 * Theme.fieldPadding

                SectionCaption { text: qsTr("Minimum aggregation per IH range") }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: metrics.cellSpacing

                    ResultHeader {
                        Layout.fillWidth: false
                        Layout.preferredWidth: resultCols.indexWidth
                        text: "#"
                    }
                    ResultHeader { Layout.preferredWidth: resultCols.rangeWidth;   text: qsTr("IH Range") }
                    ResultHeader { Layout.preferredWidth: resultCols.aggrWidth;    text: qsTr("Min Aggregation") }
                    ResultHeader { Layout.preferredWidth: resultCols.distWidth;    text: qsTr("Distance (mm)") }
                    ResultHeader { Layout.preferredWidth: resultCols.samplesWidth; text: qsTr("Total Sampling") }
                }

                HSeparator {}

                ListView {
                    id: resultTable

                    Layout.fillWidth: true
                    Layout.preferredHeight: panel.resultRowCount * metrics.cellHeight
                                            + (panel.resultRowCount - 1) * metrics.cellSpacing

                    clip: true
                    interactive: false
                    spacing: metrics.cellSpacing
                    boundsBehavior: Flickable.StopAtBounds
                    model: panel.rangeResults.length

                    delegate: RowLayout {
                        id: resultRow

                        required property int index

                        readonly property var values: panel.rangeResults[resultRow.index] || panel.blankResult

                        width: resultTable.width
                        spacing: metrics.cellSpacing

                        Label {
                            Layout.preferredWidth: resultCols.indexWidth
                            Layout.preferredHeight: metrics.cellHeight
                            text: String(resultRow.index + 1)
                            color: Theme.textCaption
                            font.pixelSize: Theme.captionFontSize
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        ResultCell { Layout.preferredWidth: resultCols.rangeWidth;   text: resultRow.values.ihRange }
                        ResultCell { Layout.preferredWidth: resultCols.aggrWidth;    text: resultRow.values.minAggregation }
                        ResultCell { Layout.preferredWidth: resultCols.distWidth;    text: resultRow.values.distance }
                        ResultCell { Layout.preferredWidth: resultCols.samplesWidth; text: resultRow.values.totalSampling }
                    }
                }
            }

            RowLayout {
                id: sideCards

                readonly property int cardWidth: 4 * metrics.fieldWidth + 3 * Theme.spaceSm
                                                 + 2 * Theme.fieldPadding

                Layout.alignment: Qt.AlignTop
                Layout.fillHeight: false
                spacing: Theme.panelGap

                SectionFrame {
                    Layout.preferredWidth: sideCards.cardWidth
                    Layout.fillHeight: true

                    SectionCaption { text: qsTr("Pattern gaps") }

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: false
                        spacing: Theme.spaceSm

                        Item {
                            Layout.preferredWidth: metrics.dirWidth
                            Layout.preferredHeight: 1
                        }
                        ColumnCaption { Layout.fillWidth: true; text: qsTr("V_Gap (mm)") }
                        ColumnCaption { Layout.fillWidth: true; text: qsTr("H_Gap (mm)") }
                    }

                    Repeater {
                        model: [{ key: "N", label: qsTr("N") },
                                { key: "S", label: qsTr("S") },
                                { key: "E", label: qsTr("E") },
                                { key: "W", label: qsTr("W") }]

                        RowLayout {
                            id: gapRow

                            required property var modelData

                            Layout.fillWidth: true
                            Layout.fillHeight: false
                            spacing: Theme.spaceSm

                            FieldCaption {
                                Layout.preferredWidth: metrics.dirWidth
                                text: gapRow.modelData.label
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                            }
                            NumberField {
                                Layout.fillWidth: true
                                value: panel.gaps["v" + gapRow.modelData.key]
                                onEdited: (value) => panel.setGap("v" + gapRow.modelData.key, value)
                            }
                            NumberField {
                                Layout.fillWidth: true
                                value: panel.gaps["h" + gapRow.modelData.key]
                                onEdited: (value) => panel.setGap("h" + gapRow.modelData.key, value)
                            }
                        }
                    }

                    Item { Layout.fillHeight: true }
                }

                SectionFrame {
                    Layout.preferredWidth: sideCards.cardWidth
                    Layout.fillHeight: true

                    SectionCaption { text: qsTr("Sensor and round") }

                    GridLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: false
                        columns: 2
                        columnSpacing: Theme.spaceSm
                        rowSpacing: Theme.spaceSm

                        StackedField {
                            caption: qsTr("Pixel Size Top (mm)")
                            value: panel.pixelSizeTop
                            onEdited: (value) => panel.pixelSizeTop = value
                        }

                        StackedField {
                            caption: qsTr("Pixel Size Side (mm)")
                            value: panel.pixelSizeSide
                            onEdited: (value) => panel.pixelSizeSide = value
                        }

                        StackedField {
                            caption: qsTr("Distance / Round (mm)")
                            value: panel.disPerRound
                            onEdited: (value) => panel.disPerRound = value
                        }

                        StackedField {
                            caption: qsTr("Round")
                            value: panel.roundLabel
                            validator: null
                            onEdited: (value) => panel.roundLabel = value
                        }
                    }

                    ActionButton {
                        Layout.fillWidth: true
                        Layout.topMargin: Theme.spaceXs
                        text: qsTr("Keep Round Data")
                        onClicked: panel.keepRoundDataRequested()

                        ToolTip.visible: hovered
                        ToolTip.delay: Theme.animSlow
                        ToolTip.text: qsTr("Store the current round's points under the round name above.")
                    }

                    Item { Layout.fillHeight: true }
                }

                SectionFrame {
                    Layout.preferredWidth: sideCards.cardWidth
                    Layout.fillHeight: true

                    SectionCaption { text: qsTr("Aggregation search") }

                    GridLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: false
                        columns: 4
                        columnSpacing: Theme.spaceSm
                        rowSpacing: Theme.spaceXs

                        FieldCaption { text: qsTr("IH Min") }
                        NumberField {
                            Layout.fillWidth: true
                            value: panel.ihMinInterval
                            onEdited: (value) => panel.ihMinInterval = value
                        }
                        FieldCaption { text: qsTr("IH Max") }
                        NumberField {
                            Layout.fillWidth: true
                            value: panel.ihMaxInterval
                            onEdited: (value) => panel.ihMaxInterval = value
                        }

                        FieldCaption { text: qsTr("Window") }
                        NumberField {
                            Layout.fillWidth: true
                            value: panel.windowInterval
                            onEdited: (value) => panel.windowInterval = value
                        }
                        FieldCaption { text: qsTr("Step") }
                        NumberField {
                            Layout.fillWidth: true
                            value: panel.stepInterval
                            onEdited: (value) => panel.stepInterval = value
                        }
                    }

                    ActionButton {
                        Layout.fillWidth: true
                        Layout.topMargin: Theme.spaceXs
                        text: qsTr("Min Aggregation by Interval")
                        onClicked: panel.minAggrByIntervalRequested()

                        ToolTip.visible: hovered
                        ToolTip.delay: Theme.animSlow
                        ToolTip.text: qsTr("Slide a window of the given size across the IH interval and keep the lowest aggregation of each step.")
                    }

                    HSeparator {}

                    GridLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: false
                        columns: 4
                        columnSpacing: Theme.spaceSm
                        rowSpacing: Theme.spaceXs

                        ColumnCaption { Layout.fillWidth: true; text: qsTr("IH Min") }
                        ColumnCaption { Layout.fillWidth: true; text: qsTr("IH Max") }
                        ColumnCaption { Layout.fillWidth: true; text: qsTr("Aggr") }
                        ColumnCaption { Layout.fillWidth: true; text: qsTr("Distance") }

                        NumberField {
                            Layout.fillWidth: true
                            value: panel.targetIhMin
                            onEdited: (value) => panel.targetIhMin = value
                        }
                        NumberField {
                            Layout.fillWidth: true
                            value: panel.targetIhMax
                            onEdited: (value) => panel.targetIhMax = value
                        }
                        NumberField {
                            Layout.fillWidth: true
                            value: panel.targetAggregation
                            onEdited: (value) => panel.targetAggregation = value
                        }
                        NumberField {
                            Layout.fillWidth: true
                            value: panel.targetDistance
                            onEdited: (value) => panel.targetDistance = value
                        }
                    }

                    Item { Layout.fillHeight: true }

                    ActionButton {
                        Layout.fillWidth: true
                        Layout.topMargin: Theme.spaceXs
                        text: qsTr("Aggr by Range and Distance")
                        tone: "accent"
                        onClicked: panel.aggrByRangeAndDistanceRequested()

                        ToolTip.visible: hovered
                        ToolTip.delay: Theme.animSlow
                        ToolTip.text: qsTr("Score one IH window at one distance and report its aggregation.")
                    }
                }
            }
        }

        SectionFrame {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 0

            SectionCaption {
                Layout.fillWidth: false
                Layout.bottomMargin: 0
                text: qsTr("Ranges")
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spaceSm

                ActionButton {
                    text: qsTr("Range Window")
                    onClicked: panel.rangeWindowRequested()

                    ToolTip.visible: hovered
                    ToolTip.delay: Theme.animSlow
                    ToolTip.text: qsTr("Fill Range_1 to Range_20 with IH windows stepped across the interval above.")
                }

                Item { Layout.fillWidth: true }

                PatternToggleSwitch {
                    text: qsTr("History Distance")
                    checked: panel.historyDistance
                    onToggled: (value) => panel.historyDistance = value
                }

                ActionButton {
                    text: qsTr("Save Distance History")
                    enabled: panel.historyDistance
                    onClicked: panel.saveHistoryDistanceRequested()

                    ToolTip.visible: hovered
                    ToolTip.delay: Theme.animSlow
                    ToolTip.text: qsTr("Append the current per-range distances to the history file.")
                }
            }

            RowLayout {
                id: rangeGrid

                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 0
                Layout.topMargin: Theme.spaceXs
                spacing: metrics.cellSpacing

                ColumnLayout {
                    Layout.fillWidth: false
                    Layout.fillHeight: true
                    Layout.preferredWidth: metrics.captionWidth
                    spacing: metrics.cellSpacing

                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: metrics.headerHeight
                    }

                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.preferredHeight: metrics.cellHeight

                        CheckSquare {
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            checked: panel.allRangesEnabled
                            onToggled: (checked) => panel.setAllRangesEnabled(checked)

                            ToolTip.visible: hovered
                            ToolTip.delay: Theme.animSlow
                            ToolTip.text: qsTr("Enable or disable every range at once.")
                        }
                    }

                    Repeater {
                        model: panel.rowCaptions

                        Label {
                            id: caption

                            required property string modelData

                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.preferredHeight: metrics.cellHeight
                            text: caption.modelData
                            color: Theme.textCaption
                            font.pixelSize: Theme.captionFontSize
                            horizontalAlignment: Text.AlignRight
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideLeft
                        }
                    }
                }

                Flickable {
                    id: grid

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.alignment: Qt.AlignTop

                    clip: true
                    contentWidth: columns.width
                    contentHeight: columns.height
                    flickableDirection: Flickable.HorizontalFlick
                    boundsBehavior: Flickable.StopAtBounds

                    ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded }

                    Row {
                        id: columns

                        spacing: metrics.cellSpacing

                        Repeater {
                            model: panel.ranges.length

                            CaliRangeColumn {
                                id: rangeColumn

                                required property int index

                                width: metrics.cellWidth
                                height: grid.height

                                rangeIndex: rangeColumn.index
                                values: panel.ranges[rangeColumn.index] || panel.blankRange

                                headerHeight: metrics.headerHeight
                                cellHeight: metrics.cellHeight
                                cellSpacing: metrics.cellSpacing

                                onEnableToggled: (enabled) => panel.setRangeEnabled(rangeColumn.index, enabled)
                                onEdited: (field, value) => panel.setRangeField(rangeColumn.index, field, value)
                                onGraphRequested: panel.zflIhGraphRequested(rangeColumn.index)
                            }
                        }
                    }
                }
            }
        }
    }
}
