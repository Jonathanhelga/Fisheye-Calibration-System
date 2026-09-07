import QtQuick
import FisheyeCaliJojo

// Debounced "the thing changed, render it again".
//
// A change is one keystroke: typing "120" into a radius field is three changes,
// and rendering a 1920x1920 pattern on the rig for each of them would queue three
// renders to throw two away. The timer restarts on every change, so the render
// happens once the operator stops typing.
//
// `fingerprint` is any string that differs when the subject differs -- see the
// pattern panels, where it is the spec JSON plus a layer revision counter.
// Enabling fires immediately rather than waiting for the next unrelated edit, so
// the switch has a visible effect.
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
