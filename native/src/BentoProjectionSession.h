/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "BentoSidePlacement.h"
#include "NativeMoveTakeover.h"

#include <effect/effectwindow.h>

#include <QList>
#include <QPointer>
#include <QString>
#include <QSet>

#include <optional>
#include <vector>

namespace Kadunce
{

struct BentoProjectionMember {
    QPointer<KWin::EffectWindow> window;
    NativeMoveSnapshot restore;
    bool minimized = false;
};

struct BentoProjectionSession {
    QPointer<KWin::LogicalOutput> output;
    QString outputName;
    KWin::Rect workspaceArea;
    QList<BentoProjectionMember> panes;
    QList<BentoProjectionMember> overflow;
    QList<QPointer<KWin::EffectWindow>> stackingOrder;
    std::vector<BentoRect> rects;
    QPointer<KWin::EffectWindow> lead;
    QPointer<KWin::EffectWindow> sideWindow;
    std::optional<BentoSidePlacement> side;
};

struct BentoProjectionShape {
    KWin::Rect workspaceArea;
    std::vector<quintptr> panes;
    std::vector<quintptr> overflow;
    std::vector<quintptr> stackingOrder;
    std::vector<BentoRect> rects;
    quintptr lead = 0;
    quintptr sideWindow = 0;
    bool hasSide = false;
};

[[nodiscard]] inline bool validBentoProjectionShape(
    const BentoProjectionShape &shape)
{
    if (shape.workspaceArea.width() <= 0 || shape.workspaceArea.height() <= 0
        || shape.panes.empty() || shape.panes.size() != shape.rects.size()
        || shape.lead == 0) {
        return false;
    }
    QSet<quintptr> identities;
    for (std::size_t index = 0; index < shape.panes.size(); ++index) {
        const auto identity = shape.panes[index];
        const auto &rect = shape.rects[index];
        if (identity == 0 || identities.contains(identity)
            || rect.width <= 0.0 || rect.height <= 0.0
            || rect.x < 0.0 || rect.y < 0.0
            || rect.x + rect.width > 1.0001
            || rect.y + rect.height > 1.0001) {
            return false;
        }
        identities.insert(identity);
    }
    if (!identities.contains(shape.lead)) {
        return false;
    }
    for (const auto identity : shape.overflow) {
        if (identity == 0 || identities.contains(identity)) {
            return false;
        }
        identities.insert(identity);
    }
    QSet<quintptr> stacked;
    for (const auto identity : shape.stackingOrder) {
        if (identity == 0 || !identities.contains(identity)
            || stacked.contains(identity)) return false;
        stacked.insert(identity);
    }
    if (stacked != identities) return false;
    return !shape.hasSide
        || (shape.sideWindow != 0 && identities.contains(shape.sideWindow));
}

[[nodiscard]] inline BentoProjectionShape projectionShape(
    const BentoProjectionSession &session)
{
    BentoProjectionShape shape;
    shape.workspaceArea = session.workspaceArea;
    shape.rects = session.rects;
    shape.lead = reinterpret_cast<quintptr>(session.lead.data());
    shape.sideWindow = reinterpret_cast<quintptr>(session.sideWindow.data());
    shape.hasSide = session.side.has_value();
    for (const auto &member : session.panes) {
        shape.panes.push_back(reinterpret_cast<quintptr>(member.window.data()));
    }
    for (const auto &member : session.overflow) {
        shape.overflow.push_back(reinterpret_cast<quintptr>(member.window.data()));
    }
    for (const auto &window : session.stackingOrder) {
        shape.stackingOrder.push_back(reinterpret_cast<quintptr>(window.data()));
    }
    return shape;
}

} // namespace Kadunce
