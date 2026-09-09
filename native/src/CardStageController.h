/*
    SPDX-FileCopyrightText: 2026 Warbler Studio contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "CardLineModel.h"

#include <effect/effectwindow.h>

#include <QElapsedTimer>
#include <QList>
#include <QPointer>
#include <QStringList>

namespace KWin
{
class LogicalOutput;
}

namespace Kadunce
{

enum class CardPresentation {
    CardLine,
    Active,
};

// Product-wide operations stay explicit. Card Stage owns the logical card
// transaction; its host owns output discovery, effect redirection, shortcuts,
// desktop-stage acceptance.
class CardStageHost
{
public:
    virtual ~CardStageHost() = default;

    [[nodiscard]] virtual KWin::LogicalOutput *tabletOutputForCardStage()
        const = 0;
    [[nodiscard]] virtual bool isTabletOutputForCardStage(
        const KWin::LogicalOutput *output) const = 0;
    [[nodiscard]] virtual bool isManagedWindowForCardStage(
        const KWin::EffectWindow *window) const = 0;
    virtual void setPagingShortcutsForCardStage(bool active) = 0;
    virtual void connectManagedWindowForCardStage(
        KWin::EffectWindow *window) = 0;
    virtual void unredirectForCardStage(KWin::EffectWindow *window) = 0;
    [[nodiscard]] virtual bool admitCardToDesktopStage(
        KWin::EffectWindow *window, KWin::LogicalOutput *output,
        const KWin::RectF &geometry) = 0;
};

class CardStageController final
{
public:
    explicit CardStageController(CardStageHost *host);

    [[nodiscard]] bool isActive() const;
    [[nodiscard]] CardPresentation presentation() const;
    [[nodiscard]] const CardLineModel &model() const;
    [[nodiscard]] const QList<QPointer<KWin::EffectWindow>> &liveCards() const;
    [[nodiscard]] KWin::EffectWindow *selectedWindow() const;
    [[nodiscard]] int liveCardIndex(const KWin::EffectWindow *window) const;
    [[nodiscard]] int visibleSlot(const KWin::EffectWindow *window) const;

    [[nodiscard]] bool cardGrabActive() const;
    [[nodiscard]] double cardGrabOffset() const;
    [[nodiscard]] int cardGrabPageOffset() const;
    [[nodiscard]] int stackPreviewTarget() const;
    [[nodiscard]] bool stackPreviewArmed() const;
    [[nodiscard]] int stackInsertionIndex() const;
    [[nodiscard]] int previousStackInsertionIndex() const;
    [[nodiscard]] int stackBrowseTarget() const;
    [[nodiscard]] double stackInsertionBlend() const;
    [[nodiscard]] double stackPreviewBlend() const;
    [[nodiscard]] bool animationsRunning() const;

    [[nodiscard]] KWin::Rect cardTargetForSlot(
        KWin::LogicalOutput *output, int slot) const;
    [[nodiscard]] KWin::Rect activeTarget(KWin::LogicalOutput *output) const;
    [[nodiscard]] bool selectedStackContains(const QPointF &position) const;
    [[nodiscard]] int activeSideForPoint(const QPointF &position) const;
    [[nodiscard]] QStringList hudState() const;

    void toggle();
    void release();
    void pageHorizontal(int delta);
    void pageStack(int delta);

    void beginCardGrab();
    void updateCardGrab(double horizontalDelta);
    void updateCardGrabDestination(const QPointF &position);
    void pageCardGrab(int direction);
    void finishCardGrab(bool commit);
    [[nodiscard]] bool finishCardGrabOnOutput(const QPointF &position);
    [[nodiscard]] int cardStackCandidate() const;
    void setCardStackPreview(int destinationId);
    void clearCardStackPreview();
    [[nodiscard]] bool pageCardStackInsertion(int direction);

    void syncSelectedElevation();
    void handleWindowActivated(KWin::EffectWindow *window);
    void admitTransferredWindowToTablet(KWin::EffectWindow *window);
    [[nodiscard]] bool handleWindowAdded(KWin::EffectWindow *window);
    void handleWindowClosed(KWin::EffectWindow *window);
    void handleActiveGeometryChanged(KWin::EffectWindow *window);

private:
    struct ActiveRestoreSnapshot {
        QPointer<KWin::EffectWindow> window;
        KWin::RectF geometry;
        KWin::QuickTileMode quickTileMode;
        KWin::MaximizeMode maximizeMode = KWin::MaximizeRestore;
        bool fullScreen = false;
        bool valid = false;
    };

    void rebuildLiveCards();
    bool enterActive();
    void restoreActiveSnapshot();
    void resetCardGrabState(KWin::EffectWindow *grabbed, bool stacked);
    void syncSelectedStackingOrder();
    void restoreOriginalStackingOrder();

    CardStageHost *m_host;
    CardLineModel m_cardLine{1};
    QList<QPointer<KWin::EffectWindow>> m_liveCards;
    QList<QPointer<KWin::EffectWindow>> m_originalCardStackingOrder;
    ActiveRestoreSnapshot m_activeRestore;
    CardPresentation m_presentation = CardPresentation::CardLine;
    double m_cardGrabOffset = 0.0;
    int m_cardGrabPageOffset = 0;
    int m_cardStackPreviewTarget = 0;
    int m_cardStackInsertionIndex = -1;
    int m_cardStackPreviousInsertionIndex = -1;
    double m_cardStackPreviewFrom = 0.0;
    double m_cardStackPreviewTo = 0.0;
    QElapsedTimer m_cardStackPreviewTimer;
    QElapsedTimer m_cardStackInsertionTimer;
    QPointF m_cardGrabPointer;
    QString m_cardGrabDestinationOutput;
    bool m_cardGrabMoved = false;
    bool m_cardStackPreviewArmed = false;
    bool m_cardGrabActive = false;
    bool m_active = false;
};

} // namespace Kadunce
