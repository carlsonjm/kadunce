/* SPDX-License-Identifier: GPL-2.0-or-later */
// Behavioral coverage of Gooseberry's notes on Spread's cards: what Kadunce
// reads from the bus, where each note is drawn on its card, and the one
// contact the notes hold from press to release.
#include "StuckNotes.h"

#include <cstdlib>
#include <iostream>

using namespace Kadunce;

namespace {
void require(bool value, const char *message)
{
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}

QVariantMap note(const QString &id, const QString &title, const QString &colour)
{
    return {{QStringLiteral("id"), id}, {QStringLiteral("title"), title},
            {QStringLiteral("text"), QString(title + QStringLiteral(" and more"))},
            {QStringLiteral("colour"), QStringLiteral("yellow")},
            {QStringLiteral("colourHex"), colour}};
}

// A note as a Gooseberry that tells where it stands on its window sends it.
QVariantMap placedNote(const QString &id, const QString &title, double x, double y,
                       double width = -1, double height = -1)
{
    QVariantMap map = note(id, title, QStringLiteral("#9CC9FF"));
    map.insert(QStringLiteral("x"), x);
    map.insert(QStringLiteral("y"), y);
    map.insert(QStringLiteral("width"), width);
    map.insert(QStringLiteral("height"), height);
    return map;
}

QVariantMap window(const QStringList &ids, const QVariantList &notes, uint count, bool shown = false)
{
    return {{QStringLiteral("windowIds"), ids}, {QStringLiteral("caption"), QStringLiteral("Report")},
            {QStringLiteral("app"), QStringLiteral("org.kde.kate")},
            {QStringLiteral("window"), QStringLiteral("report.txt")},
            {QStringLiteral("count"), count}, {QStringLiteral("notes"), notes},
            {QStringLiteral("shown"), shown}};
}

const QUuid Left(QStringLiteral("{11111111-1111-1111-1111-111111111111}"));
const QUuid Centre(QStringLiteral("{22222222-2222-2222-2222-222222222222}"));
const QUuid Right(QStringLiteral("{33333333-3333-3333-3333-333333333333}"));
const QRectF CentreCard(700, 300, 1160, 760);
const QRectF LeftCard(0, 360, 640, 420);
const QRectF RightCard(1920, 360, 640, 420);
const QList<NotesCard> Cards{{Centre, CentreCard}, {Left, LeftCard}, {Right, RightCard}};
} // namespace

int main()
{
    // Windows() as it arrives: braces or none, X11 ids ignored, empty dropped.
    const auto entries = stuckNotesEntries({
        window({Centre.toString()},
               {note(QStringLiteral("a"), QStringLiteral("Call back"), QStringLiteral("#FFE680")),
                note(QStringLiteral("b"), QStringLiteral("Totals"), QStringLiteral("#A8E6A1")),
                note(QStringLiteral("c"), QStringLiteral("Ask Sam"), QStringLiteral("#9CC9FF")),
                note(QStringLiteral("d"), QStringLiteral("Later"), QStringLiteral("#FFB5A8"))}, 4),
        window({Left.toString(QUuid::WithoutBraces), QStringLiteral("41943047")},
               {note(QStringLiteral("e"), QStringLiteral("One"), QStringLiteral("#FFE680"))}, 1),
        window({QStringLiteral("41943048")},
               {note(QStringLiteral("f"), QStringLiteral("X11"), QStringLiteral("#FFE680"))}, 1),
        window({Right.toString()}, {}, 0),
    });
    require(entries.size() == 2, "Entries without a UUID or without notes were kept");
    require(entries.at(0).count == 4 && entries.at(0).notes.first().title == QStringLiteral("Call back"),
        "The top note did not come first");
    require(entries.at(1).windowIds == QList<QUuid>{Left}, "A UUID without braces did not match");

    StuckNotesSpread notes;
    require(notes.isEmpty(), "Notes appeared before Gooseberry said any");
    notes.setEntries(entries);
    require(notes.entryFor(Centre) && notes.entryFor(Left) && !notes.entryFor(Right),
        "Cards were matched to the wrong notes");

    // The stack is the same size on a near card and a far one, at the corner.
    const auto centreStack = noteStackSquares(CentreCard, 4);
    const auto leftStack = noteStackSquares(LeftCard, 1);
    require(centreStack.size() == 2 && leftStack.size() == 1 && noteStackSquares(CentreCard, 9).size() == 2,
        "The stack was not a mini note with the next one's edge behind it");
    require(centreStack.first().top() < centreStack.last().top(),
        "The next note's edge did not peek above the top note");
    require(centreStack.last().size() == leftStack.last().size(), "Stacks differed in size between cards");
    require(CentreCard.contains(noteStackReach(CentreCard, 4)), "The stack left its card");
    require(qAbs(centreStack.last().right() - (CentreCard.right() - NoteGeometry::Inset)) < 0.5
                && qAbs(centreStack.last().bottom() - (CentreCard.bottom() - NoteGeometry::Inset)) < 0.5,
        "The top note was not at the bottom-right corner");
    require(noteStackReach(CentreCard, 4).width() >= 48, "The stack was too small to touch");

    using Kind = StuckNotesSpread::Request::Kind;
    const QPointF centreStackPoint = centreStack.last().center();
    const QPointF leftStackPoint = noteStackSquares(LeftCard, 1).last().center();
    // A press off every stack is not the notes'.
    require(!notes.press(CentreCard.center(), Cards), "A press on a card was taken by its notes");

    // A tap on a stack brings the window's notes up, and asks nothing else:
    // the effect opens the card, so the notes are typed on the window.
    require(notes.press(centreStackPoint, Cards), "A press on a stack was not the notes'");
    auto tap = notes.release(centreStackPoint, true, Cards);
    require(tap && tap->kind == Kind::Show && tap->window == Centre && tap->noteId.isEmpty(),
        "A tap on a stack did not ask Gooseberry to bring the notes up");
    // A tap never puts them away, however they stand.
    auto shownEntries = entries;
    shownEntries[0].shown = true;
    notes.setEntries(shownEntries);
    require(notes.shown(Centre) && !notes.shown(Left), "Shown was not taken from Gooseberry");
    (void)notes.press(centreStackPoint, Cards);
    tap = notes.release(centreStackPoint, true, Cards);
    require(tap && tap->kind == Kind::Show, "A tap on a stack whose notes are up put them away");
    // Shown is read from the bus as well.
    require(stuckNotesEntries({window({Centre.toString()},
                {note(QStringLiteral("a"), QStringLiteral("x"), QStringLiteral("#FFE680"))}, 1, true)})
                .first().shown, "An entry's shown was not read");

    // Presses elsewhere are not the notes': a card still opens and the row
    // still moves. A stroke on the stack asks nothing.
    require(!notes.press(LeftCard.center(), Cards), "A press beside a stack was taken");
    require(!notes.press(CentreCard.topLeft() + QPointF(20, 20), Cards), "A press on a card's face was taken");
    require(notes.press(centreStackPoint, Cards), "A press on a stack was not the notes'");
    require(!notes.release(centreStackPoint + QPointF(40, 0), false, Cards), "A moving stroke asked something");
    require(notes.press(leftStackPoint, Cards), "A press on a second stack was not the notes'");
    tap = notes.release(leftStackPoint, true, Cards);
    require(tap && tap->window == Left, "A tap on a second card's stack named another card");

    // Holding the stack carries its top note; carried to another card it is
    // stuck there; let go off every card, nothing.
    require(notes.press(centreStackPoint, Cards) && notes.hold(), "Holding a stack carried nothing");
    require(notes.carriedNote()->id == QStringLiteral("a"), "Holding a stack did not carry its top note");
    notes.move(RightCard.center());
    require(notes.dropTarget(Cards) == Right, "The card under a carried note was not its target");
    notes.move(CentreCard.center());
    require(notes.dropTarget(Cards).isNull(), "A note's own card was a target");
    const auto stick = notes.release(RightCard.center(), false, Cards);
    require(stick && stick->kind == Kind::Stick && stick->noteId == QStringLiteral("a") && stick->window == Right,
        "Letting a carried note go on another card did not stick it there");
    require(notes.press(centreStackPoint, Cards) && notes.hold(), "Holding a stack again carried nothing");
    require(!notes.release(QPointF(5, 5), false, Cards), "A note let go off every card was stuck");
    // Let go on its own card, nothing, and a held stack is no tap.
    (void)notes.press(centreStackPoint, Cards);
    (void)notes.hold();
    require(!notes.release(centreStackPoint, true, Cards), "A held stack let go in place asked something");

    // A note that goes while carried is dropped; a cancelled contact asks
    // nothing; Gooseberry going away clears everything.
    (void)notes.press(centreStackPoint, Cards);
    (void)notes.hold();
    notes.setEntries({entries.at(1)});
    require(!notes.carrying(), "A note gone from Gooseberry stayed in hand");
    notes.setEntries(shownEntries);
    (void)notes.press(centreStackPoint, Cards);
    notes.cancel();
    require(!notes.release(centreStackPoint, true, Cards), "A cancelled press asked something");
    notes.setEntries({});
    require(notes.isEmpty() && !notes.press(centreStackPoint, Cards), "Notes outlived Gooseberry");

    // A Gooseberry that says where each note stands on its window. Kadunce
    // reads the place and size and tolerates either being absent or unread.
    const auto placedEntries = stuckNotesEntries({window({Centre.toString()},
        {placedNote(QStringLiteral("p"), QStringLiteral("Placed"), 0.1, 0.2, 400, 300),
         note(QStringLiteral("q"), QStringLiteral("Loose"), QStringLiteral("#FFE680")),
         placedNote(QStringLiteral("r"), QStringLiteral("Started"), 0.6, 0.1),
         placedNote(QStringLiteral("s"), QStringLiteral("Never"), -1, -1),
         note(QStringLiteral("t"), QStringLiteral("Loose too"), QStringLiteral("#FFB5A8"))}, 5)});
    const auto &read = placedEntries.first().notes;
    require(read.at(0).placed() && read.at(0).place == QPointF(0.1, 0.2) && read.at(0).size == QSizeF(400, 300),
        "A note's place and size were not read");
    require(read.at(2).placed() && read.at(2).size.isEmpty(), "A note never resized was read with a size");
    require(!read.at(1).placed() && !read.at(3).placed(), "A note with no place was read as placed");
    {
        QVariantMap odd = placedNote(QStringLiteral("o"), QStringLiteral("Odd"), 0.5, 0.5);
        odd.insert(QStringLiteral("x"), QStringLiteral("left"));
        QVariantMap past = placedNote(QStringLiteral("o2"), QStringLiteral("Past"), 1.5, 0.5);
        const auto oddEntries = stuckNotesEntries({window({Centre.toString()}, {odd, past}, 2)});
        require(!oddEntries.first().notes.at(0).placed() && !oddEntries.first().notes.at(1).placed(),
            "A place that cannot be read did not leave the note in the stack");
    }

    // The card is its window scaled: a 2320 by 1520 window on CentreCard.
    const QSizeF centreWindow(2320, 1520);
    const QList<NotesCard> placedCards{{Centre, CentreCard, centreWindow}, {Left, LeftCard, QSizeF(1280, 840)}};
    const auto &placedEntry = placedEntries.first();
    // Each placed note in its place, the oldest lowest, and the stack over
    // them with only the notes never placed.
    auto marks = cardNoteMarks(CentreCard, {}, centreWindow, placedEntry);
    require(marks.size() == 3, "A card did not show its placed notes and one stack");
    require(marks.at(0).note == 2 && marks.at(1).note == 0, "Placed notes were not drawn oldest lowest");
    require(marks.last().kind == CardNoteMark::Kind::Stack && marks.last().note == 1 && marks.last().count == 3,
        "The stack did not hold just the notes never placed, the top one first");
    require(marks.last().sheetNotes == QList<int>({3, 1}), "The stack's sheets showed placed notes");
    require(marks.at(1).rect == placedNoteRect(CentreCard, centreWindow, QPointF(0.1, 0.2), QSizeF(400, 300)),
        "A placed note was not drawn where it stands on the window");
    require(qAbs(marks.at(1).rect.x() - (CentreCard.x() + 0.1 * CentreCard.width())) < 0.5,
        "A placed note was not at its share of the card");
    // Shown or put away, the notes stand the same: no fan comes up over the card.
    require(cardNoteMarks(CentreCard, {}, centreWindow, placedEntry).size() == 3,
        "A card's notes changed what they show");
    // Every note placed: no stack at all.
    const auto allPlaced = stuckNotesEntries({window({Left.toString()},
        {placedNote(QStringLiteral("u"), QStringLiteral("Only"), 0.5, 0.5)}, 1)});
    require(cardNoteMarks(LeftCard, {}, QSizeF(1280, 840), allPlaced.first()).size() == 1,
        "A card whose notes are all placed showed a stack");
    // A window of unknown size: a placed note is not drawn rather than drawn
    // in the stack, so it never starts from the corner and moves to its place.
    {
        const auto unknown = cardNoteMarks(CentreCard, {}, QSizeF(), placedEntry);
        require(unknown.size() == 1 && unknown.first().kind == CardNoteMark::Kind::Stack
                    && unknown.first().count == 3 && unknown.first().sheetNotes == QList<int>({3, 1}),
            "Without a window size, a placed note went to the stack");
    }

    // The window drawn smaller than its card, as KWin fits it: placed notes
    // stand on the window as drawn, the stack keeps to the card's corner.
    {
        const QRectF face = CentreCard.adjusted(40, 30, -40, -30);
        const auto onFace = cardNoteMarks(CentreCard, face, centreWindow, placedEntry);
        require(onFace.at(1).rect == placedNoteRect(face, centreWindow, QPointF(0.1, 0.2), QSizeF(400, 300)),
            "A placed note did not stand on the window as drawn");
        require(onFace.last().rect == marks.last().rect, "The stack left the card's corner for the window's");
    }

    // A frame mid-motion: the card halfway from where it stood to its slot
    // carries each placed note halfway too, with nothing kept from the frame
    // before.
    {
        const QRectF from(0, 0, 2320, 1520);
        const QRectF halfway(350, 150, 1740, 1140);
        const auto start = cardNoteMarks(from, {}, centreWindow, placedEntry);
        const auto middle = cardNoteMarks(halfway, {}, centreWindow, placedEntry);
        const auto end = cardNoteMarks(CentreCard, {}, centreWindow, placedEntry);
        for (int index = 0; index < 2; ++index) {
            const QRectF a = start.at(index).rect;
            const QRectF b = end.at(index).rect;
            const QRectF m = middle.at(index).rect;
            require(qAbs(m.x() - (a.x() + b.x()) / 2) < 0.5 && qAbs(m.y() - (a.y() + b.y()) / 2) < 0.5
                        && qAbs(m.width() - (a.width() + b.width()) / 2) < 0.5
                        && qAbs(m.height() - (a.height() + b.height()) / 2) < 0.5,
                "A note did not move with its card mid-motion");
        }
        // The same frame again, after another, draws the same.
        require(cardNoteMarks(halfway, {}, centreWindow, placedEntry).at(1).rect == middle.at(1).rect,
            "A note's place depended on the frame before");
    }

    // A tap follows what is drawn: on a placed note it asks for that note
    // ready to type on its window; on the stack it brings the notes up; on
    // the card's face beside them it is not the notes'.
    notes.setEntries(placedEntries + allPlaced);
    const QPointF onPlaced = marks.at(1).rect.center();
    require(notes.press(onPlaced, placedCards) && notes.contact() == StuckNotesSpread::Contact::Note,
        "A press on a placed note was not the note's");
    tap = notes.release(onPlaced, true, placedCards);
    require(tap && tap->kind == Kind::Write && tap->window == Centre && tap->noteId == QStringLiteral("p"),
        "A tap on a placed note did not ask for it ready to type on its window");
    // Asked again with the notes up, it asks the same: nothing is put away.
    auto placedShown = placedEntries;
    placedShown[0].shown = true;
    notes.setEntries(placedShown + allPlaced);
    require(notes.press(onPlaced, placedCards), "Up, a placed note was not where it was drawn");
    tap = notes.release(onPlaced, true, placedCards);
    require(tap && tap->kind == Kind::Write && tap->noteId == QStringLiteral("p"),
        "A tap on a placed note whose notes are up did not ask for it to type on");
    const QPointF onStack = marks.last().rect.center();
    require(notes.press(onStack, placedCards) && notes.contact() == StuckNotesSpread::Contact::Stack,
        "A press on the stack of loose notes was not the stack's");
    tap = notes.release(onStack, true, placedCards);
    require(tap && tap->kind == Kind::Show && tap->window == Centre, "A tap on the stack did not bring the notes up");
    require(notes.press(onStack, placedCards) && notes.hold() && notes.carriedNote()->id == QStringLiteral("q"),
        "Holding the stack did not carry its top loose note");
    notes.cancel();
    require(!notes.press(QPointF(CentreCard.right() - 300, CentreCard.bottom() - 60), placedCards),
        "A press on the card's face, clear of every note, was taken");
    // Pressed where it is drawn this frame: a card drawn smaller than its
    // slot is pressed on its window's notes there.
    {
        const QRectF face = CentreCard.adjusted(40, 30, -40, -30);
        const QList<NotesCard> faced{{Centre, CentreCard, centreWindow, face}};
        const QPointF onDrawn = placedNoteRect(face, centreWindow, QPointF(0.1, 0.2), QSizeF(400, 300)).center();
        require(notes.press(onDrawn, faced) && notes.contact() == StuckNotesSpread::Contact::Note,
            "A press on a note where it is drawn was not the note's");
        notes.cancel();
    }
    // Where placed notes overlap, the one on top is the one pressed.
    const auto overlapping = stuckNotesEntries({window({Left.toString()},
        {placedNote(QStringLiteral("top"), QStringLiteral("Top"), 0.3, 0.3, 400, 300),
         placedNote(QStringLiteral("under"), QStringLiteral("Under"), 0.32, 0.32, 400, 300)}, 2)});
    notes.setEntries(overlapping);
    const auto overlapMarks = cardNoteMarks(LeftCard, {}, QSizeF(1280, 840), overlapping.first());
    const QPointF both = overlapMarks.at(0).rect.intersected(overlapMarks.at(1).rect).center();
    require(notes.press(both, placedCards) && notes.contact() == StuckNotesSpread::Contact::Note,
        "A press on overlapping notes was not the notes'");
    tap = notes.release(both, true, placedCards);
    require(tap && tap->kind == Kind::Write && tap->noteId == QStringLiteral("top"),
        "A tap on overlapping notes asked for the one beneath");
    require(notes.press(both, placedCards) && notes.hold() && notes.carriedNote()->id == QStringLiteral("top"),
        "A press on overlapping notes took the one beneath");
    // Carried to another card, a placed note sticks there as any note does.
    notes.move(CentreCard.center());
    const auto moved = notes.release(CentreCard.center(), false, placedCards);
    require(moved && moved->kind == Kind::Stick && moved->noteId == QStringLiteral("top") && moved->window == Centre,
        "A placed note carried to another card was not stuck there");

    // The card a tap on its notes opens is tapped where the notes were let
    // go when that reaches it, or else where on the card a tap does.
    {
        const auto everywhereCentre = [](const QPointF &) { return Centre; };
        auto at = StuckNotesSpread::openingPoint(Centre, onPlaced, placedCards, everywhereCentre);
        require(at && *at == onPlaced, "A card was not opened where its note was tapped");
        // A nearer card covers the note's spot: a part of the card it does
        // not cover is tapped instead.
        const QPointF covered = onPlaced;
        const auto coveredThere = [covered](const QPointF &point) {
            return point == covered ? Left : Centre;
        };
        at = StuckNotesSpread::openingPoint(Centre, covered, placedCards, coveredThere);
        require(at && *at != covered && CentreCard.contains(*at), "A covered spot opened the nearer card");
        // Off the card, as a stack's reach can be: the card itself is tapped.
        at = StuckNotesSpread::openingPoint(Centre, QPointF(-10, -10), placedCards, everywhereCentre);
        require(at && *at == CentreCard.center(), "A tap off the card did not open the card");
        // A card no tap reaches, or one not in Spread, is not opened.
        require(!StuckNotesSpread::openingPoint(Centre, onPlaced, placedCards, [](const QPointF &) { return Left; }),
            "A card no tap reaches was opened");
        require(!StuckNotesSpread::openingPoint(Right, onPlaced, placedCards, everywhereCentre),
            "A card not in Spread was opened");
    }
    return 0;
}
