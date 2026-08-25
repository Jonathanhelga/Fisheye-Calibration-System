import QtQuick
import FisheyeCaliJojo

Rectangle {
    id: field

    property var value: ""
    property alias validator: input.validator
    property alias horizontalAlignment: input.horizontalAlignment
    property bool editable: false

    signal edited(string value)

    function sync() {
        const shown = (value === undefined || value === null) ? "" : String(value)
        if (input.text !== shown)
            input.text = shown
    }

    onValueChanged: sync()
    Component.onCompleted: sync()

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

        onTextEdited: if (acceptableInput) field.edited(text)
        onActiveFocusChanged: if (!activeFocus) cursorPosition = 0
    }
}
