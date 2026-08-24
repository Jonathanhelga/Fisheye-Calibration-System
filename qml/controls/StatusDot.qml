import QtQuick
import FisheyeCaliJojo

// A small coloured circle reporting service's status.
Rectangle {
    id: dot

    property int status: ServerProbe.Unknown

    implicitWidth: Math.round(Theme.unit * 0.5)
    implicitHeight: implicitWidth
    radius: width / 2
    antialiasing: true

    color: status === ServerProbe.Ok      ? Theme.statusOk
         : status === ServerProbe.Failed  ? Theme.statusFailed
         : status === ServerProbe.Partial ? Theme.statusPartial
                                          : Theme.statusUnknown

    SequentialAnimation on opacity {
        running: dot.status === ServerProbe.Checking
        loops: Animation.Infinite

        NumberAnimation { to: 0.3; duration: Theme.animSlow; easing.type: Easing.InOutQuad }
        NumberAnimation { to: 1.0; duration: Theme.animSlow; easing.type: Easing.InOutQuad }

        onRunningChanged: if (!running) dot.opacity = 1
    }
}
