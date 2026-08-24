import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FisheyeCaliJojo

Frame {
    id: control

    default property alias sectionContent: column.data

    leftPadding: Theme.fieldPadding
    rightPadding: Theme.fieldPadding
    topPadding: Theme.fieldPadding
    bottomPadding: Theme.labelSpacing
    spacing: Theme.rowSpacing

    contentItem: ColumnLayout {
        id: column
        spacing: control.spacing
    }

    background: Rectangle {
        color: "transparent"
        border.color: Theme.panelBorder
        radius: Theme.radius
    }
}
