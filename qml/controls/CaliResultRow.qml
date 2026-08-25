pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FisheyeCaliJojo

RowLayout {
    id: row

    required property int layerIndex
    required property var values

    property var directions: []
    property var syncToken
    property bool sideStart: false

    property int layerWidth:   Math.round(Theme.charUnit * 5)
    property int pctWidth:     Math.round(Theme.charUnit * 6)
    property int ictWidth:     Math.round(Theme.charUnit * 6)
    property int coreWidth:    Math.round(Theme.charUnit * 8)
    property int alphaWidth:   Math.round(Theme.charUnit * 6)
    property int zflWidth:     Math.round(Theme.charUnit * 7)
    property int avgWidth:     Math.round(Theme.charUnit * 8)
    property int dividerWidth: 1
    property int columnSpacing: Theme.spaceXs

    readonly property int cellHeight: Math.round(Theme.controlHeight * 0.72)

    signal pctEdited(string value)
    signal ictEdited(int direction, string value)
    signal sideStartPicked()

    spacing: columnSpacing

    component ComputedCell: Label {
        Layout.preferredHeight: row.cellHeight
        Layout.alignment: Qt.AlignVCenter
        color: Theme.textPrimary
        font.pixelSize: Theme.captionFontSize
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    component ColumnDivider: Rectangle {
        Layout.preferredWidth: row.dividerWidth
        Layout.preferredHeight: row.cellHeight
        Layout.alignment: Qt.AlignVCenter
        color: Theme.panelBorder
    }

    Item {
        Layout.preferredWidth: row.layerWidth
        Layout.preferredHeight: row.cellHeight

        Label {
            anchors.fill: parent
            text: row.sideStart ? qsTr("▸ %1").arg(row.layerIndex) : String(row.layerIndex)
            color: row.sideStart ? Theme.accent : Theme.textCaption
            font.pixelSize: Theme.captionFontSize
            font.bold: row.sideStart
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: row.sideStartPicked()

            ToolTip.visible: containsMouse
            ToolTip.delay: Theme.animSlow
            ToolTip.text: qsTr("Mark this layer as the first one shown on the side monitors.")
            hoverEnabled: true
        }
    }

    ValueField {
        Layout.preferredWidth: row.pctWidth
        Layout.preferredHeight: row.cellHeight
        editable: true
        validator: DecimalValidator { bottom: 0; decimals: 3; notation: DoubleValidator.StandardNotation }
        value: row.values.pct
        syncToken: row.syncToken
        onEdited: (value) => row.pctEdited(value)
    }

    Repeater {
        model: row.directions

        ValueField {
            id: ictCell

            required property int index

            Layout.preferredWidth: row.ictWidth
            Layout.preferredHeight: row.cellHeight
            editable: true
            validator: DecimalValidator { bottom: 0; decimals: 3; notation: DoubleValidator.StandardNotation }
            value: row.values.ict[ictCell.index]
            syncToken: row.syncToken
            onEdited: (value) => row.ictEdited(ictCell.index, value)
        }
    }

    ColumnDivider {}

    ComputedCell { Layout.preferredWidth: row.coreWidth; text: row.values.ictAvg }
    ComputedCell { Layout.preferredWidth: row.coreWidth; text: row.values.pctCal }
    ComputedCell { Layout.preferredWidth: row.coreWidth; text: row.values.distance }

    ColumnDivider {}

    Repeater {
        model: row.directions

        RowLayout {
            id: pair

            required property int index

            Layout.fillWidth: false
            spacing: row.columnSpacing

            ComputedCell {
                Layout.preferredWidth: row.alphaWidth
                text: row.values.alpha[pair.index]
            }
            ComputedCell {
                Layout.preferredWidth: row.zflWidth
                text: row.values.zfl[pair.index]
            }
        }
    }

    ColumnDivider {}

    ComputedCell { Layout.preferredWidth: row.avgWidth; text: row.values.alphaAvg }
    ComputedCell { Layout.preferredWidth: row.avgWidth; text: row.values.zflAvg }
}
