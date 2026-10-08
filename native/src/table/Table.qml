// SPDX-License-Identifier: GPL-2.0-or-later
// Table's rows, drawn over the display that holds them. Kadunce lays every
// piece out and says what state it is in; this only draws it and eases each
// change. The rows are Float: no stem and no drawn line, the cards answering
// the finger instead, and the card in hand is the card itself. The shade is
// the dock's band turned to hang from the top edge. What the finger is on
// lifts rather than lights up, and the workspace the person is in is the
// brightest.
import QtQuick
import QtQuick.Shapes
import org.kde.kirigami as Kirigami

Item {
    id: root
    // A screen reader names Table and each piece in it by what it is to you.
    Accessible.role: Accessible.Pane
    Accessible.name: "Table"
    anchors.fill: parent

    property var model: ({})
    // The name being typed into the tab Kadunce marks as editing, which
    // Kadunce reads as it changes and keeps when the typing is done.
    property string renameText: ""
    signal editingStarted(string text)
    signal editingEnded()
    function beginEditing(text) {
        renameText = text;
        editingStarted(text);
    }
    function endEditing() {
        editingEnded();
    }
    // Kadunce sets this while Table is open. The rows fade down into place
    // with it over the dock tide's 260 ms. The band leads them in and follows
    // them out, since a soft shade reads later than solid pills and the dark
    // must never arrive after the tabs.
    property bool shown: false
    // Plasma's animation speed, as KWin's animation time factor: 1 at the
    // default, smaller when faster, 0 for instant. Kadunce sets it as Table
    // opens, and every duration here passes through ms().
    property real motionFactor: 1
    function ms(base) {
        return motionFactor > 0 ? Math.max(1, Math.round(base * motionFactor)) : 1;
    }
    property real reveal: 0
    property real bandReveal: 0
    // Started here, each with its own duration, so neither reads the other
    // state's while the change is still arriving.
    NumberAnimation { id: rowsFade; target: root; property: "reveal"; easing.type: Easing.OutCubic }
    NumberAnimation { id: bandFade; target: root; property: "bandReveal"; easing.type: Easing.OutCubic }
    // How far a pulling finger has come toward the tabs; 1 without one. The
    // band's darkness follows it directly during a stroke and eases when a
    // flick leaves the tabs open.
    readonly property bool scrubbing: model.scrubbing === true
    // The band lightens while a lift would cancel. It eases on its own: an
    // ease on the band's whole opacity restarted on every frame of the fade
    // in and held the band back until it ended (measured 26 September).
    property real cancelShade: model.cancelling ? 0.4 : 1
    Behavior on cancelShade { NumberAnimation { duration: root.ms(200) } }
    property real pull: model.pull ?? 1
    Behavior on pull { enabled: !root.scrubbing; NumberAnimation { duration: root.ms(150); easing.type: Easing.OutCubic } }
    onShownChanged: {
        rowsFade.stop();
        rowsFade.to = shown ? 1 : 0;
        rowsFade.duration = root.ms(shown ? 260 : 200);
        rowsFade.start();
        bandFade.stop();
        bandFade.to = shown ? 1 : 0;
        bandFade.duration = root.ms(shown ? 150 : 320);
        bandFade.start();
    }
    readonly property real drift: -12 * (1 - reveal)

    readonly property var tabs: model.tabs || []
    readonly property var cards: model.cards || []
    readonly property real row: model.row || 44
    readonly property bool carrying: model.carrying === true
    // How near the finger is to the cards, and how far the chosen card is
    // pulled toward the lift line: 0 to 1 each.
    readonly property real approach: model.approach ?? 1
    readonly property real lean: model.lean ?? 0

    // Itasca's colour roles (ITASCA-VISUAL-LANGUAGE.md). Tabs are controls,
    // raised; cards are information, on the surface. Hover and selection
    // are steps of white, the accent marks only a drop's destination, and
    // nothing is outlined. They follow the colour scheme, which Kadunce reads
    // and sets here as Table opens (SurfaceTone.h): on a dark ground they are
    // these fixed values, and on a light one each step is the scheme's text
    // laid over its ground.
    property color themeGround: "#141414"
    property color themeText: "#f8f8ff"
    property color themeAccent: "#f8f8ff"
    property color themeAccentText: "#102729"
    readonly property bool dark: 0.299 * themeGround.r + 0.587 * themeGround.g + 0.114 * themeGround.b <= 0.5
    readonly property color ink: dark ? "#ffffff" : themeText
    // The ink at a given strength, see-through.
    function wash(alpha) {
        return Qt.rgba(ink.r, ink.g, ink.b, alpha);
    }
    // The ink at a given strength laid over the ground, then held at an
    // opacity of its own.
    function over(alpha, opacity) {
        const solid = Qt.tint(themeGround, wash(alpha));
        return Qt.rgba(solid.r, solid.g, solid.b, opacity);
    }
    readonly property color text: dark ? "#f8f8ff" : themeText
    readonly property color text2: wash(0.66)
    readonly property color raised: dark ? Qt.rgba(36 / 255, 36 / 255, 36 / 255, 0.94) : over(0.07, 0.94)
    readonly property color hoverFill: dark ? Qt.rgba(63 / 255, 63 / 255, 63 / 255, 0.95) : over(0.12, 0.95)
    readonly property color selectedFill: dark ? Qt.rgba(89 / 255, 89 / 255, 89 / 255, 0.96) : over(0.22, 0.96)
    readonly property color surface: dark ? Qt.rgba(20 / 255, 20 / 255, 20 / 255, 0.92) : over(0, 0.92)
    readonly property color chosenCard: dark ? Qt.rgba(76 / 255, 76 / 255, 76 / 255, 0.97) : over(0.17, 0.97)
    // The edges of a stack's cards behind its face, nearest first.
    readonly property var sliverFills: dark
        ? [Qt.rgba(58 / 255, 58 / 255, 58 / 255, 0.96), Qt.rgba(42 / 255, 42 / 255, 42 / 255, 0.96),
           Qt.rgba(32 / 255, 32 / 255, 32 / 255, 0.96)]
        : [over(0.15, 0.96), over(0.09, 0.96), over(0.05, 0.96)]
    // The band's shade: black on dark, the ground on light.
    readonly property color shade: dark ? "#000000" : themeGround
    // TableSizes::SliverStep: each edge a step left of the one before it.
    readonly property real sliverStep: 7
    readonly property color accent: themeAccent
    readonly property color accentText: themeAccentText
    readonly property int ease: Easing.OutCubic
    // A piece rises under the finger and settles a little faster once it
    // has gone, so paging across hands the lift from one to the next.
    readonly property int rise: ms(180)
    readonly property int settle: ms(140)
    // How much a lifted piece grows: its share, but never more than `side`
    // on each side, which keeps Itasca's 4 between it and a neighbour
    // (TableSizes: tabs 8 apart, cards 12).
    // A card as a screen reader says it: its application, its window's title,
    // and whether it is a Stack.
    function cardName(entry) {
        const parts = [entry.application || "", entry.title || ""].filter(p => p.length > 0);
        if ((entry.stacked || 0) > 0) parts.push("Stack");
        return parts.join(", ");
    }
    function growth(width, share, side) {
        return width > 0 ? Math.min(share, 2 * side / width) : 0;
    }
    // How much a crowded row shows: "all", "noColours", "namesOnly" or
    // "numbers", after TableTabParts.
    readonly property string parts: model.parts || "all"

    // The dock's band, turned to hang from the top edge and reaching 300 px
    // down to carry the rows, with its own curve for that depth: 80% shade at
    // the edge and clear by its end, along cubic-bezier(0.128, 0, 0.686,
    // 0.971), deepest from 98% of the way. The stops sample that curve at 40
    // intervals, within half a level of it. It fades down from the edge into
    // place at its full depth rather than filling like the dock's tide.
    Rectangle {
        width: parent.width
        height: 300
        y: -12 * (1 - root.bandReveal)
        opacity: root.bandReveal * root.pull * root.cancelShade
        visible: root.bandReveal > 0.001
        gradient: Gradient {
            GradientStop { position: 0.0000; color: Qt.rgba(root.shade.r, root.shade.g, root.shade.b, 0.8000) }
            GradientStop { position: 0.0250; color: Qt.rgba(root.shade.r, root.shade.g, root.shade.b, 0.7996) }
            GradientStop { position: 0.0500; color: Qt.rgba(root.shade.r, root.shade.g, root.shade.b, 0.7956) }
            GradientStop { position: 0.0750; color: Qt.rgba(root.shade.r, root.shade.g, root.shade.b, 0.7889) }
            GradientStop { position: 0.1000; color: Qt.rgba(root.shade.r, root.shade.g, root.shade.b, 0.7800) }
            GradientStop { position: 0.1250; color: Qt.rgba(root.shade.r, root.shade.g, root.shade.b, 0.7691) }
            GradientStop { position: 0.1500; color: Qt.rgba(root.shade.r, root.shade.g, root.shade.b, 0.7565) }
            GradientStop { position: 0.1750; color: Qt.rgba(root.shade.r, root.shade.g, root.shade.b, 0.7423) }
            GradientStop { position: 0.2000; color: Qt.rgba(root.shade.r, root.shade.g, root.shade.b, 0.7268) }
            GradientStop { position: 0.2250; color: Qt.rgba(root.shade.r, root.shade.g, root.shade.b, 0.7100) }
            GradientStop { position: 0.2500; color: Qt.rgba(root.shade.r, root.shade.g, root.shade.b, 0.6921) }
            GradientStop { position: 0.2750; color: Qt.rgba(root.shade.r, root.shade.g, root.shade.b, 0.6733) }
            GradientStop { position: 0.3000; color: Qt.rgba(root.shade.r, root.shade.g, root.shade.b, 0.6535) }
            GradientStop { position: 0.3250; color: Qt.rgba(root.shade.r, root.shade.g, root.shade.b, 0.6329) }
            GradientStop { position: 0.3500; color: Qt.rgba(root.shade.r, root.shade.g, root.shade.b, 0.6115) }
            GradientStop { position: 0.3750; color: Qt.rgba(root.shade.r, root.shade.g, root.shade.b, 0.5894) }
            GradientStop { position: 0.4000; color: Qt.rgba(root.shade.r, root.shade.g, root.shade.b, 0.5668) }
            GradientStop { position: 0.4250; color: Qt.rgba(root.shade.r, root.shade.g, root.shade.b, 0.5435) }
            GradientStop { position: 0.4500; color: Qt.rgba(root.shade.r, root.shade.g, root.shade.b, 0.5197) }
            GradientStop { position: 0.4750; color: Qt.rgba(root.shade.r, root.shade.g, root.shade.b, 0.4955) }
            GradientStop { position: 0.5000; color: Qt.rgba(root.shade.r, root.shade.g, root.shade.b, 0.4708) }
            GradientStop { position: 0.5250; color: Qt.rgba(root.shade.r, root.shade.g, root.shade.b, 0.4458) }
            GradientStop { position: 0.5500; color: Qt.rgba(root.shade.r, root.shade.g, root.shade.b, 0.4204) }
            GradientStop { position: 0.5750; color: Qt.rgba(root.shade.r, root.shade.g, root.shade.b, 0.3948) }
            GradientStop { position: 0.6000; color: Qt.rgba(root.shade.r, root.shade.g, root.shade.b, 0.3689) }
            GradientStop { position: 0.6250; color: Qt.rgba(root.shade.r, root.shade.g, root.shade.b, 0.3428) }
            GradientStop { position: 0.6500; color: Qt.rgba(root.shade.r, root.shade.g, root.shade.b, 0.3165) }
            GradientStop { position: 0.6750; color: Qt.rgba(root.shade.r, root.shade.g, root.shade.b, 0.2902) }
            GradientStop { position: 0.7000; color: Qt.rgba(root.shade.r, root.shade.g, root.shade.b, 0.2638) }
            GradientStop { position: 0.7250; color: Qt.rgba(root.shade.r, root.shade.g, root.shade.b, 0.2375) }
            GradientStop { position: 0.7500; color: Qt.rgba(root.shade.r, root.shade.g, root.shade.b, 0.2112) }
            GradientStop { position: 0.7750; color: Qt.rgba(root.shade.r, root.shade.g, root.shade.b, 0.1851) }
            GradientStop { position: 0.8000; color: Qt.rgba(root.shade.r, root.shade.g, root.shade.b, 0.1593) }
            GradientStop { position: 0.8250; color: Qt.rgba(root.shade.r, root.shade.g, root.shade.b, 0.1340) }
            GradientStop { position: 0.8500; color: Qt.rgba(root.shade.r, root.shade.g, root.shade.b, 0.1091) }
            GradientStop { position: 0.8750; color: Qt.rgba(root.shade.r, root.shade.g, root.shade.b, 0.0851) }
            GradientStop { position: 0.9000; color: Qt.rgba(root.shade.r, root.shade.g, root.shade.b, 0.0622) }
            GradientStop { position: 0.9250; color: Qt.rgba(root.shade.r, root.shade.g, root.shade.b, 0.0409) }
            GradientStop { position: 0.9500; color: Qt.rgba(root.shade.r, root.shade.g, root.shade.b, 0.0220) }
            GradientStop { position: 0.9750; color: Qt.rgba(root.shade.r, root.shade.g, root.shade.b, 0.0071) }
            GradientStop { position: 1.0000; color: Qt.rgba(root.shade.r, root.shade.g, root.shade.b, 0.0000) }
        }
    }

    // Letting go at the edge would cancel: said beneath the dimmed tabs.
    Text {
        visible: text.length > 0
        opacity: root.reveal
        anchors.horizontalCenter: parent.horizontalCenter
        y: root.model.hintY || 0
        text: root.model.hint || ""
        color: root.text2
        font.pixelSize: 13
        Accessible.role: Accessible.StaticText
        Accessible.name: text
    }

    // A Lucide glyph (ITASCA-VISUAL-LANGUAGE.md § Icons), its path copied
    // unchanged from the vendored SVG in lucide/ and stroked as Lucide
    // strokes it, drawn as a shape so nothing can leave it out.
    component Glyph: Shape {
        id: glyph
        property string path
        property color stroke
        property real size: 16
        width: size
        height: size
        Accessible.ignored: true
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeColor: glyph.stroke
            strokeWidth: Math.max(1.25, 2 * glyph.size / 24)
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            scale: Qt.size(glyph.size / 24, glyph.size / 24)
            PathSvg { path: glyph.path }
        }
    }
    readonly property string lucidePlus: "M5 12h14M12 5v14"

    // A stack is its face with the edges of the cards behind it to its left,
    // as Spread's closed stack shows them. Drawn behind the
    // face that holds it, so they lift and are carried with it.
    component Slivers: Repeater {
        id: slivers
        property int count: 0
        model: count
        delegate: Rectangle {
            required property int index
            z: -1 - index
            x: -(index + 1) * root.sliverStep
            width: parent ? parent.width : 0
            height: parent ? parent.height : 0
            radius: 14
            color: root.sliverFills[Math.min(index, root.sliverFills.length - 1)]
            Accessible.ignored: true
        }
    }

    // An application's icon, its name and its window's title.
    component CardFace: Row {
        id: face
        property var entry: ({})
        anchors.left: parent.left
        anchors.leftMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        spacing: 8
        Kirigami.Icon {
            width: 24
            height: 24
            anchors.verticalCenter: parent.verticalCenter
            source: face.entry.icon || "application-x-executable"
            // A symbolic icon takes the text's colour, not the window colours'.
            color: root.text
        }
        Column {
            anchors.verticalCenter: parent.verticalCenter
            Text {
                width: face.entry.textWidth || 0
                text: face.entry.application || ""
                color: root.text
                elide: Text.ElideRight
                font.pixelSize: 13
                font.weight: Font.Medium
            }
            Text {
                width: face.entry.textWidth || 0
                text: face.entry.title || ""
                color: root.text2
                elide: Text.ElideRight
                font.pixelSize: 12
            }
        }
    }

    // The rows come down from the edge.
    Item {
        id: rows
        anchors.fill: parent
        opacity: root.reveal * (root.model.cancelling ? 0.35 : 1)
        transform: [
            Translate {
                y: root.model.cancelling ? -8 : 0
                Behavior on y { NumberAnimation { duration: root.ms(220); easing.type: root.ease } }
            },
            Translate { y: root.drift }
        ]

        Repeater {
            model: root.tabs.length
            delegate: Kirigami.ShadowedRectangle {
                id: tab
                required property int index
                readonly property var entry: root.tabs[index] || ({})
                readonly property bool target: entry.state === "target"
                // Its name is being typed, in place.
                readonly property bool editing: entry.editing === true
                readonly property bool numbersOnly: root.parts === "numbers" && !editing
                // The tab under the finger, or whose cards hang, lifts from
                // the row: a step lighter, a little larger, a soft shadow.
                readonly property bool touched: (entry.state === "hover" || entry.state === "locked" || editing) && !root.carrying
                // The workspace the person is in is the brightest; the
                // others' labels step back until the finger comes to them.
                readonly property bool quiet: entry.current !== true && !touched && !target
                Accessible.role: Accessible.PageTab
                Accessible.name: "Workspace " + (entry.number || "") + (entry.name ? ", " + entry.name : "")
                Accessible.selected: entry.current === true
                property real lift: touched ? 1 : 0
                Behavior on lift {
                    id: tabLift
                    NumberAnimation { duration: tabLift.targetValue > 0 ? root.rise : root.settle; easing.type: root.ease }
                }
                x: entry.x || 0
                y: entry.y || 0
                width: entry.width || 0
                height: entry.height || 0
                radius: height / 2
                scale: 1 + lift * root.growth(width, 0.03, 4)
                shadow.size: 12 * lift
                shadow.yOffset: 3 * lift
                shadow.color: Qt.rgba(0, 0, 0, 0.3 * lift)
                color: target ? root.accent
                    : entry.state === "locked" || editing ? root.selectedFill
                    : entry.state === "hover" ? root.hoverFill
                    : root.raised
                Behavior on color { ColorAnimation { duration: root.ms(160) } }
                Behavior on x { NumberAnimation { duration: root.ms(160); easing.type: root.ease } }
                Behavior on width { NumberAnimation { duration: root.ms(160); easing.type: root.ease } }

                Row {
                    x: tab.numbersOnly ? (tab.width - 20) / 2 : 13
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 8
                    // Every tab carries its desktop's number, the one KDE's
                    // Switch to Desktop shortcuts use.
                    Text {
                        width: 20
                        horizontalAlignment: Text.AlignHCenter
                        anchors.verticalCenter: parent.verticalCenter
                        text: tab.entry.number || ""
                        color: tab.target ? root.accentText : tab.quiet ? root.wash(0.4) : root.text
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        Behavior on color { ColorAnimation { duration: root.ms(160) } }
                    }
                    Text {
                        visible: !tab.numbersOnly && !tab.editing && (tab.entry.nameWidth || 0) > 0
                        width: tab.entry.nameWidth || 0
                        anchors.verticalCenter: parent.verticalCenter
                        text: tab.entry.name || ""
                        color: tab.target ? root.accentText : tab.quiet ? root.wash(0.5) : root.text
                        elide: Text.ElideRight
                        font.pixelSize: 14
                        font.weight: Font.Medium
                        Behavior on color { ColorAnimation { duration: root.ms(160) } }
                    }
                    TextInput {
                        id: editor
                        visible: tab.editing
                        Accessible.role: Accessible.EditableText
                        Accessible.name: "Workspace name"
                        width: tab.entry.nameWidth || 0
                        anchors.verticalCenter: parent.verticalCenter
                        clip: true
                        color: root.text
                        selectionColor: root.accent
                        selectedTextColor: root.accentText
                        font.pixelSize: 14
                        font.weight: Font.Medium
                        cursorVisible: activeFocus
                        // What is shown, including what the keyboard is
                        // still composing, is the name.
                        onDisplayTextChanged: if (tab.editing) root.renameText = displayText
                        Connections {
                            target: root
                            function onEditingStarted(text) {
                                if (!tab.editing) return;
                                editor.text = text;
                                editor.selectAll();
                                editor.forceActiveFocus();
                            }
                            function onEditingEnded() { editor.focus = false; }
                        }
                    }
                    Row {
                        visible: root.parts === "all" && !tab.editing && (tab.entry.icons || []).length > 0
                        opacity: tab.quiet ? 0.5 : 1
                        spacing: 4
                        anchors.verticalCenter: parent.verticalCenter
                        Repeater {
                            model: tab.entry.icons || []
                            Kirigami.Icon { width: 10; height: 10; source: modelData; color: root.text }
                        }
                    }
                }
            }
        }

        // Beside a tab whose name has been cleared: removes the workspace.
        Rectangle {
            readonly property var entry: root.model.remove || ({})
            visible: entry.visible === true
            Accessible.role: Accessible.Button
            Accessible.name: entry.label || "Remove workspace"
            x: entry.x || 0
            y: entry.y || 0
            width: entry.width || 0
            height: entry.height || 0
            radius: height / 2
            color: root.raised
            Text {
                anchors.centerIn: parent
                text: parent.entry.label || ""
                color: root.text
                font.pixelSize: 14
                font.weight: Font.Medium
            }
        }

        // + makes a workspace: tapped, an empty one; with a card carried to
        // it, one for that card. At rest it is a circle.
        Kirigami.ShadowedRectangle {
            id: plus
            readonly property var entry: root.model.plus || ({})
            readonly property bool target: entry.target === true
            Accessible.role: Accessible.Button
            Accessible.name: "New workspace"
            property real lift: entry.hover === true && !target ? 1 : 0
            Behavior on lift {
                id: plusLift
                NumberAnimation { duration: plusLift.targetValue > 0 ? root.rise : root.settle; easing.type: root.ease }
            }
            visible: entry.visible === true
            x: entry.x || 0
            y: entry.y || 0
            width: entry.width || 0
            height: entry.height || 0
            radius: height / 2
            scale: 1 + lift * root.growth(width, 0.03, 4)
            shadow.size: 12 * lift
            shadow.yOffset: 3 * lift
            shadow.color: Qt.rgba(0, 0, 0, 0.3 * lift)
            color: target ? root.accent : entry.hover === true ? root.hoverFill : root.raised
            Behavior on color { ColorAnimation { duration: root.ms(160) } }
            Behavior on x { NumberAnimation { duration: root.ms(160); easing.type: root.ease } }
            Behavior on width { NumberAnimation { duration: root.ms(160); easing.type: root.ease } }
            Row {
                anchors.centerIn: parent
                spacing: 6
                Glyph {
                    anchors.verticalCenter: parent.verticalCenter
                    size: 18
                    path: root.lucidePlus
                    stroke: plus.target ? root.accentText : plus.entry.hover === true ? root.text : root.text2
                }
                Text {
                    visible: text.length > 0
                    anchors.verticalCenter: parent.verticalCenter
                    text: plus.entry.label || ""
                    color: plus.target ? root.accentText : root.text
                    font.pixelSize: 14
                    font.weight: Font.Medium
                }
            }
        }

        // The cards of the tab under the finger. They come up to meet a
        // finger nearing them, and the chosen one leans toward the lift line.
        Repeater {
            model: root.cards.length
            delegate: Kirigami.ShadowedRectangle {
                id: card
                required property int index
                readonly property var entry: root.cards[index] || ({})
                readonly property bool chosen: entry.chosen === true
                readonly property bool arriving: ghost.visible && index === (ghost.drop.card ?? -1)
                Accessible.role: Accessible.ListItem
                Accessible.name: root.cardName(entry)
                Accessible.selected: chosen
                // How lifted the chosen card is, eased, so the dip, the size,
                // the light and the shadow hand over from one card to its
                // neighbour as the finger pages across; the
                // finger's own depth still moves them directly. Pulled toward
                // the lift line it rises further, its shadow growing toward
                // the card in hand's.
                property real held: chosen ? 1 : 0
                Behavior on held {
                    id: cardLift
                    NumberAnimation { duration: cardLift.targetValue > 0 ? root.rise : root.settle; easing.type: root.ease }
                }
                // A stack's slot holds its edges, so its face stands right of them.
                readonly property int stacked: entry.stacked || 0
                x: (entry.x || 0) + stacked * root.sliverStep
                y: (entry.y || 0) - 10 * (1 - root.approach) + held * 18 * root.lean
                width: (entry.width || 0) - stacked * root.sliverStep
                height: entry.height || 0
                radius: 14
                scale: (0.94 + 0.06 * root.approach) * (1 - held) + (1 + root.growth(entry.width || 0, 0.03 + 0.02 * root.lean, 8)) * held
                shadow.size: held * (12 + 10 * root.lean)
                shadow.yOffset: held * (3 + 4 * root.lean)
                shadow.color: Qt.rgba(0, 0, 0, held * (0.3 + 0.15 * root.lean))
                color: Qt.tint(root.surface, Qt.rgba(root.chosenCard.r, root.chosenCard.g, root.chosenCard.b, held))
                opacity: entry.carried || arriving ? 0 : 0.45 + 0.55 * root.approach
                Behavior on x { NumberAnimation { duration: root.ms(160); easing.type: root.ease } }
                Slivers { count: card.stacked }
                CardFace { entry: card.entry }
            }
        }

        Rectangle {
            readonly property var empty: root.model.empty || ({})
            visible: empty.visible === true
            Accessible.role: Accessible.StaticText
            Accessible.name: empty.text || ""
            x: empty.x || 0
            y: (empty.y || 0) - 10 * (1 - root.approach)
            width: empty.width || 0
            height: empty.height || 0
            radius: 14
            opacity: 0.45 + 0.55 * root.approach
            color: root.surface
            Text {
                anchors.centerIn: parent
                text: parent.empty.text || ""
                color: root.text2
                font.pixelSize: 12
            }
        }

        // The card in hand, hanging from the finger. It pops free as it
        // lifts.
        Kirigami.ShadowedRectangle {
            id: lifted
            readonly property var entry: root.model.carried || null
            readonly property int stacked: entry ? entry.stacked || 0 : 0
            property real pop: 1.04
            visible: entry !== null
            Accessible.role: Accessible.ListItem
            Accessible.name: entry ? "Moving " + root.cardName(entry) : ""
            x: entry ? entry.x + stacked * root.sliverStep : 0
            y: entry ? entry.y : 0
            width: entry ? entry.width - stacked * root.sliverStep : 0
            height: entry ? entry.height : 0
            scale: pop
            radius: 14
            color: root.chosenCard
            shadow.size: 24
            shadow.yOffset: 8
            shadow.color: Qt.rgba(0, 0, 0, 0.5)
            onVisibleChanged: if (visible) popping.restart()
            SequentialAnimation {
                id: popping
                NumberAnimation { target: lifted; property: "pop"; from: 1.0; to: 1.14; duration: root.ms(90); easing.type: Easing.OutQuad }
                NumberAnimation { target: lifted; property: "pop"; to: 1.04; duration: root.ms(240); easing.type: Easing.OutBack }
            }
            Slivers { count: lifted.stacked }
            CardFace { entry: lifted.entry || ({}) }
        }

        // A moved card lands from where it was let go into its place in the
        // row it joined.
        Kirigami.ShadowedRectangle {
            id: ghost
            readonly property var drop: root.model.drop || ({})
            readonly property int seq: drop.seq ?? 0
            Accessible.ignored: true
            readonly property int stacked: drop.stacked || 0
            visible: false
            width: (drop.width || 0) - stacked * root.sliverStep
            height: drop.height || 0
            radius: 14
            color: root.chosenCard
            shadow.size: 24
            shadow.yOffset: 8
            shadow.color: Qt.rgba(0, 0, 0, 0.5)
            onSeqChanged: {
                if (seq <= 0) return;
                x = drop.x + stacked * root.sliverStep;
                y = drop.y;
                scale = 1.04;
                visible = true;
                landingAnimation.restart();
            }
            ParallelAnimation {
                id: landingAnimation
                NumberAnimation { target: ghost; property: "x"; to: (ghost.drop.toX ?? 0) + ghost.stacked * root.sliverStep; duration: root.ms(300); easing.type: Easing.OutCubic }
                NumberAnimation { target: ghost; property: "y"; to: ghost.drop.toY ?? 0; duration: root.ms(300); easing.type: Easing.OutCubic }
                NumberAnimation { target: ghost; property: "scale"; to: 1.0; duration: root.ms(300); easing.type: Easing.OutCubic }
                onFinished: ghost.visible = false
            }
            Slivers { count: ghost.stacked }
            CardFace { entry: ghost.drop }
        }
    }
}
