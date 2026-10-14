/* SPDX-License-Identifier: GPL-2.0-or-later */
import QtQuick

// Spread as a screen reader finds it: a list naming every entry in the row,
// its focus on the entry Spread has chosen. Nothing here is drawn; Kadunce
// paints Spread itself.
Item {
    id: root

    // Set by SpreadReader.
    property string label: ""
    property var names: []
    property var places: []
    property int current: -1

    Accessible.role: Accessible.List
    Accessible.name: label

    function focusCurrent() {
        const entry = entries.itemAt(current);
        if (entry) entry.forceActiveFocus(Qt.OtherFocusReason);
    }

    onCurrentChanged: focusCurrent()

    Repeater {
        id: entries
        model: root.names.length
        onItemAdded: Qt.callLater(root.focusCurrent)

        delegate: Item {
            required property int index
            width: root.width
            height: root.height
            Accessible.role: Accessible.ListItem
            Accessible.name: root.names[index] || ""
            Accessible.description: root.places[index] || ""
            Accessible.focusable: true
            Accessible.selected: index === root.current
        }
    }
}
