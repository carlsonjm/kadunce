/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QHash>
#include <QString>

#include <vector>

namespace Kadunce
{

// CARD-LIFECYCLE.md §11, Every display: a card's status is one fact, wherever
// it stands. Today that fact is spread over each display's card stage, its row
// and its Stacks behind panes, and each display's layout, on every desktop.
// Each move keeps them in step by hand, and a move that forgets one leaves a
// card whose status depends on which record is read. The card book reads them
// all at once, as one entry per window, and names every place they disagree.
//
// It reports and never repairs, as the ownership ledger does: a mismatch is a
// defect in the move that made it, kept visible so the move can be found.

// A Stack that lists a card behind a pane, and the display whose card stage
// keeps that Stack.
struct CardBookStackPlace {
    quintptr pane = 0;
    QString keeper;
};

struct CardBookEntry {
    quintptr window = 0;
    // The display the window stands on.
    QString screen;
    // Held by a card stage, in that display's Spread row.
    bool card = false;
    // The display whose stage holds it as a card, and how many stages do, on
    // every desktop.
    QString cardStage;
    int cardStages = 0;
    // The display whose layout holds it as a pane, and how many layouts do;
    // empty and none when no layout does.
    QString paneOf;
    int panes = 0;
    // Every Stack that lists it behind a pane.
    std::vector<CardBookStackPlace> behindPanes;
};

struct CardBookMismatch {
    enum class Rule {
        // A window is held twice: as a card and a pane, or by two layouts.
        CardAndPane,
        // A layout shows a window that stands on another display.
        PaneOffItsDisplay,
        // A Stack lists a window a layout shows behind another. A window
        // that is not a card yet may be listed, as one arriving is.
        StackMemberIsPane,
        // A Stack is kept behind a window no layout shows.
        StackBehindNoPane,
        // A card is listed behind two panes.
        InTwoStacks,
        // A card behind a pane is held by another display's stage than the
        // one keeping that Stack, as one left behind by a Stack's move is.
        StackSplitAcrossDisplays,
        // A card is held by the stages of two displays, or two desktops, at once.
        CardOnTwoStages,
        // A card stands on another display than the one whose stage holds it,
        // as one a display that is gone still holds does.
        CardOffItsDisplay,
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
    case CardBookMismatch::Rule::StackMemberIsPane: return QStringLiteral("shown, in another shown window's Stack");
    case CardBookMismatch::Rule::StackBehindNoPane: return QStringLiteral("a Stack behind a window no display shows");
    case CardBookMismatch::Rule::InTwoStacks: return QStringLiteral("in two Stacks");
    case CardBookMismatch::Rule::StackSplitAcrossDisplays: return QStringLiteral("held apart from its Stack");
    case CardBookMismatch::Rule::CardOnTwoStages: return QStringLiteral("a card of two displays");
    case CardBookMismatch::Rule::CardOffItsDisplay: return QStringLiteral("a card off its display");
    }
    return {};
}

[[nodiscard]] inline std::vector<CardBookMismatch> checkCardBook(const std::vector<CardBookEntry> &entries)
{
    std::vector<CardBookMismatch> found;
    const auto note = [&](quintptr window, CardBookMismatch::Rule rule, const QString &detail) {
        found.push_back({window, rule, detail});
    };
    QHash<quintptr, const CardBookEntry *> byWindow;
    for (const auto &entry : entries) byWindow.insert(entry.window, &entry);

    for (const auto &entry : entries) {
        const bool pane = entry.panes > 0;
        if (int(entry.card) + entry.panes > 1)
            note(entry.window, CardBookMismatch::Rule::CardAndPane,
                 entry.card ? entry.paneOf : QString::number(entry.panes) + QStringLiteral(" layouts"));
        if (entry.cardStages > 1)
            note(entry.window, CardBookMismatch::Rule::CardOnTwoStages,
                 QString::number(entry.cardStages) + QStringLiteral(" stages"));
        if (!entry.cardStage.isEmpty() && entry.screen != entry.cardStage)
            note(entry.window, CardBookMismatch::Rule::CardOffItsDisplay,
                 entry.cardStage + QStringLiteral(" holds it on ") + entry.screen);
        if (pane && entry.screen != entry.paneOf)
            note(entry.window, CardBookMismatch::Rule::PaneOffItsDisplay,
                 entry.paneOf + QStringLiteral(" holds it on ") + entry.screen);
        if (entry.behindPanes.size() > 1)
            note(entry.window, CardBookMismatch::Rule::InTwoStacks,
                 QString::number(entry.behindPanes.size()) + QStringLiteral(" panes"));
        for (const auto &place : entry.behindPanes) {
            if (pane) note(entry.window, CardBookMismatch::Rule::StackMemberIsPane, QString());
            const auto front = byWindow.constFind(place.pane);
            if (front == byWindow.cend() || (*front)->panes == 0) {
                note(place.pane, CardBookMismatch::Rule::StackBehindNoPane, QString());
                continue;
            }
            if (entry.card && entry.cardStage != place.keeper)
                note(entry.window, CardBookMismatch::Rule::StackSplitAcrossDisplays,
                     QStringLiteral("held by %1, its Stack is kept by %2").arg(entry.cardStage, place.keeper));
        }
    }
    return found;
}

} // namespace Kadunce
