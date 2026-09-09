pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FisheyeCaliJojo

RowLayout {
    id: row

    required property int rangeIndex
    required property var values

    property var syncToken

    property int enableWidth:   Math.round(Theme.charUnit * 4)
    property int rangeWidth:    Math.round(Theme.charUnit * 7)
    property int ihWidth:       Math.round(Theme.charUnit * 7)
    property int totalWidth:    Math.round(Theme.charUnit * 7)
    property int alphaWidth:    Math.round(Theme.charUnit * 7)
    property int distanceWidth: Math.round(Theme.charUnit * 8)
    property int aggrWidth:     Math.round(Theme.charUnit * 9)
    property int dividerWidth:  1
    property int columnSpacing: Theme.spaceXs

    readonly property bool globalRange: rangeIndex === 0
    readonly property int cellHeight: Math.round(Theme.controlHeight * 0.72)

    signal enableToggled(bool enabled)
    signal edited(string field, string value)
    signal graphRequested()

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
        Layout.preferredWidth: row.enableWidth
        Layout.preferredHeight: row.cellHeight

        CheckBox {
            anchors.centerIn: parent
            checked: row.values.enabled
            onToggled: row.enableToggled(checked)
        }
    }

    Item {
        Layout.preferredWidth: row.rangeWidth
        Layout.preferredHeight: row.cellHeight

        GhostButton {
            anchors.centerIn: parent
            text: row.values.label
            onClicked: row.graphRequested()

            ToolTip.visible: hovered
            ToolTip.delay: Theme.animSlow
            ToolTip.text: qsTr("Plot ZFL against IH for this range only.")
        }
    }

    ValueField {
        Layout.preferredWidth: row.ihWidth
        Layout.preferredHeight: row.cellHeight
        editable: !row.globalRange
        validator: DecimalValidator { bottom: 0; top: 100; decimals: 3; notation: DoubleValidator.StandardNotation }
        value: row.values.ihMin
        syncToken: row.syncToken
        onEdited: (value) => row.edited("ihMin", value)
    }

    ValueField {
        Layout.preferredWidth: row.ihWidth
        Layout.preferredHeight: row.cellHeight
        editable: !row.globalRange
        validator: DecimalValidator { bottom: 0; top: 100; decimals: 3; notation: DoubleValidator.StandardNotation }
        value: row.values.ihMax
        syncToken: row.syncToken
        onEdited: (value) => row.edited("ihMax", value)
    }

    ColumnDivider {}

    ComputedCell { Layout.preferredWidth: row.totalWidth; text: row.values.total }
    ComputedCell { Layout.preferredWidth: row.alphaWidth; text: row.values.alphaMin }
    ComputedCell { Layout.preferredWidth: row.alphaWidth; text: row.values.alphaMax }

    ColumnDivider {}

    ComputedCell { Layout.preferredWidth: row.distanceWidth; text: row.values.distance }
    ComputedCell { Layout.preferredWidth: row.aggrWidth;     text: row.values.aggregation }
}
