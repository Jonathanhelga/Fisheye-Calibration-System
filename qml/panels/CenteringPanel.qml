import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FisheyeCaliJojo

Rectangle {
    id: root

    readonly property int modeAuto: 0
    readonly property int modeManual: 1
    readonly property int modeLocked: 2

    property alias mode: modeSelector.currentIndex
    readonly property bool locked: mode === modeLocked

    property int posThreshold: 150
    property int negThreshold: 150
    property int centerRoi: 100

    property int positiveCpx: -1
    property int positiveCpy: -1
    property int negativeCpx: -1
    property int negativeCpy: -1

    property bool positiveEdge: false
    property bool negativeEdge: false
    property int positiveEdgeRadius: 0
    property int negativeEdgeRadius: 0
    property int positiveEdgeThickness: 10
    property int negativeEdgeThickness: 10

    readonly property color positiveEdgeColor: Theme.statusOk
    readonly property color negativeEdgeColor: Theme.danger

    readonly property bool hasPositiveCenter: positiveCpx >= 0 && positiveCpy >= 0
    readonly property bool hasNegativeCenter: negativeCpx >= 0 && negativeCpy >= 0

    // 0 lets auto_center read the prepared PNG.
    property int expectedRings: 0
    property bool noiseCleaning: false

    property string lastMethod: ""
    property string lastConfidence: ""
    property string refusalReason: ""

    readonly property bool linked: ComputeController.status === ProbeStatus.Ok
    readonly property bool computing: ComputeController.busy

    // Ok means a trusted fit, nothing else.
    readonly property int detectState:
          computing                     ? ProbeStatus.Checking
        : !linked                       ? ProbeStatus.Unknown
        : refusalReason !== ""          ? ProbeStatus.Failed
        : lastConfidence === "good"     ? ProbeStatus.Ok
        : lastConfidence === "marginal" ? ProbeStatus.Partial
                                        : ProbeStatus.Unknown

    readonly property string detectStatus:
          computing                     ? ComputeController.activity
        : !linked                       ? qsTr("Compute node not connected")
        : refusalReason !== ""          ? qsTr("No centre found")
        : lastConfidence === "good"     ? qsTr("Centre found by %1").arg(lastMethod)
        : lastConfidence === "marginal" ? qsTr("Marginal fit, do not drive on it")
        : lastMethod !== ""             ? qsTr("Picked by hand, not verified")
                                        : qsTr("Ready")

    // Full sentence, too long for the strip.
    readonly property string detectDetail:
          refusalReason !== ""  ? refusalReason
        : lastConfidence !== "" ? qsTr("%1, %2 confidence").arg(lastMethod).arg(lastConfidence)
                                : ""

    readonly property color detectColor:
          detectState === ProbeStatus.Failed  ? Theme.danger
        : detectState === ProbeStatus.Partial ? Theme.statusPartial
                                              : Theme.textCaption

    signal centerChanged(string target, int x, int y)

    function slotFor(target) {
        return target === "Positive" ? "positive" : target === "Negative" ? "negative" : ""
    }

    function thresholdFor(target) {
        return target === "Negative" ? negThreshold : posThreshold
    }

    // No caller since 2026-09-09. Kept for the cascade.
    function findCenter(target) {
        const slot = slotFor(target)
        if (slot === "" || locked)
            return
        refusalReason = ""
        ComputeController.autoCenter(slot, expectedRings, noiseCleaning)
    }

    // Accessors, so polarities cannot be mixed up.
    function edgeShown(target) {
        return target === "Positive" ? positiveEdge
             : target === "Negative" ? negativeEdge
                                     : false
    }

    function edgeRadius(target) {
        return target === "Positive" ? positiveEdgeRadius
             : target === "Negative" ? negativeEdgeRadius
                                     : 0
    }

    function edgeThickness(target) {
        return target === "Positive" ? positiveEdgeThickness
             : target === "Negative" ? negativeEdgeThickness
                                     : 1
    }

    function edgeColor(target) {
        return target === "Positive" ? positiveEdgeColor
             : target === "Negative" ? negativeEdgeColor
                                     : "transparent"
    }

    function centerX(target) {
        return target === "Positive" ? positiveCpx
             : target === "Negative" ? negativeCpx
                                     : -1
    }

    function centerY(target) {
        return target === "Positive" ? positiveCpy
             : target === "Negative" ? negativeCpy
                                     : -1
    }

    // Optional; omitted it stays "picked by hand".
    function setCenter(target, x, y, method) {
        if (locked)
            return
        if (target === "Positive") {
            positiveCpx = x
            positiveCpy = y
        } else if (target === "Negative") {
            negativeCpx = x
            negativeCpy = y
        } else {
            return
        }
        refusalReason = ""
        lastMethod = (method !== undefined && method !== "") ? method : qsTr("picked by hand")
        lastConfidence = ""
        centerChanged(target, x, y)

        // Manual seeds roi_exact; Locked sends nothing.
        if (mode === modeManual && linked)
            ComputeController.refineCenter(slotFor(target), x, y, thresholdFor(target))
    }

    Connections {
        target: ComputeController

        function onCenterFound(slot, x, y, method, confidence) {
            if (root.locked)
                return
            // Direct, not setCenter, which would loop.
            if (slot === "positive") {
                root.positiveCpx = x
                root.positiveCpy = y
                root.centerChanged("Positive", x, y)
            } else if (slot === "negative") {
                root.negativeCpx = x
                root.negativeCpy = y
                root.centerChanged("Negative", x, y)
            } else {
                return
            }
            root.refusalReason = ""
            root.lastMethod = method
            root.lastConfidence = confidence
        }

        function onCenterRefused(slot, reason) {
            // A refused fit is cleared, not left stale.
            if (slot === "positive") {
                root.positiveCpx = -1
                root.positiveCpy = -1
            } else if (slot === "negative") {
                root.negativeCpx = -1
                root.negativeCpy = -1
            }
            root.lastMethod = ""
            root.lastConfidence = ""
            root.refusalReason = reason
        }
    }

    implicitWidth: content.implicitWidth + 2 * Theme.panelMargin + Theme.rowSpacing
    implicitHeight: content.implicitHeight + 2 * Theme.panelMargin

    color: Theme.panelBackground
    border.color: Theme.panelBorder
    radius: Theme.radius

    ColumnLayout {
        id: content

        anchors.fill: parent
        anchors.margins: Theme.panelMargin
        spacing: Theme.rowSpacing

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.rowSpacing

            Label {
                text: qsTr("Centering")
                font.bold: true
                font.pixelSize: Theme.fontTitle
                color: Theme.accent
            }

            Item { Layout.fillWidth: true }

            SegmentedControl {
                id: modeSelector
                model: [qsTr("Auto"), qsTr("Manual"), qsTr("Locked")]
                currentIndex: root.modeAuto
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.rowSpacing

            StatusDot {
                Layout.alignment: Qt.AlignVCenter
                status: root.detectState
            }

            Label {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredWidth: 0
                text: root.detectStatus
                color: root.detectColor
                font.pixelSize: Theme.captionFontSize
                elide: Text.ElideRight

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: root.detectDetail !== ""
                    acceptedButtons: Qt.NoButton

                    ToolTip.visible: containsMouse
                    ToolTip.delay: Theme.animSlow
                    ToolTip.text: root.detectDetail
                }
            }

            // Find Pos/Neg removed 2026-09-09; findCenter() remains.
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.rowSpacing

            LabeledField {
                Layout.fillWidth: true
                label: qsTr("Positive Threshold")
                text: root.posThreshold
                validator: IntValidator { bottom: 0; top: 255 }
                onEdited: (value) => root.posThreshold = parseInt(value)
            }

            LabeledField {
                Layout.fillWidth: true
                label: qsTr("Center ROI")
                text: root.centerRoi
                validator: IntValidator { bottom: 1; top: 9999 }
                onEdited: (value) => root.centerRoi = parseInt(value)
            }

            LabeledField {
                Layout.fillWidth: true
                label: qsTr("Negative Threshold")
                text: root.negThreshold
                validator: IntValidator { bottom: 0; top: 255 }
                onEdited: (value) => root.negThreshold = parseInt(value)
            }

            LabeledField {
                Layout.fillWidth: true
                label: qsTr("Rings")
                text: root.expectedRings
                validator: IntValidator { bottom: 0; top: 99 }
                // 0 reads the count from the prepared PNG.
                onEdited: (value) => root.expectedRings = parseInt(value)
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.rowSpacing

            PatternToggleSwitch {
                text: qsTr("Noise cleaning")
                checked: root.noiseCleaning
                onToggled: (value) => root.noiseCleaning = value
            }

            Item { Layout.fillWidth: true }

            Label {
                text: root.mode === root.modeAuto
                        ? qsTr("Auto: a shot centres on the middle of its frame")
                        : root.mode === root.modeManual
                            ? qsTr("Manual: type a centre, or click to seed roi_exact")
                            : qsTr("Locked: centres are read-only")
                color: Theme.textCaption
                font.pixelSize: Theme.captionFontSize
            }
        }

        SectionFrame {
            Layout.fillWidth: true

            GridLayout {
                Layout.fillWidth: true
                columns: 3
                columnSpacing: Theme.rowSpacing
                rowSpacing: Theme.labelSpacing

                Item { Layout.preferredWidth: Math.round(Theme.charUnit * 8) }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("CPX")
                    font.bold: true
                    font.pixelSize: Theme.captionFontSize
                    color: Theme.textPrimary
                    horizontalAlignment: Text.AlignHCenter
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("CPY")
                    font.bold: true
                    font.pixelSize: Theme.captionFontSize
                    color: Theme.textPrimary
                    horizontalAlignment: Text.AlignHCenter
                }

                Label {
                    text: qsTr("Positive")
                    font.pixelSize: Theme.captionFontSize
                    color: Theme.textCaption
                }

                ValueField {
                    Layout.fillWidth: true
                    editable: !root.locked
                    value: root.hasPositiveCenter ? root.positiveCpx : ""
                    validator: IntValidator { bottom: 0; top: 99999 }
                    onEdited: (value) => root.setCenter("Positive", parseInt(value), root.positiveCpy)
                }

                ValueField {
                    Layout.fillWidth: true
                    editable: !root.locked
                    value: root.hasPositiveCenter ? root.positiveCpy : ""
                    validator: IntValidator { bottom: 0; top: 99999 }
                    onEdited: (value) => root.setCenter("Positive", root.positiveCpx, parseInt(value))
                }

                Label {
                    text: qsTr("Negative")
                    font.pixelSize: Theme.captionFontSize
                    color: Theme.textCaption
                }

                ValueField {
                    Layout.fillWidth: true
                    editable: !root.locked
                    value: root.hasNegativeCenter ? root.negativeCpx : ""
                    validator: IntValidator { bottom: 0; top: 99999 }
                    onEdited: (value) => root.setCenter("Negative", parseInt(value), root.negativeCpy)
                }

                ValueField {
                    Layout.fillWidth: true
                    editable: !root.locked
                    value: root.hasNegativeCenter ? root.negativeCpy : ""
                    validator: IntValidator { bottom: 0; top: 99999 }
                    onEdited: (value) => root.setCenter("Negative", root.negativeCpx, parseInt(value))
                }
            }
        }

        SectionFrame {
            Layout.fillWidth: true

            GridLayout {
                Layout.fillWidth: true
                columns: 4
                columnSpacing: Theme.rowSpacing
                rowSpacing: Theme.labelSpacing

                Label {
                    text: qsTr("Edge")
                    font.pixelSize: Theme.captionFontSize
                    color: Theme.textCaption
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Radius")
                    font.pixelSize: Theme.captionFontSize
                    color: Theme.textCaption
                    horizontalAlignment: Text.AlignHCenter
                }

                Label {
                    text: qsTr("Color")
                    font.pixelSize: Theme.captionFontSize
                    color: Theme.textCaption
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Thickness")
                    font.pixelSize: Theme.captionFontSize
                    color: Theme.textCaption
                    horizontalAlignment: Text.AlignHCenter
                }

                CheckBox {
                    checked: root.positiveEdge
                    onToggled: root.positiveEdge = checked
                }

                ValueField {
                    Layout.fillWidth: true
                    editable: true
                    value: root.positiveEdgeRadius
                    validator: IntValidator { bottom: 0; top: 9999 }
                    onEdited: (value) => root.positiveEdgeRadius = parseInt(value)
                }

                ColorSwatch { color: root.positiveEdgeColor }

                ValueField {
                    Layout.fillWidth: true
                    editable: true
                    value: root.positiveEdgeThickness
                    validator: IntValidator { bottom: 1; top: 99 }
                    onEdited: (value) => root.positiveEdgeThickness = parseInt(value)
                }

                CheckBox {
                    checked: root.negativeEdge
                    onToggled: root.negativeEdge = checked
                }

                ValueField {
                    Layout.fillWidth: true
                    editable: true
                    value: root.negativeEdgeRadius
                    validator: IntValidator { bottom: 0; top: 9999 }
                    onEdited: (value) => root.negativeEdgeRadius = parseInt(value)
                }

                ColorSwatch { color: root.negativeEdgeColor }

                ValueField {
                    Layout.fillWidth: true
                    editable: true
                    value: root.negativeEdgeThickness
                    validator: IntValidator { bottom: 1; top: 99 }
                    onEdited: (value) => root.negativeEdgeThickness = parseInt(value)
                }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
