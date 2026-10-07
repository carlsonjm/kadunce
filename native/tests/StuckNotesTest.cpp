/* SPDX-License-Identifier: GPL-2.0-or-later */
// Behavioral coverage of Gooseberry's notes on Spread's cards: what Kadunce
// reads from the bus, the stack and fan geometry, and the one contact the
// notes hold from press to release.
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
    require(centreStack.size() == 1 && leftStack.size() == 1 && noteStackSquares(CentreCard, 9).size() == 1,
        "The stack was not one note");
    require(noteStackReach(CentreCard, 4).contains(noteStackBadge(centreStack.last())),
        "The count's badge was outside the stack's reach");
    require(noteStackReach(LeftCard, 1).width() < noteStackReach(CentreCard, 4).width(),
        "A single note's reach made room for a badge it does not show");
    require(centreStack.last().size() == leftStack.last().size(), "Stacks differed in size between cards");
    require(CentreCard.contains(noteStackReach(CentreCard, 4)), "The stack left its card");
    require(qAbs(centreStack.last().right() - (CentreCard.right() - NoteGeometry::Inset)) < 0.5
                && qAbs(centreStack.last().bottom() - (CentreCard.bottom() - NoteGeometry::Inset)) < 0.5,
        "The top note was not at the bottom-right corner");
    require(noteStackReach(CentreCard, 4).width() >= 48, "The stack was too small to touch");

    // The fan lies over its card, the top note at the corner, none overlapping.
    const auto fan = fannedNoteRects(CentreCard, 4);
    require(fan.size() == 4, "Not every note fanned out over a large card");
    for (int i = 0; i < fan.size(); ++i) {
        require(CentreCard.contains(fan.at(i)), "A fanned note left its card");
        for (int j = i + 1; j < fan.size(); ++j)
            require(!fan.at(i).intersects(fan.at(j)), "Two fanned notes overlapped");
    }
    require(qAbs(fan.first().bottom() - centreStack.last().bottom()) < 0.5
                && qAbs(fan.first().right() - centreStack.last().right()) < 0.5,
        "The top note did not fan out in the stack's corner");
    require(fannedNoteRects(QRectF(0, 0, 60, 40), 3).size() == 1, "A tiny card lost its top note");

    using Kind = StuckNotesSpread::Request::Kind;
    const QPointF centreStackPoint = centreStack.last().center();
    const QPointF leftStackPoint = noteStackSquares(LeftCard, 1).last().center();
    // A press off every stack is not the notes', fanned or not.
    require(!notes.press(CentreCard.center(), Cards), "A press on a card was taken by its notes");

    // A tap on a stack asks Gooseberry to toggle it and shows it fanned until
    // Gooseberry answers; Gooseberry's answer is the truth.
    require(notes.press(centreStackPoint, Cards), "A press on a stack was not the notes'");
    auto toggle = notes.release(centreStackPoint, true, Cards);
    require(toggle && toggle->kind == Kind::Toggle && toggle->window == Centre,
        "A tap on a stack did not ask Gooseberry to toggle it");
    require(notes.fanned(Centre), "A tapped stack did not fan while Gooseberry answered");
    notes.setEntries(entries);
    require(!notes.fanned(Centre), "A guess outlived Gooseberry's answer");
    // A reply after Gooseberry has already said what it holds changes nothing.
    require(!notes.toggleAnswered(Centre, std::nullopt) && !notes.fanned(Centre),
        "A late reply overrode Gooseberry's word");

    // Gooseberry's reply to a toggle confirms the guess or ends it, since a
    // toggle that changes nothing brings no signal.
    (void)notes.press(centreStackPoint, Cards);
    (void)notes.release(centreStackPoint, true, Cards);
    require(!notes.toggleAnswered(Centre, true) && notes.fanned(Centre),
        "A reply that agreed with the guess dropped it or asked for a read");
    notes.setEntries(entries);
    (void)notes.press(centreStackPoint, Cards);
    (void)notes.release(centreStackPoint, true, Cards);
    require(notes.toggleAnswered(Centre, false) && !notes.fanned(Centre),
        "A reply that contradicted the guess left Spread disagreeing with the window");
    (void)notes.press(centreStackPoint, Cards);
    (void)notes.release(centreStackPoint, true, Cards);
    require(notes.toggleAnswered(Centre, std::nullopt) && !notes.fanned(Centre),
        "A failed toggle left its guess standing");
    require(!notes.toggleAnswered(Centre, std::nullopt), "A reply with no guess left asked for a read");
    // Only the window answered for loses its guess.
    (void)notes.press(centreStackPoint, Cards);
    (void)notes.release(centreStackPoint, true, Cards);
    (void)notes.press(leftStackPoint, Cards);
    (void)notes.release(leftStackPoint, true, Cards);
    require(notes.toggleAnswered(Left, std::nullopt) && notes.fanned(Centre) && !notes.fanned(Left),
        "A failed toggle on one card dropped another card's guess");
    notes.setEntries(entries);
    auto shownEntries = entries;
    shownEntries[0].shown = true;
    notes.setEntries(shownEntries);
    require(notes.fanned(Centre), "Notes shown over a window did not arrive fanned");
    // Shown is read from the bus as well.
    require(stuckNotesEntries({window({Centre.toString()},
                {note(QStringLiteral("a"), QStringLiteral("x"), QStringLiteral("#FFE680"))}, 1, true)})
                .first().shown, "An entry's shown was not read");

    // Fanned, presses elsewhere are not the notes': a card still opens and the
    // row still moves, and the notes stay out.
    require(!notes.press(LeftCard.center(), Cards), "A press beside a fan was taken");
    require(!notes.press(CentreCard.topLeft() + QPointF(20, 20), Cards), "A press on a fanned card's face was taken");
    require(notes.fanned(Centre), "A press elsewhere folded the notes");
    // A stroke on the stack asks nothing.
    require(notes.press(centreStackPoint, Cards), "A press on a fanned stack was not the notes'");
    require(!notes.release(centreStackPoint + QPointF(40, 0), false, Cards), "A moving stroke toggled a stack");
    // Several cards may be fanned at once.
    require(notes.press(leftStackPoint, Cards), "A press on a second stack was not the notes'");
    toggle = notes.release(leftStackPoint, true, Cards);
    require(toggle && toggle->window == Left && notes.fanned(Left) && notes.fanned(Centre),
        "Fanning a second card folded the first");
    // Only a tap on its own stack folds it.
    (void)notes.press(centreStackPoint, Cards);
    toggle = notes.release(centreStackPoint, true, Cards);
    require(toggle && toggle->window == Centre && !notes.fanned(Centre), "A tap on a fanned stack did not fold it");
    notes.setEntries(shownEntries);

    // Hold a fanned note and carry it to another card: it is stuck there.
    const QPointF third = fan.at(2).center();
    require(notes.press(third, Cards) && notes.contact() == StuckNotesSpread::Contact::Note,
        "A press on a fanned note was not the note's");
    require(notes.hold() && notes.carrying(), "Holding a fanned note did not carry it");
    require(notes.carriedNote()->id == QStringLiteral("c"), "The wrong note was carried");
    notes.move(RightCard.center());
    require(notes.dropTarget(Cards) == Right, "The card under a carried note was not its target");
    notes.move(CentreCard.center());
    require(notes.dropTarget(Cards).isNull(), "A note's own card was a target");
    const auto stick = notes.release(RightCard.center(), false, Cards);
    require(stick && stick->kind == Kind::Stick && stick->noteId == QStringLiteral("c") && stick->window == Right,
        "Letting a carried note go on another card did not stick it there");
    require(!notes.carrying() && notes.fanned(Centre), "Sticking a note changed what is fanned");
    // The fan takes the stack's place: a tap on a fanned note folds them.
    (void)notes.press(third, Cards);
    const auto fold = notes.release(third, true, Cards);
    require(fold && fold->kind == Kind::Toggle && fold->window == Centre && !notes.fanned(Centre),
        "A tap on a fanned note did not fold them");
    notes.setEntries(shownEntries);

    // Holding the stack carries its top note; let go off every card, nothing.
    require(notes.press(centreStackPoint, Cards) && notes.hold(), "Holding a stack carried nothing");
    require(notes.carriedNote()->id == QStringLiteral("a"), "Holding a stack did not carry its top note");
    require(!notes.release(QPointF(5, 5), false, Cards), "A note let go off every card was stuck");
    // Let go on its own card, nothing, and a held stack is no tap.
    (void)notes.press(centreStackPoint, Cards);
    (void)notes.hold();
    require(!notes.release(centreStackPoint, true, Cards), "A held stack let go in place asked something");

    // A note that goes while carried is dropped; a cancelled contact asks
    // nothing; Gooseberry going away clears everything.
    (void)notes.press(third, Cards);
    (void)notes.hold();
    notes.setEntries({entries.at(1)});
    require(!notes.carrying() && !notes.fanned(Centre), "A note gone from Gooseberry stayed in hand");
    notes.setEntries(shownEntries);
    (void)notes.press(centreStackPoint, Cards);
    notes.cancel();
    require(!notes.release(centreStackPoint, true, Cards), "A cancelled press asked something");
    notes.setEntries({});
    require(notes.isEmpty() && !notes.press(centreStackPoint, Cards), "Notes outlived Gooseberry");
    return 0;
}
