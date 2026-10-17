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
        // A card is held by the card stage and by a layout at once.
        CardAndPane,
        // A layout holds a pane that stands on another display.
        PaneOffItsDisplay,
        // A card waits for Spread's display, or one that keeps no cards. A
        // display that is gone is not one: its cards wait for it to return.
        WaitsForNoCardDisplay,
        // A pane is also said to wait for a display.
        PaneWaits,
        // A Stack lists a pane behind another pane. A window that is not a
        // card yet may be listed, as one arriving is.
        StackMemberIsPane,
        // A Stack is kept behind a window that is no pane.
        StackBehindNoPane,
        // A card is listed behind two panes.
        InTwoStacks,
        // A card behind a pane waits for another display than the pane's.
        StackSplitAcrossDisplays,
        // The cards of one Stack in Spread's row wait for different displays.
        RowStackSplitAcrossDisplays,
        // A Solo display shows more than one card.
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
    case CardBookMismatch::Rule::CardAndPane: return QStringLiteral("a card and a pane at once");
    case CardBookMismatch::Rule::PaneOffItsDisplay: return QStringLiteral("a pane off its layout's display");
    case CardBookMismatch::Rule::WaitsForNoCardDisplay: return QStringLiteral("waits for a display that holds no cards");
    case CardBookMismatch::Rule::PaneWaits: return QStringLiteral("a pane that also waits");
    case CardBookMismatch::Rule::StackMemberIsPane: return QStringLiteral("a pane in another pane's Stack");
    case CardBookMismatch::Rule::StackBehindNoPane: return QStringLiteral("a Stack behind a window that is no pane");
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

    QHash<QString, int> shownPanes;
    QHash<quintptr, QSet<QString>> rowStackHomes;
    for (const auto &entry : entries) {
        const bool pane = !entry.paneOf.isEmpty();
        if (entry.card && pane) note(entry.window, CardBookMismatch::Rule::CardAndPane, entry.paneOf);
        if (pane && entry.screen != entry.paneOf)
            note(entry.window, CardBookMismatch::Rule::PaneOffItsDisplay,
                 entry.paneOf + QStringLiteral(" holds it on ") + entry.screen);
        if (pane && !entry.minimized) ++shownPanes[entry.paneOf];
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
            if (front == byWindow.cend() || (*front)->paneOf.isEmpty()) {
                note(holder, CardBookMismatch::Rule::StackBehindNoPane, QString());
                continue;
            }
            // Behind a pane on Spread's display a card waits for none; behind
            // one on another display, for that display.
            const auto display = byName.constFind((*front)->paneOf);
            const QString expected = display == byName.cend() || display->spread
                ? QString() : (*front)->paneOf;
            if (entry.home != expected)
                note(entry.window, CardBookMismatch::Rule::StackSplitAcrossDisplays,
                     QStringLiteral("waits for '%1', its pane is on %2").arg(entry.home, (*front)->paneOf));
        }
        if (entry.card && entry.stackFace) rowStackHomes[entry.stackFace].insert(entry.home);
    }
    for (auto it = rowStackHomes.cbegin(); it != rowStackHomes.cend(); ++it)
        if (it.value().size() > 1)
            note(it.key(), CardBookMismatch::Rule::RowStackSplitAcrossDisplays,
                 QString::number(it.value().size()) + QStringLiteral(" displays"));
    for (auto it = shownPanes.cbegin(); it != shownPanes.cend(); ++it) {
        const auto display = byName.constFind(it.key());
        if (display != byName.cend() && display->solo && !display->spread && it.value() > 1)
            note(0, CardBookMismatch::Rule::SoloShowsTwo,
                 it.key() + QStringLiteral(" shows ") + QString::number(it.value()));
    }
    return found;
}

} // namespace Kadunce
