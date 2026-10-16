/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

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
};

// What Spread says for an entry, as Table names its cards: the application
// and the window's title, the panes it shares a layout with, then whether it
// is a Stack and how many cards it holds, and whether the card is sleeping.
inline QString spreadEntryName(const SpreadReadingEntry &entry)
{
    QStringList parts;
    if (!entry.application.isEmpty()) parts.append(entry.application);
    if (!entry.title.isEmpty() && entry.title != entry.application) parts.append(entry.title);
    QStringList partners;
    for (const QString &application : entry.pairedWith)
        if (!application.isEmpty()) partners.append(application);
    if (partners.size() == 1) {
        parts.append(QObject::tr("paired with %1").arg(partners.first()));
    } else if (!partners.isEmpty()) {
        const QString last = partners.takeLast();
        parts.append(QObject::tr("paired with %1 and %2").arg(partners.join(QStringLiteral(", ")), last));
    }
    if (entry.stacked > 1) parts.append(QObject::tr("Stack of %1 cards").arg(entry.stacked));
    if (entry.sleeping) parts.append(QObject::tr("sleeping"));
    if (parts.isEmpty()) parts.append(QObject::tr("Card"));
    return parts.join(QStringLiteral(", "));
}

// Where an entry stands in the row, counted from the row's first entry.
inline QString spreadEntryPlace(int index, int count)
{
    return QObject::tr("%1 of %2").arg(index + 1).arg(count);
}

// What a screen reader says for Spread's chosen entry: its name and where it
// stands in the row, or nothing when the choice is outside the row.
inline QString spreadLine(const QList<SpreadReadingEntry> &entries, int selected)
{
    if (selected < 0 || selected >= entries.size()) return QString();
    return QObject::tr("%1, %2").arg(spreadEntryName(entries.at(selected)), spreadEntryPlace(selected, entries.size()));
}

} // namespace Kadunce
