pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import FisheyeCaliJojo

Popup {
    id: popup

    property var curves: []
    property int selectedIndex: 0

    readonly property var current: selectedIndex >= 0 && selectedIndex < curves.length
                                   ? curves[selectedIndex]
                                   : null

    // Beyond this, rows scroll.
    readonly property real curveListMaxHeight: Theme.controlHeight * 5 + Theme.rowSpacing * 4
    readonly property real anchorGap: Theme.spaceXs

    signal picked(string key, color value)
    signal cleared(string key)
    signal resetRequested()

    padding: Theme.panelMargin
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

    onAboutToShow: selectedIndex = 0

    // Opens below, flips above when short of room.
    y: {
        if (!parent)
            return anchorGap
        const win = parent.Window.window
        if (!win)
            return parent.height + anchorGap
        const anchorBottomInWindow = parent.mapToItem(null, 0, parent.height).y
        const spaceBelow = win.height - anchorBottomInWindow
        return spaceBelow >= implicitHeight + anchorGap
               ? parent.height + anchorGap
               : -implicitHeight - anchorGap
    }

    background: Rectangle {
        color: Theme.panelBackground
        border.color: Theme.panelBorder
        radius: Theme.radius
    }

    contentItem: ColumnLayout {
        spacing: Theme.rowSpacing

        Label {
            text: qsTr("Curve Color")
            font.bold: true
            font.pixelSize: Theme.fontTitle
            color: Theme.accent
        }

        Label {
            visible: popup.curves.length === 0
            text: qsTr("Select a direction first.")
            color: Theme.textCaption
            font.pixelSize: Theme.captionFontSize
        }

        RowLayout {
            visible: popup.curves.length > 0
            spacing: Theme.rowSpacing

            SectionFrame {
                Layout.alignment: Qt.AlignTop

                ScrollView {
                    id: curveListScroll

                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.min(curveListColumn.implicitHeight,
                                                      popup.curveListMaxHeight)
                    clip: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    ColumnLayout {
                        id: curveListColumn

                        width: curveListScroll.availableWidth
                        spacing: Theme.rowSpacing

                        Repeater {
                            model: popup.curves

                            AbstractButton {
                                id: row

                                required property int index
                                required property var modelData

                                readonly property bool selected: popup.selectedIndex === index

                                Layout.fillWidth: true
                                implicitHeight: Theme.controlHeight
                                hoverEnabled: true

                                onClicked: popup.selectedIndex = index

                                background: Rectangle {
                                    radius: Theme.radius
                                    color: row.selected ? Theme.fieldDisabledBackground
                                         : row.hovered ? Theme.fieldBackground
                                                       : "transparent"
                                }

                                contentItem: RowLayout {
                                    spacing: Theme.rowSpacing

                                    ColorSwatch {
                                        implicitWidth: Math.round(Theme.charUnit * 3)
                                        implicitHeight: Math.round(Theme.controlHeight * 0.6)
                                        color: row.modelData.color
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: qsTr("%1 %2").arg(row.modelData.side === "pos" ? qsTr("Pos")
                                                                                             : qsTr("Neg"))
                                                           .arg(row.modelData.direction.toUpperCase())
                                        color: row.selected ? Theme.textPrimary : Theme.textCaption
                                        font.pixelSize: Theme.captionFontSize
                                        font.bold: row.selected
                                    }

                                    ActionButton {
                                        text: qsTr("x")
                                        visible: !Qt.colorEqual(row.modelData.color, row.modelData.auto)
                                        implicitWidth: Theme.controlHeight
                                        implicitHeight: Math.round(Theme.controlHeight * 0.75)
                                        onClicked: popup.cleared(row.modelData.key)
                                    }
                                }
                            }
                        }
                    }
                }
            }

            SectionFrame {
                Layout.alignment: Qt.AlignTop

                Label {
                    text: popup.current ? qsTr("Colour for %1 %2")
                                            .arg(popup.current.side === "pos" ? qsTr("Pos") : qsTr("Neg"))
                                            .arg(popup.current.direction.toUpperCase())
                                        : ""
                    color: Theme.textCaption
                    font.pixelSize: Theme.captionFontSize
                }

                GridLayout {
                    columns: 2
                    rowSpacing: Theme.spaceXs
                    columnSpacing: Theme.spaceXs

                    Repeater {
                        model: Theme.curvePalette

                        ColorSwatch {
                            id: chip

                            required property string modelData

                            color: modelData
                            selected: popup.current !== null
                                      && Qt.colorEqual(popup.current.color, modelData)

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: if (popup.current)
                                               popup.picked(popup.current.key, chip.modelData)
                            }
                        }
                    }
                }
            }
        }

        ActionButton {
            Layout.fillWidth: true
            visible: popup.curves.length > 0
            text: qsTr("Reset All Colours")
            onClicked: popup.resetRequested()
        }
    }
}
