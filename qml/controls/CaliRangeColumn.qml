pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FisheyeCaliJojo

ColumnLayout {
    id: col

    required property int rangeIndex
    required property var values

    property var syncToken

    property int headerHeight: Math.round(Theme.controlHeight * 0.8)
    property int cellHeight:   Math.round(Theme.controlHeight * 0.8)
    property int cellSpacing:  Theme.spaceXs

    signal enableToggled(bool enabled)
    signal edited(string field, string value)
    signal graphRequested()

    spacing: cellSpacing

    component InputCell: ValueField {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.preferredHeight: col.cellHeight

        editable: true
        horizontalAlignment: TextInput.AlignHCenter
        syncToken: col.syncToken
        validator: DecimalValidator {
            bottom: 0
            decimals: 3
            notation: DoubleValidator.StandardNotation
        }
    }

    component ComputedCell: Rectangle {
        property alias text: cellText.text

        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.preferredHeight: col.cellHeight

        color: Theme.fieldDisabledBackground
        radius: Theme.radius

        Label {
            id: cellText

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

    Item {
        Layout.fillWidth: true
        Layout.preferredHeight: col.headerHeight

        GhostButton {
            anchors.centerIn: parent
            text: col.values.label
            onClicked: col.graphRequested()

            ToolTip.visible: hovered
            ToolTip.delay: Theme.animSlow
            ToolTip.text: qsTr("Plot ZFL against IH for this range only.")
        }
    }

    Item {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.preferredHeight: col.cellHeight

        CheckSquare {
            anchors.centerIn: parent
            checked: col.values.enabled
            onToggled: (checked) => col.enableToggled(checked)
        }
    }

    InputCell {
        value: col.values.ihMin
        onEdited: (value) => col.edited("ihMin", value)
    }
    InputCell {
        value: col.values.ihMax
        onEdited: (value) => col.edited("ihMax", value)
    }
    InputCell {
        value: col.values.distMin
        onEdited: (value) => col.edited("distMin", value)
    }
    InputCell {
        value: col.values.distMax
        onEdited: (value) => col.edited("distMax", value)
    }

    ComputedCell { text: col.values.aggregation }

    InputCell {
        value: col.values.pctToPupil
        onEdited: (value) => col.edited("pctToPupil", value)
    }

    ComputedCell { text: col.values.samplingNumber }
    ComputedCell { text: col.values.alphaMin }
    ComputedCell { text: col.values.alphaMax }
}
