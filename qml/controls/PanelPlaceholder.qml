import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FisheyeCaliJojo

Rectangle {
    id: panel

    property alias title: heading.text
    default property alias content: contentColumn.data

    implicitWidth:  Math.max(Theme.unit * 16, body.implicitWidth + 2 * Theme.panelMargin)
    implicitHeight: Math.max(Theme.unit * 8, body.implicitHeight + 2 * Theme.panelMargin)

    color: Theme.panelBackground
    border.color: Theme.panelBorder
    radius: Theme.radius

    ColumnLayout {
        id: body
        anchors.fill: parent
        anchors.margins: Theme.panelMargin
        spacing: Theme.rowSpacing

        Label {
            id: heading
            Layout.fillWidth: true
            font.bold: true
            font.pixelSize: Theme.fontTitle
            color: Theme.accent
            elide: Text.ElideRight
        }

        Item {
            id: contentArea

            Layout.fillWidth:  true
            Layout.fillHeight: true
            Layout.preferredWidth:  contentColumn.implicitWidth
            Layout.preferredHeight: contentColumn.implicitHeight

            ColumnLayout {
                id: contentColumn
                width: contentArea.width
                anchors.verticalCenter: contentArea.verticalCenter
                spacing: Theme.rowSpacing
            }
        }
    }
}
