pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FisheyeCaliJojo

Rectangle {
    id: panel

    readonly property real alphaMax: 90
    readonly property real ictMax: 2500

    property var alphaRounds: []
    property var alphaFit: []
    property var zflRounds: []
    property var ihBands: []

    property string cameraName: ""
    property string cameraFov: ""
    property string cameraSensorWidth: "1"
    property string cameraSensorHeight: "1"
    property string iCx: ""
    property string iCy: ""
    property string ratio: "1"
    property string imageWidth: ""
    property string imageHeight: ""
    property string calibrationRatio: ""
    property string distancePerRound: ""

    property var coefficients: ["0", "0", "", "", "", ""]

    readonly property var coefficientRows: [
        { name: "parameter0", note: qsTr("always 0"), fitted: false },
        { name: "parameter1", note: qsTr("always 0"), fitted: false },
        { name: "parameter2", note: "c₄", fitted: true },
        { name: "parameter3", note: "c₃", fitted: true },
        { name: "parameter4", note: "c₂", fitted: true },
        { name: "parameter5", note: "c₁", fitted: true }
    ]

    signal updateAllRequested()
    signal updateIhAlphaRequested()
    signal updateIhZflRequested()
    signal saveParametersRequested()
    signal saveConfigurationRequested()

    function roundColor(index) {
        return Theme.curvePalette[index % Theme.curvePalette.length]
    }

    function bandColor(index) {
        const base = Qt.color(Theme.curvePalette[index % Theme.curvePalette.length])
        return Qt.rgba(base.r, base.g, base.b, 0.14)
    }

    function setCoefficient(index, value) {
        const next = panel.coefficients.slice()
        next[index] = value
        panel.coefficients = next
    }

    readonly property var roundLegend: {
        const out = []
        for (let r = 0; r < panel.alphaRounds.length; ++r)
            out.push({ color: panel.roundColor(r), label: qsTr("Round %1").arg(r + 1) })
        return out
    }

    readonly property var alphaCurves: {
        const out = []
        for (let r = 0; r < panel.alphaRounds.length; ++r)
            out.push({ color: panel.roundColor(r), points: panel.alphaRounds[r], style: "scatter" })
        if (panel.alphaFit.length > 0)
            out.push({ color: Theme.textPrimary, points: panel.alphaFit })
        return out
    }

    readonly property var alphaLegend: {
        const out = panel.roundLegend.slice()
        if (panel.alphaFit.length > 0)
            out.push({ color: Theme.textPrimary, label: qsTr("Fit") })
        return out
    }

    readonly property var zflCurves: {
        const out = []
        for (let r = 0; r < panel.zflRounds.length; ++r)
            out.push({ color: panel.roundColor(r), points: panel.zflRounds[r], style: "scatter" })
        return out
    }

    readonly property var zflRegions: {
        const out = []
        for (let i = 0; i < panel.ihBands.length; ++i)
            out.push({ min: panel.ihBands[i].min,
                       max: panel.ihBands[i].max,
                       color: panel.bandColor(i) })
        return out
    }

    color: Theme.panelBackground
    border.color: Theme.panelBorder
    radius: Theme.radius

    component ParamField: RowLayout {
        id: field

        property string caption: ""
        property string unit: ""
        property string note: ""
        property color noteColor: Theme.textCaption
        property var value: ""
        property bool numeric: true

        signal edited(string value)

        Layout.fillWidth: true
        spacing: Theme.labelSpacing

        DecimalValidator { id: decimal }

        Label {
            Layout.fillWidth: true
            text: field.caption
            color: Theme.textCaption
            font.pixelSize: Theme.captionFontSize
            elide: Text.ElideRight
        }

        Label {
            text: field.note
            color: field.noteColor
            font.pixelSize: Theme.captionFontSize
            font.bold: field.note.length > 0 && field.noteColor === Theme.accent
        }

        ValueField {
            Layout.preferredWidth: Math.round(Theme.charUnit * 11)
            Layout.preferredHeight: Theme.controlHeight
            editable: true
            value: field.value
            validator: field.numeric ? decimal : null
            onEdited: (value) => field.edited(value)
        }

        Label {
            Layout.preferredWidth: Math.round(Theme.charUnit * 3)
            text: field.unit
            color: Theme.textCaption
            font.pixelSize: Theme.captionFontSize
        }
    }

    component SectionTitle: Label {
        Layout.fillWidth: true
        color: Theme.accent
        font.bold: true
        font.pixelSize: Theme.captionFontSize
    }

    ColumnLayout {
        id: content

        anchors.fill: parent
        anchors.margins: Theme.panelMargin
        spacing: Theme.rowSpacing

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spaceSm

            ActionButton {
                text: qsTr("Update All Cali Result")
                tone: "accent"
                onClicked: panel.updateAllRequested()

                ToolTip.visible: hovered
                ToolTip.delay: Theme.animSlow
                ToolTip.text: qsTr("Recompute every round, refit the polynomial, and redraw both plots.")
            }

            Label {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.spaceSm
                text: qsTr("Fit a curve through every round's measurements, then save the six numbers that describe this camera.")
                color: Theme.textCaption
                font.pixelSize: Theme.captionFontSize
                elide: Text.ElideRight
            }

            ActionButton {
                id: aboutButton

                text: qsTr("About the fit")
                onClicked: aboutPopup.open()

                ToolTip.visible: hovered
                ToolTip.delay: Theme.animSlow
                ToolTip.text: qsTr("Show how the polynomial coefficients map onto the parameter fields.")

                Popup {
                    id: aboutPopup

                    y: aboutButton.height + Theme.spaceXs
                    x: -width + aboutButton.width
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
                            text: qsTr("How the camera parameters are fitted")
                            font.bold: true
                            font.pixelSize: Theme.fontTitle
                            color: Theme.accent
                        }

                        Repeater {
                            model: [
                                { caption: qsTr("Image height against incidence angle"),
                                  formula: "IH(α) = c₀ + c₁·α + c₂·α² + c₃·α³ + c₄·α⁴" },
                                { caption: qsTr("Fitted by"),
                                  formula: qsTr("Least squares, degree 4, over every enabled round pooled") },
                                { caption: qsTr("Coefficient to field"),
                                  formula: "parameter5 = c₁    parameter4 = c₂    parameter3 = c₃    parameter2 = c₄" },
                                { caption: qsTr("Unused slots"),
                                  formula: qsTr("parameter0 and parameter1 stay 0, and c₀ is dropped because IH is 0 at α = 0") },
                                { caption: qsTr("Units"),
                                  formula: qsTr("The plot draws α in degrees, but the coefficients are fitted against α in radians") },
                                { caption: qsTr("Order of work"),
                                  formula: qsTr("Load the rounds, press Update All Cali Result, then Save Parameters") }
                            ]

                            ColumnLayout {
                                id: aboutEntry

                                required property var modelData

                                Layout.fillWidth: true
                                spacing: 0

                                Label {
                                    text: aboutEntry.modelData.caption
                                    color: Theme.textCaption
                                    font.pixelSize: Theme.captionFontSize
                                }
                                Label {
                                    text: aboutEntry.modelData.formula
                                    color: Theme.textPrimary
                                    font.bold: true
                                }
                            }
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.panelGap

            PlotBlock {
                Layout.fillWidth: true
                Layout.fillHeight: true

                title: qsTr("IH-Alpha")
                legend: panel.alphaLegend
                xLabel: qsTr("ALPHA (degree)")
                yLabel: qsTr("IH (pixel)")
                xMin: 0
                xMax: panel.alphaMax
                yMin: 0
                yMax: 2000
                curves: panel.alphaCurves
                actionText: qsTr("Update IH-Alpha")

                onActionTriggered: panel.updateIhAlphaRequested()
            }

            PlotBlock {
                Layout.fillWidth: true
                Layout.fillHeight: true

                title: qsTr("ZFL-IH")
                legend: panel.roundLegend
                xLabel: qsTr("ICT (pixel)")
                yLabel: qsTr("ZFL (pixel)")
                xMin: -panel.ictMax
                xMax: panel.ictMax
                yMin: -600
                yMax: 3000
                curves: panel.zflCurves
                regions: panel.zflRegions
                actionText: qsTr("Update IH-ZFL")

                onActionTriggered: panel.updateIhZflRequested()
            }

            ColumnLayout {
                Layout.preferredWidth: Math.round(Theme.charUnit * 42)
                Layout.minimumWidth: Math.round(Theme.charUnit * 38)
                Layout.fillHeight: true
                spacing: Theme.rowSpacing

                ScrollView {
                    id: paramScroll

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    contentWidth: availableWidth
                    clip: true

                    ColumnLayout {
                        width: paramScroll.availableWidth
                        spacing: Theme.rowSpacing

                        SectionFrame {
                            Layout.fillWidth: true

                            SectionTitle { text: qsTr("Camera") }

                            ParamField {
                                caption: qsTr("cameraName")
                                numeric: false
                                value: panel.cameraName
                                onEdited: (value) => panel.cameraName = value
                            }
                            ParamField {
                                caption: qsTr("cameraFov")
                                unit: "°"
                                value: panel.cameraFov
                                onEdited: (value) => panel.cameraFov = value
                            }
                            ParamField {
                                caption: qsTr("cameraSensorWidth")
                                unit: qsTr("mm")
                                value: panel.cameraSensorWidth
                                onEdited: (value) => panel.cameraSensorWidth = value
                            }
                            ParamField {
                                caption: qsTr("cameraSensorHeight")
                                unit: qsTr("mm")
                                value: panel.cameraSensorHeight
                                onEdited: (value) => panel.cameraSensorHeight = value
                            }
                        }

                        SectionFrame {
                            Layout.fillWidth: true

                            SectionTitle { text: qsTr("Image") }

                            ParamField {
                                caption: qsTr("iCx")
                                unit: qsTr("px")
                                value: panel.iCx
                                onEdited: (value) => panel.iCx = value
                            }
                            ParamField {
                                caption: qsTr("iCy")
                                unit: qsTr("px")
                                value: panel.iCy
                                onEdited: (value) => panel.iCy = value
                            }
                            ParamField {
                                caption: qsTr("ratio")
                                value: panel.ratio
                                onEdited: (value) => panel.ratio = value
                            }
                            ParamField {
                                caption: qsTr("imageWidth")
                                unit: qsTr("px")
                                value: panel.imageWidth
                                onEdited: (value) => panel.imageWidth = value
                            }
                            ParamField {
                                caption: qsTr("imageHeight")
                                unit: qsTr("px")
                                value: panel.imageHeight
                                onEdited: (value) => panel.imageHeight = value
                            }
                            ParamField {
                                caption: qsTr("calibrationRatio")
                                value: panel.calibrationRatio
                                onEdited: (value) => panel.calibrationRatio = value
                            }
                        }

                        SectionFrame {
                            Layout.fillWidth: true

                            SectionTitle { text: qsTr("Polynomial coefficients") }

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Filled in by Update IH-Alpha, editable by hand.")
                                color: Theme.textCaption
                                font.pixelSize: Theme.captionFontSize
                                wrapMode: Text.WordWrap
                            }

                            Repeater {
                                model: panel.coefficientRows

                                ParamField {
                                    id: coefficientField

                                    required property int index
                                    required property var modelData

                                    caption: coefficientField.modelData.name
                                    note: coefficientField.modelData.note
                                    noteColor: coefficientField.modelData.fitted ? Theme.accent
                                                                                 : Theme.textDisabled
                                    value: panel.coefficients[coefficientField.index]
                                    onEdited: (value) => panel.setCoefficient(coefficientField.index, value)
                                }
                            }
                        }

                        SectionFrame {
                            Layout.fillWidth: true

                            SectionTitle { text: qsTr("Configuration") }

                            ParamField {
                                caption: qsTr("Distance / Round")
                                unit: qsTr("px")
                                value: panel.distancePerRound
                                onEdited: (value) => panel.distancePerRound = value
                            }

                            ActionButton {
                                Layout.fillWidth: true
                                text: qsTr("Save Configuration")
                                onClicked: panel.saveConfigurationRequested()

                                ToolTip.visible: hovered
                                ToolTip.delay: Theme.animSlow
                                ToolTip.text: qsTr("Write the calibration system and distance per round to main.json.")
                            }
                        }
                    }
                }

                ActionButton {
                    Layout.fillWidth: true
                    text: qsTr("Save Parameters")
                    tone: "accent"
                    onClicked: panel.saveParametersRequested()

                    ToolTip.visible: hovered
                    ToolTip.delay: Theme.animSlow
                    ToolTip.text: qsTr("Write every field above to a camera parameter JSON file.")
                }
            }
        }
    }
}
