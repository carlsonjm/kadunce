/* SPDX-License-Identifier: GPL-2.0-or-later */
import QtQuick

// What a screen reader says about a surface Kadunce paints. Screen readers
// speak a switcher as the name of its focused filler, so the line is held by
// two fillers that take the focus in turn: each new line is a new focus, and
// is spoken at once. Nothing here is drawn.
Item {
    id: root

    // The filler the next line goes to.
    property bool onOne: true

    function say(line) {
        const next = onOne ? one : two;
        onOne = !onOne;
        next.line = line;
        next.forceActiveFocus(Qt.OtherFocusReason);
    }

    function reset() {
        onOne = true;
    }

    Item {
        id: one
        property string line: ""
        anchors.fill: parent
        Accessible.role: Accessible.Client
        Accessible.name: line
        Accessible.focusable: true
    }

    Item {
        id: two
        property string line: ""
        anchors.fill: parent
        Accessible.role: Accessible.Client
        Accessible.name: line
        Accessible.focusable: true
    }
}
