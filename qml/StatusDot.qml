import QtQuick
import FisheyeCaliJojo

// A small coloured circle reporting service's status.
Rectangle {
    id: dot

    property int status: ServerProbe.Unknown

    implicitWidth: 8
    implicitHeight: 8
    width: implicitWidth
    height: implicitHeight
    radius: width / 2
    antialiasing: true

    color: status === ServerProbe.Ok      ? "#3aa76d"
         : status === ServerProbe.Failed  ? "#c0483c"
         : status === ServerProbe.Partial ? "#e0a02a"
                                          : "#c6ced5"

    SequentialAnimation on opacity {
        running: dot.status === ServerProbe.Checking
        loops: Animation.Infinite

        NumberAnimation { to: 0.3; duration: 450; easing.type: Easing.InOutQuad }
        NumberAnimation { to: 1.0; duration: 450; easing.type: Easing.InOutQuad }

        onRunningChanged: if (!running) dot.opacity = 1
    }
}
