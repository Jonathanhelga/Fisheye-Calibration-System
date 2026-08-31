import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FisheyeCaliJojo

// A small coloured circle, with an optional label, reporting service's status.
RowLayout {
    id: control

    property int status: ProbeStatus.Unknown
    property bool showLabel: false
    property alias color: dot.color

    property string okText:       qsTr("Connected")
    property string failedText:   qsTr("No connection found")
    property string partialText:  qsTr("Partially connected")
    property string checkingText: qsTr("Checking...")
    property string unknownText:  qsTr("Not checked yet")

    readonly property string text:
          status === ProbeStatus.Ok       ? control.okText
        : status === ProbeStatus.Failed   ? control.failedText
        : status === ProbeStatus.Partial  ? control.partialText
        : status === ProbeStatus.Checking ? control.checkingText
                                          : control.unknownText

    spacing: Theme.spaceXs

    Rectangle {
        id: dot

        Layout.alignment: Qt.AlignVCenter
        implicitWidth: Math.round(Theme.unit * 0.5)
        implicitHeight: implicitWidth
        radius: width / 2
        antialiasing: true

        color: control.status === ProbeStatus.Ok      ? Theme.statusOk
             : control.status === ProbeStatus.Failed  ? Theme.statusFailed
             : control.status === ProbeStatus.Partial ? Theme.statusPartial
                                                      : Theme.statusUnknown

        SequentialAnimation on opacity {
            running: control.status === ProbeStatus.Checking
            loops: Animation.Infinite

            NumberAnimation { to: 0.3; duration: Theme.animSlow; easing.type: Easing.InOutQuad }
            NumberAnimation { to: 1.0; duration: Theme.animSlow; easing.type: Easing.InOutQuad }

            onRunningChanged: if (!running) dot.opacity = 1
        }
    }

    Label {
        Layout.fillWidth: true
        Layout.alignment: Qt.AlignVCenter
        visible: control.showLabel && control.text.length > 0
        text: control.text
        color: Theme.textCaption
        font.pixelSize: Theme.captionFontSize
        elide: Text.ElideRight
    }
}
