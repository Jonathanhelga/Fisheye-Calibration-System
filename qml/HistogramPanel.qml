import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Replaces the two hand-copied "Histogram1"/"Histogram2" QGroupBox blocks
// in the old mainwindow_main.ui with one component parameterized by
// channel. Compass checkboxes and curve wiring still need to be ported;
// this is the shell.
Rectangle {
    id: root
    property int channel: 1

    color: "#f4f6f8"
    border.color: "#c7cfd6"
    radius: 4

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8

        Label {
            text: "Histogram" + root.channel
            font.bold: true
            color: "#2f6fbf"
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 8

            // A nested layout defaults to Layout.fillWidth: true, so this
            // button column would swell past its 140 and eat the plot's
            // space. It is a fixed-width sidebar, so opt out of growing.
            ColumnLayout {
                Layout.preferredWidth: 140
                Layout.fillWidth: false
                Layout.alignment: Qt.AlignTop
                Button { text: "Show Curve"; Layout.fillWidth: true }
                Button { text: "Curve Color"; Layout.fillWidth: true }
            }

            Rectangle {
                // A bare Rectangle is implicitly 0x0, and fillWidth splits
                // leftover space in proportion to preferred width, so
                // without this the plot collapses to nothing.
                Layout.preferredWidth: 400
                Layout.preferredHeight: 160
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "white"
                border.color: "#c7cfd6"

                Label {
                    anchors.centerIn: parent
                    text: "plot goes here"
                    color: "#8a939c"
                }
            }
        }
    }
}
