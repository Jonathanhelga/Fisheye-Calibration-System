pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FisheyeCaliJojo

// Aggregation: the old client's tab_aggr_by_sitance_and_range, ported.
//
// This is the range workspace -- Global plus Range_1..20 as columns, the pattern
// gaps, the sensor/round fields, and the per-IH-range result table. Its five
// buttons are the old client's, name for name:
//
//   btn_range_window                -> Range Window
//   btn_min_aggregation_by_interval -> Min Aggregation by Interval
//   btn_aggr_by_range_and_distance  -> Aggr by Range and Distance
//   btn_keep_round_data             -> Keep Round Data
//   btn_save_history_distance       -> Save Distance History
//
// The searches live HERE, not in Overlap. In the old controller
// `minAggregationByInterval()` and `aggrByRangeAndDistance()` are what fill
// `distAggrSamples_`; Overlap's "Update Dist vs. Aggr" only redraws them. That
// is why this panel carries the Stop button and the progress strip: these are
// the only cancellable jobs in the window, and the target search is 282 probes
// each recomputing all eleven rounds -- minutes, not seconds.
//
// IH FIELDS ARE PERCENTAGES, THE OPS TAKE PIXELS. The old client converts with
//     xLo = pct / 100 * maxIctAllRounds()
// before every call (see aggrByRangeAndDistance). Passing the percentages
// straight through is the silent failure to watch for here: 0..100 is a valid
// pixel window, so the search runs, converges, and returns a plausible wrong
// distance with no error anywhere. windowXLo/windowXHi below do the conversion
// once so no caller can forget it.
Rectangle {
    id: panel

    readonly property int rangeCount: 20
    readonly property int resultRowCount: 137
    readonly property int visibleResultRows: 10

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

    // Search state, bound to CalibrationController by the window. Kept as plain
    // properties so the panel still lays out and can be read without a rig.
    property bool searching: false
    property bool busy: false
    property string searchStage: ""
    property string searchSummary: ""
    property int searchDone: 0
    property int searchTotal: 0

    // maxIctAllRounds() in the old client. Zero means no IH data has been
    // loaded, which is exactly when a percent->pixel conversion must NOT run.
    property real maxIct: 0

    readonly property var blankRange: panel.emptyRange("")
    readonly property var blankResult: panel.emptyResult()

    // ---- percent -> pixel, done once so no caller can forget it -------------

    function pixelFor(percentText) {
        const pct = panel.numberOf(percentText)
        if (pct === null || !(panel.maxIct > 0))
            return NaN
        return pct / 100 * panel.maxIct
    }

    function numberOf(text) {
        const value = Number(String(text).trim())
        return String(text).trim().length > 0 && isFinite(value) ? value : null
    }

    // The IH window the "Aggr by Range and Distance" group asks for, in pixels.
    readonly property real windowXLo: panel.pixelFor(panel.targetIhMin)
    readonly property real windowXHi: panel.pixelFor(panel.targetIhMax)

    // The old client rejected anything outside 0..100 with min<max before
    // converting, and so do we -- an inverted window silently scores nothing.
    readonly property bool windowValid: {
        const lo = panel.numberOf(panel.targetIhMin)
        const hi = panel.numberOf(panel.targetIhMax)
        return lo !== null && hi !== null && lo >= 0 && hi <= 100 && lo < hi
               && panel.maxIct > 0
    }

    // Blank distance means "search for the target aggregation instead"; that is
    // the branch the old aggrByRangeAndDistance() takes on an empty line-edit.
    readonly property real requestedDistance: {
        const d = panel.numberOf(panel.targetDistance)
        return d === null ? NaN : d
    }
    readonly property real requestedTarget: {
        const t = panel.numberOf(panel.targetAggregation)
        return t === null ? NaN : t
    }
    readonly property bool hasRequestedDistance: !isNaN(panel.requestedDistance)

    // The sliding-window interval, also in pixels.
    readonly property real intervalXLo: panel.pixelFor(panel.ihMinInterval)
    readonly property real intervalXHi: panel.pixelFor(panel.ihMaxInterval)
    readonly property bool intervalValid: !isNaN(panel.intervalXLo) && !isNaN(panel.intervalXHi)
                                          && panel.intervalXLo < panel.intervalXHi

    readonly property bool allRangesEnabled: {
        for (let i = 0; i < panel.ranges.length; ++i)
            if (!panel.ranges[i].enabled)
                return false
        return panel.ranges.length > 0
    }

    signal rangeWindowRequested()
    signal aggrByRangeAndDistanceRequested()
    signal minAggrByIntervalRequested()
    signal stopRequested()
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

    // Ports rangeWindow(): step a window of `windowInterval` across
    // [ihMinInterval, ihMaxInterval] by `stepInterval` and write the resulting
    // IH Min/Max into Range_1..Range_20.
    //
    // The old client read these four numbers out of a JSON file chosen from a
    // dialog (range_min / range_max / step / window); the New-UI layout puts
    // them on the panel instead, which is the same arithmetic without the file.
    // Range 0 is Global and is deliberately not touched.
    function fillRangeWindows() {
        const rangeMin = panel.numberOf(panel.ihMinInterval)
        const rangeMax = panel.numberOf(panel.ihMaxInterval)
        const step = panel.numberOf(panel.stepInterval)
        const window = panel.numberOf(panel.windowInterval)
        if (rangeMin === null || rangeMax === null || step === null || window === null)
            return false
        if (!(step > 0) || !(window > 0) || rangeMax < rangeMin)
            return false

        // Whole numbers stay whole -- the old client's fmt() did this so the
        // filled cells read like the ones an operator types by hand.
        const fmt = (v) => Math.abs(v - Math.round(v)) < 1e-9
                             ? String(Math.round(v)) : String(Number(v.toPrecision(7)))

        const rows = panel.ranges.slice()
        let curMin = rangeMin
        for (let i = 1; i <= panel.rangeCount && i < rows.length; ++i) {
            rows[i] = Object.assign({}, rows[i], {
                ihMin: fmt(curMin),
                ihMax: fmt(Math.min(curMin + window, rangeMax))
            })
            curMin += step
        }
        panel.ranges = rows
        return true
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
    clip: true

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

        // The old client ran these searches behind a modal progress dialog with
        // a Cancel button. This is the same affordance without the modality --
        // the searches are minutes long and blocking the whole window for them
        // is what made the old one feel hung.
        RowLayout {
            id: searchStrip

            Layout.fillWidth: true
            spacing: Theme.spaceSm

            Label {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredWidth: 0
                text: panel.searching
                        ? qsTr("%1 ...").arg(panel.searchStage)
                        : panel.searchSummary !== ""
                            ? panel.searchSummary
                            : qsTr("Pick a search. These run for minutes and can be stopped.")
                color: panel.searching ? Theme.textPrimary : Theme.textCaption
                font.pixelSize: Theme.captionFontSize
                elide: Text.ElideRight
            }

            // Indeterminate when the op cannot report a total, rather than a 0%
            // bar that looks stuck.
            ProgressBar {
                Layout.preferredWidth: Math.round(Theme.charUnit * 24)
                visible: panel.searching
                indeterminate: panel.searchTotal <= 0
                from: 0
                to: Math.max(1, panel.searchTotal)
                value: panel.searchDone
            }

            ActionButton {
                text: qsTr("Stop")
                tone: "danger"
                enabled: panel.searching
                onClicked: panel.stopRequested()

                ToolTip.visible: hovered
                ToolTip.delay: Theme.animSlow
                ToolTip.text: qsTr("Unwinds the search on the rig. The table comes back at "
                                 + "whatever the last probe wrote, and the answer is marked "
                                 + "partial.")
            }
        }

        RowLayout {
            id: topRow

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: 1
            Layout.minimumHeight: 0
            spacing: Theme.panelGap

            SectionFrame {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 0
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
                    Layout.fillHeight: true
                    Layout.minimumHeight: 0
                    Layout.preferredHeight: panel.visibleResultRows * metrics.cellHeight
                                            + (panel.visibleResultRows - 1) * metrics.cellSpacing

                    clip: true
                    interactive: true
                    spacing: metrics.cellSpacing
                    boundsBehavior: Flickable.StopAtBounds
                    model: panel.rangeResults.length

                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

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
                        enabled: !panel.searching && !panel.busy && panel.intervalValid
                        onClicked: panel.minAggrByIntervalRequested()

                        ToolTip.visible: hovered
                        ToolTip.delay: Theme.animSlow
                        ToolTip.text: panel.maxIct > 0
                            ? qsTr("Slide a window of the given size across the IH interval and keep the lowest aggregation of each step.")
                            : qsTr("Load a table first -- the IH percentages cannot be turned into pixels until a round has data.")
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

                    // Two ops behind one button, exactly as the old client had
                    // it: a Distance in the box means "score that distance"; an
                    // empty Distance with an Aggregation means "search for the
                    // distance that hits it". The window reads
                    // hasRequestedDistance to pick.
                    ActionButton {
                        Layout.fillWidth: true
                        Layout.topMargin: Theme.spaceXs
                        text: qsTr("Aggr by Range and Distance")
                        tone: "accent"
                        enabled: !panel.searching && !panel.busy && panel.windowValid
                                 && (panel.hasRequestedDistance || !isNaN(panel.requestedTarget))
                        onClicked: panel.aggrByRangeAndDistanceRequested()

                        ToolTip.visible: hovered
                        ToolTip.delay: Theme.animSlow
                        ToolTip.text: !panel.windowValid
                            ? qsTr("Fill IH Min and IH Max with 0..100, min below max, and load a table first.")
                            : panel.hasRequestedDistance
                                ? qsTr("Score one IH window at the distance given and report its aggregation.")
                                : qsTr("Search the distance whose aggregation matches the target. "
                                     + "282 probes, each recomputing all eleven rounds -- minutes, not seconds.")
                    }
                }
            }
        }

        SectionFrame {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: 1
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
                    onClicked: {
                        if (panel.fillRangeWindows())
                            panel.rangeWindowRequested()
                    }

                    ToolTip.visible: hovered
                    ToolTip.delay: Theme.animSlow
                    ToolTip.text: qsTr("Fill Range_1 to Range_20 with IH windows stepped across the interval above. "
                                     + "Needs IH Min, IH Max, Window and Step in the Aggregation search box.")
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
