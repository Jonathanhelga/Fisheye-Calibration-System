import QtQuick
import FisheyeCaliJojo

QtObject {
    id: root

    property bool enabled: false
    property string fingerprint: ""

    signal triggered()

    readonly property Timer timer: Timer {
        interval: Theme.autoUpdateDelay
        onTriggered: root.triggered()
    }

    onFingerprintChanged: if (root.enabled) root.timer.restart()
    onEnabledChanged: root.enabled ? root.triggered() : root.timer.stop()
}
