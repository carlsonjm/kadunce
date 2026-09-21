#pragma once

#include "SpreadModel.h"
#include <QStringList>
#include <QList>

namespace Kadunce {
// Value-only boundary: no KWin pointers, render geometry, timers or mutations.
// Indices remain compatibility metadata; only window-lifetime UUIDs identify
// windows across snapshots. This is not persistence across effect unload.
struct WorkspaceCard {
    QString windowId;
    int cardIndex = 0;
    // The card's place in the row. cardIndex beside it is stable
    // compatibility metadata and does not move when the order does.
    int rowPosition = 0;
    QString stackId;
    int stackPosition = 0;
    int stackSize = 0;
    bool selected = false;
};

struct CardWorkspaceSnapshot {
    QList<WorkspaceCard> cards;
    QString selectedCardId;
    QStringList selectedStack;

    const WorkspaceCard *find(const QString &windowId) const
    {
        if (windowId.isEmpty()) return nullptr;
        for (const auto &card : cards)
            if (card.windowId == windowId) return &card;
        return nullptr;
    }
};

inline CardWorkspaceSnapshot makeCardWorkspaceSnapshot(
    const SpreadModel &model, const QStringList &identities, bool active)
{
    CardWorkspaceSnapshot result;
    const auto identity = [&identities](int id) { return identities.value(id - 1); };
    for (int index = 0; index < identities.size(); ++index) {
        const int id = index + 1;
        const auto members = model.stackMembersForId(id);
        result.cards.append({identities[index], id,
            model.rowPositionForId(id),
            members.empty() ? QString() : identity(members.front()),
            model.stackPositionForId(id) + 1, model.stackSizeForId(id),
            model.selectedId() == id});
    }
    if (active && !identities.isEmpty()) {
        result.selectedCardId = identity(model.selectedId());
        for (int member : model.stackMembersForId(model.selectedId()))
            result.selectedStack.append(identity(member));
    }
    return result;
}
} // namespace Kadunce
