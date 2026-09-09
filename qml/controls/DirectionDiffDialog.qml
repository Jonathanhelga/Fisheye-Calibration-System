pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FisheyeCaliJojo

// ICT Direction Difference: opposite directions, compared node by node.
//
// This is the Widgets client's btn_direction_diff, which is a "?" beside the shot
// buttons -- an INFO POPUP, not a measurement control. Restored 2026-09-09, when
// it turned out this app had put the label on something else entirely: pressing
// Direction Diff here re-ran the curves, which is what a Pos/Neg shot does by
// itself in the old client.
//
// What it is for: a well-aimed capture is symmetric about the centre, so the
// crossing at ring k going north should sit at the same radius as the one going
// south. It does not, quite, and the size of that disagreement is the most direct
// read on whether the rig is aimed properly -- far more direct than looking at
// the curves. N-S large means the centre is off vertically; W-E large means
// horizontally; the diagonals catch a tilt the straights miss.
//
// Red at |difference| >= 5 px, which is the old client's threshold.
//
// The nodes are the SAME ones Update Table writes -- same op, same slots, same
// parameters. That is the whole value of it: it shows what Update Table is about
// to record. A second way of getting these numbers would make it show something
// else, and the difference would look like a measurement.
Dialog {
    id: dialog

    property var nodes: ({})

    // find-style access, never nodes[d] with a default. A direction the server
    // did not return is ABSENT, and inventing an empty list for it turns "this
    // direction was not detected" into "this direction has no nodes" -- which
    // reads as a measurement rather than as missing data. The header row prints
    // both counts so the two stay distinguishable.
    function listFor(direction) {
        const v = dialog.nodes[direction]
        return (v !== undefined && v !== null) ? v : []
    }

    readonly property var pairs: [
        { label: "N - S",   a: "n",  b: "s"  },
        { label: "W - E",   a: "w",  b: "e"  },
        { label: "NW - SE", a: "nw", b: "se" },
        { label: "SW - NE", a: "sw", b: "ne" }
    ]

    readonly property var built: {
        const rows = []
        const lines = []
        for (let p = 0; p < dialog.pairs.length; ++p) {
            const pair = dialog.pairs[p]
            const A = dialog.listFor(pair.a)
            const B = dialog.listFor(pair.b)
            const m = Math.min(A.length, B.length)

            rows.push({ header: true,
                        text: qsTr("%1   (%2 nodes / %3 nodes)")
                                  .arg(pair.label).arg(A.length).arg(B.length) })

            let sum = 0
            let worst = 0
            for (let i = 0; i < m; ++i) {
                const d = Math.abs(A[i] - B[i])
                sum += d
                worst = Math.max(worst, d)
                rows.push({ header: false, n: i + 1, a: A[i], b: B[i], d: d })
            }
            lines.push(qsTr("%1: mean |Δ| = %2, max |Δ| = %3")
                           .arg(pair.label)
                           .arg(m > 0 ? (sum / m).toFixed(2) : "0.00")
                           .arg(worst.toFixed(2)))
        }
        return { rows: rows, summary: lines.join("\n") }
    }

    title: qsTr("ICT Direction Difference (N-S, W-E, NW-SE, SW-NE)")
    modal: true
    standardButtons: Dialog.Close

    implicitWidth: Math.round(Theme.charUnit * 62)
    implicitHeight: Math.round(Theme.unit * 34)

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.rowSpacing

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.textCaption
            font.pixelSize: Theme.captionFontSize
            text: qsTr("Difference between opposite directions of the latest Pos / Neg shot.\n"
                     + "Near 0 = well-centred and symmetric; red = |difference| >= 5 px.")
        }

        ListView {
            id: table

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: dialog.built.rows
            boundsBehavior: Flickable.StopAtBounds

            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            header: Rectangle {
                width: ListView.view.width
                height: heads.implicitHeight + Theme.spaceXs
                color: Theme.panelBackground
                z: 2

                RowLayout {
                    id: heads
                    anchors.fill: parent
                    anchors.leftMargin: Theme.spaceXs
                    anchors.rightMargin: Theme.spaceXs
                    spacing: Theme.spaceSm

                    Label {
                        Layout.preferredWidth: Math.round(Theme.charUnit * 8)
                        text: qsTr("Node #")
                        font.bold: true
                        font.pixelSize: Theme.captionFontSize
                        color: Theme.textCaption
                    }
                    Label {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignRight
                        text: qsTr("Dir A")
                        font.bold: true
                        font.pixelSize: Theme.captionFontSize
                        color: Theme.textCaption
                    }
                    Label {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignRight
                        text: qsTr("Dir B")
                        font.bold: true
                        font.pixelSize: Theme.captionFontSize
                        color: Theme.textCaption
                    }
                    Label {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignRight
                        text: qsTr("|A - B|")
                        font.bold: true
                        font.pixelSize: Theme.captionFontSize
                        color: Theme.textCaption
                    }
                }
            }

            delegate: Item {
                id: row

                required property var modelData

                width: ListView.view.width
                height: line.implicitHeight + Theme.spaceXs

                Rectangle {
                    anchors.fill: parent
                    color: row.modelData.header ? Theme.fieldDisabledBackground : "transparent"
                }

                RowLayout {
                    id: line
                    anchors.fill: parent
                    anchors.leftMargin: Theme.spaceXs
                    anchors.rightMargin: Theme.spaceXs
                    spacing: Theme.spaceSm

                    Label {
                        visible: row.modelData.header
                        Layout.fillWidth: true
                        text: row.modelData.header ? row.modelData.text : ""
                        font.bold: true
                        font.pixelSize: Theme.captionFontSize
                        color: Theme.accent
                        elide: Text.ElideRight
                    }

                    Label {
                        visible: !row.modelData.header
                        Layout.preferredWidth: Math.round(Theme.charUnit * 8)
                        text: row.modelData.header ? "" : String(row.modelData.n)
                        font.pixelSize: Theme.captionFontSize
                        color: Theme.textCaption
                    }
                    Label {
                        visible: !row.modelData.header
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignRight
                        text: row.modelData.header ? "" : row.modelData.a.toFixed(1)
                        font.pixelSize: Theme.captionFontSize
                        color: Theme.textPrimary
                    }
                    Label {
                        visible: !row.modelData.header
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignRight
                        text: row.modelData.header ? "" : row.modelData.b.toFixed(1)
                        font.pixelSize: Theme.captionFontSize
                        color: Theme.textPrimary
                    }
                    Label {
                        visible: !row.modelData.header
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignRight
                        text: row.modelData.header ? "" : row.modelData.d.toFixed(1)
                        font.pixelSize: Theme.captionFontSize
                        // The old client's threshold, kept exactly: 5 px is where
                        // an aiming error stops being noise.
                        color: (!row.modelData.header && row.modelData.d >= 5.0)
                                   ? Theme.statusFailed : Theme.textPrimary
                        font.bold: !row.modelData.header && row.modelData.d >= 5.0
                    }
                }
            }
        }

        Label {
            Layout.fillWidth: true
            text: dialog.built.summary
            font.bold: true
            font.pixelSize: Theme.captionFontSize
            color: Theme.textPrimary
        }
    }
}
