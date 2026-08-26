import QtQuick
import FisheyeCaliJojo

Item {
    id: root

    property real contentWidth:  Theme.designWidth
    property real contentHeight: Theme.designHeight

    readonly property real factor: Math.max(0.01,
                                            Math.min(1,
                                                     root.width  / Math.max(1, root.contentWidth),
                                                     root.height / Math.max(1, root.contentHeight)))

    readonly property alias canvasWidth:  canvas.width
    readonly property alias canvasHeight: canvas.height

    default property alias content: canvas.data

    Item {
        id: canvas

        width:  root.width  / root.factor
        height: root.height / root.factor
        transformOrigin: Item.TopLeft
        scale: root.factor
    }
}
