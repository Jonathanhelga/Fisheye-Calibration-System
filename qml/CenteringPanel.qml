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

    signal centerChanged(string target, int x, int y)

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

    function setCenter(target, x, y) {
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
        centerChanged(target, x, y)
    }

    implicitWidth: content.implicitWidth + 2 * Theme.panelMargin
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
