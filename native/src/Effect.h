/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "CardStageController.h"
#include "DesktopStageController.h"
#include "WorkspaceInputRouter.h"

#include <effect/offscreeneffect.h>

#include <QList>
#include <QPointer>
#include <QStringList>

#include <memory>

class QAction;

namespace Kadunce
{

class Effect final : public KWin::OffscreenEffect,
                     private WorkspaceInputTarget,
                     private DesktopStageHost,
                     private CardStageHost
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "studio.warbler.Kadunce")

public:
    Effect();
    ~Effect() override;

    static bool supported();

    void prePaintScreen(KWin::ScreenPrePaintData &data) override;
    void prePaintWindow(KWin::RenderView *view,
                        KWin::EffectWindow *window,
                        KWin::WindowPrePaintData &data) override;
    void paintScreen(const KWin::RenderTarget &renderTarget,
                     const KWin::RenderViewport &viewport,
                     int mask,
                     const KWin::Region &deviceRegion,
                     KWin::LogicalOutput *screen) override;
    void paintWindow(const KWin::RenderTarget &renderTarget,
                     const KWin::RenderViewport &viewport,
                     KWin::EffectWindow *window,
                     int mask,
                     const KWin::Region &deviceRegion,
                     KWin::WindowPaintData &data) override;
    void drawWindow(const KWin::RenderTarget &renderTarget,
                    const KWin::RenderViewport &viewport,
                    KWin::EffectWindow *window,
                    int mask,
                    const KWin::Region &deviceRegion,
                    KWin::WindowPaintData &data) override;

    [[nodiscard]] int requestedEffectChainPosition() const override
    {
        return 50;
    }

    [[nodiscard]] bool blocksDirectScanout() const override
    {
        return (m_cardStage && m_cardStage->isActive())
            || hasActiveDesktopStage();
    }

    [[nodiscard]] bool isActive() const override
    {
        return (m_cardStage && m_cardStage->isActive())
            || hasActiveDesktopStage();
    }

private Q_SLOTS:
    void toggle();
    void release();
    void pageLeft();
    void pageRight();
    void pageStackUp();
    void pageStackDown();
    void toggleBento();

public Q_SLOTS:
    Q_SCRIPTABLE void showCardLine();
    Q_SCRIPTABLE void showActive();
    Q_SCRIPTABLE QStringList outputStageState() const;
    Q_SCRIPTABLE QString workspaceContext() const;
    Q_SCRIPTABLE bool activateApplicationWindow(const QString &windowId);
    Q_SCRIPTABLE bool toggleBentoOnOutput(const QString &outputName);
    Q_SCRIPTABLE bool handoffBentoLeadToOutput(
        const QString &sourceName, const QString &destinationName);

private:
    static bool isTabletOutput(const KWin::LogicalOutput *output);
    static bool isCardWindow(const KWin::EffectWindow *window);
    static bool isApplicationWindow(const KWin::EffectWindow *window);
    KWin::LogicalOutput *tabletOutput() const;
    [[nodiscard]] bool isTabletOutputForDesktopStage(
        const KWin::LogicalOutput *output) const override;
    [[nodiscard]] bool allowsDesktopStageOnOutput(
        const KWin::LogicalOutput *output) const override;
    [[nodiscard]] bool isManagedWindowForDesktopStage(
        const KWin::EffectWindow *window) const override;
    [[nodiscard]] KWin::LogicalOutput *tabletOutputForDesktopStage()
        const override;
    [[nodiscard]] KWin::Rect activeTargetForDesktopStage(
        KWin::LogicalOutput *output) const override;
    void prepareOutputForDesktopStage(
        KWin::LogicalOutput *output) override;
    [[nodiscard]] KWin::LogicalOutput *tabletOutputForCardStage()
        const override;
    [[nodiscard]] bool isTabletOutputForCardStage(
        const KWin::LogicalOutput *output) const override;
    [[nodiscard]] bool isManagedWindowForCardStage(
        const KWin::EffectWindow *window) const override;
    void setPagingShortcutsForCardStage(bool active) override;
    void connectManagedWindowForCardStage(
        KWin::EffectWindow *window) override;
    void unredirectForCardStage(KWin::EffectWindow *window) override;
    [[nodiscard]] bool admitCardToDesktopStage(
        KWin::EffectWindow *window, KWin::LogicalOutput *output,
        const KWin::RectF &geometry) override;
    void setPagingShortcutsActive(bool active);
    [[nodiscard]] WorkspacePresentation presentationForInput() const override;
    [[nodiscard]] WorkspaceInputGeometry geometryForInput() const override;
    [[nodiscard]] bool cardGrabActiveForInput() const override;
    [[nodiscard]] bool stackPreviewArmedForInput() const override;
    [[nodiscard]] int stackPreviewTargetForInput() const override;
    [[nodiscard]] bool centerCardContainsForInput(
        const QPointF &position) const override;
    [[nodiscard]] bool isTabletPoint(
        const QPointF &position) const override;
    [[nodiscard]] int activeSideForPoint(
        const QPointF &position) const override;
    [[nodiscard]] bool selectedStackContains(
        const QPointF &position) const override;
    void toggleFromInput() override;
    void pageLeftFromInput() override;
    void pageRightFromInput() override;
    void pageStackFromInput(int delta) override;
    void pageHorizontal(int delta);
    void pageStack(int delta);
    void beginCardGrab() override;
    void updateCardGrab(double horizontalDelta) override;
    void updateCardGrabDestination(const QPointF &position) override;
    void pageCardGrab(int direction) override;
    void finishCardGrab(bool commit) override;
    [[nodiscard]] bool finishCardGrabOnOutput(
        const QPointF &position) override;
    [[nodiscard]] int cardStackCandidate() const override;
    void setCardStackPreview(int destinationId) override;
    void clearCardStackPreview() override;
    [[nodiscard]] bool pageCardStackInsertion(int direction) override;
    [[nodiscard]] int cardStackBrowseTarget() const;
    [[nodiscard]] double cardStackInsertionBlend() const;
    [[nodiscard]] double cardStackPreviewBlend() const;
    void syncSelectedElevation();
    void activateSelectedFromInput() override;
    [[nodiscard]] KWin::EffectWindow *selectedWindow() const;
    void handleWindowAdded(KWin::EffectWindow *window);
    void handleWindowClosed(KWin::EffectWindow *window);
    void handleWindowActivated(KWin::EffectWindow *window);
    void handleActiveGeometryChanged(KWin::EffectWindow *window,
                                     const KWin::RectF &oldGeometry);
    void handleWindowMoveResizeStarted(KWin::EffectWindow *window);
    void handleWindowMoveResizeStepped(KWin::EffectWindow *window,
                                       const KWin::RectF &geometry);
    void handleWindowMoveResizeFinished(KWin::EffectWindow *window);
    void handleScreenRemoved(KWin::LogicalOutput *output);
    void handleSessionStateChanged();
    [[nodiscard]] int liveCardIndex(const KWin::EffectWindow *window) const;
    [[nodiscard]] int visibleSlot(const KWin::EffectWindow *window) const;
    [[nodiscard]] KWin::Rect cardTargetForSlot(
        KWin::LogicalOutput *output, int slot) const;
    [[nodiscard]] KWin::Rect activeTarget(KWin::LogicalOutput *output) const;
    [[nodiscard]] bool hasActiveDesktopStage() const;
    void connectManagedWindow(KWin::EffectWindow *window);
    void admitTransferredWindowToTablet(
        KWin::EffectWindow *window) override;

    QAction *m_toggleAction = nullptr;
    QAction *m_releaseAction = nullptr;
    QAction *m_previousAction = nullptr;
    QAction *m_nextAction = nullptr;
    QAction *m_stackPreviousAction = nullptr;
    QAction *m_stackNextAction = nullptr;
    QAction *m_bentoAction = nullptr;
    QAction *m_showCardLineAction = nullptr;
    QAction *m_showActiveAction = nullptr;
    bool m_usesDirectSystemEdges = true;
    std::unique_ptr<WorkspaceInputRouter> m_inputRouter;
    std::unique_ptr<DesktopStageController> m_desktopStage;
    std::unique_ptr<CardStageController> m_cardStage;
    KWin::LogicalOutput *m_paintingOutput = nullptr;
    std::unique_ptr<KWin::GLShader> m_fanApertureShader;
    KWin::EffectWindow *m_fanApertureWindow = nullptr;
    QSizeF m_fanPaintSize;
    QPointF m_fanApertureOrigin;
    QSizeF m_fanApertureSize;
    float m_fanApertureRadius = 0.0F;
    int m_fanPaintSizeLocation = -1;
    int m_fanApertureOriginLocation = -1;
    int m_fanApertureSizeLocation = -1;
    int m_fanApertureRadiusLocation = -1;
};

} // namespace Kadunce
