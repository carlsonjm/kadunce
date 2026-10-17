/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QHash>
#include <QList>
#include <QSet>
#include <QString>

#include <vector>

namespace Kadunce
{

// CARD-LIFECYCLE.md §11, Every display: a card's status is one fact, wherever
// it stands. Today that fact is spread over the card stage's row and its
// Stacks behind panes, each display's layout, and the effect's lists of which
// display a waiting card belongs to. Each move keeps them in step by hand, and
// a move that forgets one leaves a card whose status depends on which record
// is read. The card book reads them all at once, as one entry per window, and
// names every place they disagree.
//
// It reports and never repairs, as the ownership ledger does: a mismatch is a
// defect in the move that made it, kept visible so the move can be found.

struct CardBookDisplay {
    QString name;
    // Spread's display: its waiting cards wait for no other display.
    bool spread = false;
    // It holds its own cards, so a card may wait for it.
    bool holdsCards = false;
    // Sized Solo: it shows one card at a time.
    bool solo = false;
};

struct CardBookEntry {
    quintptr window = 0;
    // The display the window stands on.
    QString screen;
    // Held by the card stage, in Spread's row.
    bool card = false;
    // The display whose layout holds it as a pane; empty when none does.
    QString paneOf;
    // The display whose own card stage shows it; empty when none does.
    QString stage;
    // The display a waiting card waits for; empty for Spread's display.
    QString home;
    // Every pane whose Stack lists it behind that pane.
    std::vector<quintptr> behindPanes;
    // The front card of the Stack it is in, in Spread's row; 0 in none.
    quintptr stackFace = 0;
    bool minimized = false;
};

struct CardBookMismatch {
    enum class Rule {
        // A card is held twice: by Spread's stage, a layout or a display's
        // own stage, any two at once.
        CardAndPane,
        // A layout, or a display's own stage, shows a window that stands on
        // another display.
        PaneOffItsDisplay,
        // A card waits for Spread's display, or one that keeps no cards. A
        // display that is gone is not one: its cards wait for it to return.
        WaitsForNoCardDisplay,
        // A window a display shows is also said to wait for a display.
        PaneWaits,
        // A Stack lists a window a display shows behind another. A window
        // that is not a card yet may be listed, as one arriving is.
        StackMemberIsPane,
        // A Stack is kept behind a window no display shows.
        StackBehindNoPane,
        // A card is listed behind two panes.
        InTwoStacks,
        // A card behind a shown window waits for another display than the
        // one showing it.
        StackSplitAcrossDisplays,
        // The cards of one Stack in Spread's row wait for different displays.
        RowStackSplitAcrossDisplays,
        // A Solo display holding its own cards shows more than one. One
        // keeping ordinary windows composes a layout of any size.
        SoloShowsTwo,
    };
    quintptr window = 0;
    Rule rule = Rule::CardAndPane;
    QString detail;

    bool operator==(const CardBookMismatch &other) const = default;
};

[[nodiscard]] inline QString cardBookRuleName(CardBookMismatch::Rule rule)
{
    switch (rule) {
    case CardBookMismatch::Rule::CardAndPane: return QStringLiteral("held twice at once");
    case CardBookMismatch::Rule::PaneOffItsDisplay: return QStringLiteral("shown off the display showing it");
    case CardBookMismatch::Rule::WaitsForNoCardDisplay: return QStringLiteral("waits for a display that holds no cards");
    case CardBookMismatch::Rule::PaneWaits: return QStringLiteral("shown and also waiting");
    case CardBookMismatch::Rule::StackMemberIsPane: return QStringLiteral("shown, in another shown window's Stack");
    case CardBookMismatch::Rule::StackBehindNoPane: return QStringLiteral("a Stack behind a window no display shows");
    case CardBookMismatch::Rule::InTwoStacks: return QStringLiteral("in two Stacks");
    case CardBookMismatch::Rule::StackSplitAcrossDisplays: return QStringLiteral("waits apart from its Stack's pane");
    case CardBookMismatch::Rule::RowStackSplitAcrossDisplays: return QStringLiteral("waits apart from its Stack");
    case CardBookMismatch::Rule::SoloShowsTwo: return QStringLiteral("a Solo display showing two cards");
    }
    return {};
}

[[nodiscard]] inline std::vector<CardBookMismatch> checkCardBook(
    const QList<CardBookDisplay> &displays, const std::vector<CardBookEntry> &entries)
{
    std::vector<CardBookMismatch> found;
    const auto note = [&](quintptr window, CardBookMismatch::Rule rule, const QString &detail) {
        found.push_back({window, rule, detail});
    };
    QHash<QString, CardBookDisplay> byName;
    for (const auto &display : displays) byName.insert(display.name, display);
    QHash<quintptr, const CardBookEntry *> byWindow;
    for (const auto &entry : entries) byWindow.insert(entry.window, &entry);

    // The display showing a window: its layout's, or its own stage's.
    const auto shownOn = [](const CardBookEntry &entry) {
        return entry.paneOf.isEmpty() ? entry.stage : entry.paneOf;
    };
    QHash<QString, int> shownPanes;
    QHash<quintptr, QSet<QString>> rowStackHomes;
    for (const auto &entry : entries) {
        const QString shows = shownOn(entry);
        const bool pane = !shows.isEmpty();
        const int holders = int(entry.card) + int(!entry.paneOf.isEmpty()) + int(!entry.stage.isEmpty());
        if (holders > 1) note(entry.window, CardBookMismatch::Rule::CardAndPane, shows);
        if (pane && entry.screen != shows)
            note(entry.window, CardBookMismatch::Rule::PaneOffItsDisplay,
                 shows + QStringLiteral(" holds it on ") + entry.screen);
        if (pane && !entry.minimized) ++shownPanes[shows];
        if (!entry.home.isEmpty()) {
            const auto display = byName.constFind(entry.home);
            if (display != byName.cend() && (display->spread || !display->holdsCards))
                note(entry.window, CardBookMismatch::Rule::WaitsForNoCardDisplay, entry.home);
            if (pane) note(entry.window, CardBookMismatch::Rule::PaneWaits, entry.home);
        }
        if (entry.behindPanes.size() > 1)
            note(entry.window, CardBookMismatch::Rule::InTwoStacks,
                 QString::number(entry.behindPanes.size()) + QStringLiteral(" panes"));
        for (const auto holder : entry.behindPanes) {
            if (pane) note(entry.window, CardBookMismatch::Rule::StackMemberIsPane, QString());
            const auto front = byWindow.constFind(holder);
            const QString frontShows = front == byWindow.cend() ? QString() : shownOn(**front);
            if (frontShows.isEmpty()) {
                note(holder, CardBookMismatch::Rule::StackBehindNoPane, QString());
                continue;
            }
            // Behind a pane on Spread's display a card waits for none; behind
            // one on another display, for that display.
            const auto display = byName.constFind(frontShows);
            const QString expected = display == byName.cend() || display->spread
                ? QString() : frontShows;
            if (entry.home != expected)
                note(entry.window, CardBookMismatch::Rule::StackSplitAcrossDisplays,
                     QStringLiteral("waits for '%1', its pane is on %2").arg(entry.home, frontShows));
        }
        if (entry.card && entry.stackFace) rowStackHomes[entry.stackFace].insert(entry.home);
    }
    for (auto it = rowStackHomes.cbegin(); it != rowStackHomes.cend(); ++it)
        if (it.value().size() > 1)
            note(it.key(), CardBookMismatch::Rule::RowStackSplitAcrossDisplays,
                 QString::number(it.value().size()) + QStringLiteral(" displays"));
    for (auto it = shownPanes.cbegin(); it != shownPanes.cend(); ++it) {
        const auto display = byName.constFind(it.key());
        if (display != byName.cend() && display->solo && display->holdsCards && !display->spread
            && it.value() > 1)
            note(0, CardBookMismatch::Rule::SoloShowsTwo,
                 it.key() + QStringLiteral(" shows ") + QString::number(it.value()));
    }
    return found;
}

} // namespace Kadunce
