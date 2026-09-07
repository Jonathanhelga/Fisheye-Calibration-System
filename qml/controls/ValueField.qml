import QtQuick
import FisheyeCaliJojo

Rectangle {
    id: field

    property var value: ""
    property var syncToken
    property alias validator: input.validator
    property alias horizontalAlignment: input.horizontalAlignment
    property bool editable: false

    signal edited(string value)
    signal moveFocusRequested(int delta)

    function takeFocus() {
        input.forceActiveFocus()
        input.selectAll()
    }

    function displayText() {
        if (value === undefined || value === null)
            return ""
        if (typeof value === "number" && !isFinite(value))
            return ""
        return String(value)
    }

    function forceSync() {
        const shown = field.displayText()
        if (input.text !== shown)
            input.text = shown
    }

    function sync() {
        if (input.activeFocus)
            return
        field.forceSync()
    }

    onValueChanged: sync()
    onSyncTokenChanged: forceSync()
    Component.onCompleted: forceSync()

    implicitWidth: Theme.fieldMinWidth
    implicitHeight: Theme.controlHeight

    color: field.enabled ? Theme.fieldBackground : Theme.fieldDisabledBackground
    border.color: input.activeFocus ? Theme.accent : Theme.panelBorder
    radius: Theme.radius

    Behavior on color {
        ColorAnimation { duration: Theme.animFast }
    }

    TextInput {
        id: input

        anchors.fill: parent
        anchors.leftMargin: Theme.fieldPadding
        anchors.rightMargin: Theme.fieldPadding
        clip: true

        horizontalAlignment: TextInput.AlignLeft
        verticalAlignment: TextInput.AlignVCenter

        color: field.enabled ? Theme.textPrimary : Theme.textDisabled
        font.bold: true

        readOnly: !field.editable || !field.enabled
        activeFocusOnPress: field.editable
        selectByMouse: field.editable

        Keys.onReturnPressed: (event) => { event.accepted = true; field.moveFocusRequested(1) }
        Keys.onEnterPressed:  (event) => { event.accepted = true; field.moveFocusRequested(1) }
        Keys.onDownPressed:   (event) => { event.accepted = true; field.moveFocusRequested(1) }
        Keys.onUpPressed:     (event) => { event.accepted = true; field.moveFocusRequested(-1) }

        onTextEdited: if (acceptableInput) field.edited(text)
        onActiveFocusChanged: {
            if (!activeFocus) {
                field.forceSync()
                cursorPosition = 0
            }
        }
    }
}
