pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FisheyeCaliJojo

Rectangle {
    id: root

    property url source
    property bool pickEnabled: false
    property bool showGrid: false
    property string emptyText: qsTr("No image")
    property string hint
    property string badgeText
    property bool badgeActive: false

    readonly property int sourceWidth: image.sourceSize.width
    readonly property int sourceHeight: image.sourceSize.height
    readonly property bool loaded: image.status === Image.Ready && sourceWidth > 0
    readonly property real zoom: loaded ? image.paintedWidth / sourceWidth : 0

    readonly property real padLeft: (width - image.paintedWidth) / 2
    readonly property real padTop: (height - image.paintedHeight) / 2

    readonly property real loupeMagnification: 18

    property int hoverX: -1
    property int hoverY: -1

    property int centerX: -1
    property int centerY: -1
    property int roiRadius: 0

    readonly property bool hasCenter: loaded && centerX >= 0 && centerY >= 0 && roiRadius > 0
    readonly property real markerThickness: Math.max(1, Math.round(Theme.unit / 8))
    readonly property real hairline: 1

    signal picked(int x, int y)

    implicitWidth: Theme.unit * 16
    implicitHeight: Theme.unit * 12

    color: Theme.previewBackground
    border.color: Theme.panelBorder
    radius: Theme.radius
    clip: true

    Image {
        id: image
        anchors.fill: parent
        anchors.margins: root.border.width + root.radius * (1 - 1 / Math.SQRT2)
        source: root.source
        fillMode: Image.PreserveAspectFit
        asynchronous: true
        smooth: true
        mipmap: true
    }

    Label {
        anchors.centerIn: parent
        visible: !root.loaded
        text: image.status === Image.Loading ? qsTr("Loading...")
            : image.status === Image.Error ? qsTr("Image could not be loaded")
                                           : root.emptyText
        color: Theme.textOnPreview
        font.pixelSize: Theme.fontTitle
    }

    MouseArea {
        id: picker

        x: root.padLeft
        y: root.padTop
        width: image.paintedWidth
        height: image.paintedHeight

        enabled: root.loaded
        hoverEnabled: true
        cursorShape: root.pickEnabled ? Qt.CrossCursor : Qt.ArrowCursor

        function toSourceX(px) { return Math.floor(px / image.paintedWidth * root.sourceWidth) }
        function toSourceY(py) { return Math.floor(py / image.paintedHeight * root.sourceHeight) }

        onPositionChanged: (mouse) => {
            root.hoverX = toSourceX(mouse.x)
            root.hoverY = toSourceY(mouse.y)
        }

        onExited: {
            root.hoverX = -1
            root.hoverY = -1
        }

        onClicked: (mouse) => {
            if (root.pickEnabled)
                root.picked(toSourceX(mouse.x), toSourceY(mouse.y))
        }
    }

    Item {
        id: guides

        visible: root.showGrid && root.loaded

        x: root.padLeft
        y: root.padTop
        width: image.paintedWidth
        height: image.paintedHeight

        Repeater {
            model: [1 / 3, 2 / 3]

            Rectangle {
                required property real modelData

                x: Math.round(guides.width * modelData)
                width: root.hairline
                height: guides.height
                color: Theme.previewGrid
            }
        }

        Repeater {
            model: [1 / 3, 2 / 3]

            Rectangle {
                required property real modelData

                y: Math.round(guides.height * modelData)
                width: guides.width
                height: root.hairline
                color: Theme.previewGrid
            }
        }

        Rectangle {
            x: Math.round(guides.width / 2)
            width: root.hairline
            height: guides.height
            color: Theme.previewGuide
        }

        Rectangle {
            y: Math.round(guides.height / 2)
            width: guides.width
            height: root.hairline
            color: Theme.previewGuide
        }
    }

    Item {
        id: roi

        readonly property real half: root.roiRadius * root.zoom

        visible: root.hasCenter

        x: root.padLeft + (root.centerX + 0.5) * root.zoom - half
        y: root.padTop + (root.centerY + 0.5) * root.zoom - half
        width: 2 * half
        height: 2 * half

        Rectangle {
            anchors.fill: parent
            color: "transparent"
            border.color: Theme.previewMarker
            border.width: root.markerThickness
        }

        Rectangle {
            anchors.fill: parent
            radius: width / 2
            color: "transparent"
            border.color: Theme.previewMarker
            border.width: root.markerThickness
            antialiasing: true
        }

        Rectangle {
            anchors.centerIn: parent
            width: roi.width * Math.SQRT2
            height: root.markerThickness
            color: Theme.previewMarker
            rotation: 45
            antialiasing: true
        }

        Rectangle {
            anchors.centerIn: parent
            width: roi.width * Math.SQRT2
            height: root.markerThickness
            color: Theme.previewMarker
            rotation: -45
            antialiasing: true
        }
    }

    Rectangle {
        id: loupe

        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: Theme.spaceXs

        width: Theme.unit * 10
        height: width
        radius: Theme.radius
        color: Theme.previewBackground
        border.color: Theme.panelBorder
        clip: true

        visible: root.loaded && root.hoverX >= 0

        readonly property real pixelScale: root.zoom * root.loupeMagnification

        Image {
            source: root.source
            width: root.sourceWidth * loupe.pixelScale
            height: root.sourceHeight * loupe.pixelScale
            x: loupe.width / 2 - root.hoverX * loupe.pixelScale
            y: loupe.height / 2 - root.hoverY * loupe.pixelScale
            smooth: false
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: Theme.spaceXs

        visible: root.badgeText !== ""
        width: badge.implicitWidth + 2 * Theme.fieldPadding
        height: badge.implicitHeight + 2 * Theme.spaceXs
        radius: Theme.radius
        color: Theme.previewOverlay

        RowLayout {
            id: badge

            anchors.centerIn: parent
            spacing: Theme.spaceXs

            Rectangle {
                visible: root.badgeActive
                Layout.alignment: Qt.AlignVCenter

                implicitWidth: Math.round(Theme.unit * 0.4)
                implicitHeight: implicitWidth
                radius: width / 2
                antialiasing: true
                color: Theme.previewMarker
            }

            Label {
                text: root.badgeText
                color: Theme.textOnPreview
                font.pixelSize: Theme.captionFontSize
                font.bold: true
            }
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.margins: Theme.spaceXs

        visible: overlay.text !== ""
        width: overlay.implicitWidth + 2 * Theme.fieldPadding
        height: overlay.implicitHeight + 2 * Theme.spaceXs
        radius: Theme.radius
        color: Theme.previewOverlay

        Label {
            id: overlay
            anchors.centerIn: parent
            text: root.hoverX >= 0 ? qsTr("x %1  y %2").arg(root.hoverX).arg(root.hoverY)
                                   : root.hint
            color: Theme.textOnPreview
            font.pixelSize: Theme.captionFontSize
        }
    }
}
