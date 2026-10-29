/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <KLocalizedString>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

#include "CardLabelPresentation.h"

namespace Kadunce
{

// One Spread entry as a screen reader hears it.
struct SpreadReadingEntry {
    QString application;
    QString title;
    // Cards in the entry's Stack, its face included; 1 for a card alone.
    int stacked = 1;
    bool sleeping = false;
    // The applications of the panes sharing this card's layout, in the
    // layout's order; empty for a card in no layout.
    QStringList pairedWith;
    // A Stack's cards waiting behind its face, nearest first, as many as
    // Spread shows. Said in place of the panes beside it.
    QStringList behind;
};

// What Spread says for an entry, as Table names its cards: the application
// and the window's title, the panes it shares a layout with, then whether it
// is a Stack and how many cards it holds, and whether the card is sleeping.
// A Stack names the cards behind its face instead of the panes beside it.
inline QString spreadEntryName(const SpreadReadingEntry &entry)
{
    QStringList parts;
    if (!entry.application.isEmpty()) parts.append(entry.application);
    if (!entry.title.isEmpty() && entry.title != entry.application) parts.append(entry.title);
    const QString behind = namesInLine(entry.behind);
    const QString partners = namesInLine(entry.pairedWith);
    if (!behind.isEmpty()) parts.append(i18n("then %1", behind));
    else if (!partners.isEmpty()) parts.append(i18n("with %1", partners));
    if (entry.stacked > 1) parts.append(i18n("Stack of %1 cards", entry.stacked));
    if (entry.sleeping) parts.append(i18n("sleeping"));
    if (parts.isEmpty()) parts.append(i18n("Card"));
    return parts.join(QStringLiteral(", "));
}

// Where an entry stands in the row, counted from the row's first entry.
inline QString spreadEntryPlace(int index, int count)
{
    return i18n("%1 of %2", index + 1, count);
}

// What a screen reader says for Spread's chosen entry: its name and where it
// stands in the row, or nothing when the choice is outside the row.
inline QString spreadLine(const QList<SpreadReadingEntry> &entries, int selected)
{
    if (selected < 0 || selected >= entries.size()) return QString();
    return i18n("%1, %2", spreadEntryName(entries.at(selected)), spreadEntryPlace(selected, entries.size()));
}

} // namespace Kadunce
