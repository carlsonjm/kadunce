/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
// What the Effect's source files share: each desktop's and display's stages,
// the hosts those stages call back through, and the helpers more than one of the
// files needs. Not for the stages.
#include "Effect.h"
#include "EffectShared.h"
#include "LogName.h"
#include "MotionTime.h"
#include <QScopeGuard>
#include <fstream>
#include <string>
#include "LaunchIdentity.h"
#include "PendingLaunch.h"
#include "PlacementAim.h"
#include "SpreadLayout.h"
#include "DisplayHandoffPolicy.h"
#include "CarryPaintPlan.h"
#include "PaneArrival.h"
#include "NativeLanding.h"
#include "MonitorDropIntent.h"
#include "DeliberateEdgeEntry.h"
#include "TouchDisplay.h"
#include "DesktopRemoval.h"
#include "StuckNotesWatcher.h"
#include "SurfaceTone.h"
#include "KWinMatch.h"
#include "DisplayBounds.h"
#include <QCoreApplication>
#include <cursor.h>
#include <cursorsource.h>
#include <core/inputdevice.h>

#include <core/output.h>
#include <core/region.h>
#include <core/renderviewport.h>
#include <core/rendertarget.h>
#include <effect/effecthandler.h>
#include <effect/effectwindow.h>
#include <input.h>
#include <pointer_input.h>
#include <touch_input.h>
#include <inputmethod.h>
#include <inputpanelv1window.h>
#include <main.h>
#include <opengl/glshadermanager.h>
#include <opengl/glutils.h>
#include <opengl/glvertexbuffer.h>
#include <scene/item.h>
#include <scene/itemrenderer.h>
#include <scene/scene.h>
#include <scene/workspacescene.h>
#include <window.h>
#include <workspace.h>
#include <virtualdesktops.h>
#include <wayland_server.h>
#include <wayland/seat.h>
#include <wayland/surface.h>
#include <wayland/textinput_v3.h>

#include <KConfigGroup>
#include <KSharedConfig>
#include <KService>
#include <KSharedConfig>

#include <QAction>
#include <QKeyEvent>
#include <QLineF>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusPendingCall>
#include <QDBusServiceWatcher>
#include <QDebug>
#include <QEasingCurve>
#include <QJsonArray>
#include <chrono>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequence>
#include <QLoggingCategory>
#include <QPainterPath>
#include <QRegion>
#include <QRegularExpression>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <vector>

namespace Kadunce
{

// The version this build is, as the journal names it (CMakeLists.txt).
#ifndef KADUNCE_REVISION
#define KADUNCE_REVISION "unversioned"
#endif

constexpr auto Revision = KADUNCE_REVISION;
// Milliseconds on a clock that never steps back.
inline qint64 steadyMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

inline QString windowIdentity(const KWin::EffectWindow *window)
{
    return window
        ? window->internalId().toString(QUuid::WithoutBraces)
        : QString();
}

inline QString applicationIdentity(const KWin::EffectWindow *window)
{
    if (!window) {
        return {};
    }
    if (window->window()) {
        if (!window->window()->desktopFileName().isEmpty()) {
            return window->window()->desktopFileName();
        }
        // Some Electron clients publish their profile path in resourceName.
        // resourceClass is the clean application-level fallback Tettegouche
        // needs for ranking and never exposes that machine-local path.
        if (!window->window()->resourceClass().isEmpty()) {
            return window->window()->resourceClass().trimmed().toLower();
        }
        if (!window->window()->resourceName().isEmpty()) {
            return window->window()->resourceName().trimmed().toLower();
        }
    }
    return window->windowClass().section(QLatin1Char(' '), -1)
        .trimmed().toLower();
}
const QString OverviewEffect = QStringLiteral("overview");
// The display KWin lists under this name; none when it lists none.
inline KWin::LogicalOutput *screenNamed(const QString &name)
{
    if (name.isEmpty()) return nullptr;
    KWin::LogicalOutput *output = KWin::effects->findScreen(name);
    return output && output->name() == name ? output : nullptr;
}

// The cards behind a card on its stage: its Spread Stack, or the Stack behind
// it as a pane.
inline QList<KWin::EffectWindow *> cardsBehind(const CardStageController &stage, KWin::EffectWindow *card)
{
    QList<KWin::EffectWindow *> behind = stage.paneStack(card);
    for (auto *member : stage.stackFrontToBack(card))
        if (member != card && !behind.contains(member)) behind.append(member);
    return behind;
}

// One display's card stage on one desktop, its host declared first so it
// outlives the stage.
struct Effect::DisplayStage {
    std::unique_ptr<StageHost> host;
    std::unique_ptr<CardStageController> cards;
};

// One virtual desktop's ownership session. Its host is declared first so it
// outlives the stages that call it.
struct Effect::DesktopSession {
    QString desktopId;
    QPointer<KWin::VirtualDesktop> desktop;
    std::unique_ptr<SessionHost> host;
    std::unique_ptr<DesktopStageController> layouts;
    // CARD-LIFECYCLE.md §11: each display's own card stage on this desktop,
    // by stage key (Effect::stageKeyFor).
    std::map<QString, std::unique_ptr<DisplayStage>> displays;

    // The stage holding the window as one of its cards, if any does.
    [[nodiscard]] CardStageController *stageHolding(const KWin::EffectWindow *window) const
    {
        if (!window) return nullptr;
        for (const auto &display : displays)
            if (display.second->cards->liveCardIndex(window) >= 0) return display.second->cards.get();
        return nullptr;
    }
    [[nodiscard]] std::optional<QString> keyHolding(const KWin::EffectWindow *window) const
    {
        if (!window) return std::nullopt;
        for (const auto &display : displays)
            if (display.second->cards->liveCardIndex(window) >= 0) return display.first;
        return std::nullopt;
    }
};

// Names one session's stage on one display for the length of a call. Leaving
// hands back the session and display named before, or the current desktop's
// when that was the one named, since a desktop switch inside the call makes
// another desktop current. Without a display, the one named now is kept.
class Effect::SessionScope
{
public:
    SessionScope(Effect *effect, DesktopSession *session, std::optional<QString> key = std::nullopt)
        : m_effect(effect)
        , m_previous(effect->scopedToCurrent() ? nullptr : effect->m_scopedSession)
        , m_previousKey(effect->m_scopedKey)
    {
        ++effect->m_scopeDepth;
        effect->scopeTo(session, key ? *key : effect->m_scopedKey);
    }
    ~SessionScope()
    {
        --m_effect->m_scopeDepth;
        m_effect->scopeTo(m_previous ? m_previous : m_effect->m_currentSession, m_previousKey);
    }
    SessionScope(const SessionScope &) = delete;
    SessionScope &operator=(const SessionScope &) = delete;

private:
    Effect *m_effect;
    DesktopSession *m_previous;
    QString m_previousKey;
};

// Every host call a card stage makes is answered for that stage's own desktop
// and display, so a stage never reads another desktop's windows, or another
// display's, as its own: inside the call, the effect's m_cardStage is this
// stage and tabletOutput() is its display. The effect keeps the answers.
class Effect::StageHost final : public CardStageHost
{
public:
    StageHost(Effect *effect, DesktopSession *session, const QString &key)
        : m_effect(effect)
        , m_session(session)
        , m_key(key)
    {
    }

    KWin::LogicalOutput *tabletOutputForCardStage() const override
    {
        SessionScope scope(m_effect, m_session, m_key);
        return cards().tabletOutputForCardStage();
    }
    bool isTabletOutputForCardStage(const KWin::LogicalOutput *output) const override
    {
        SessionScope scope(m_effect, m_session, m_key);
        return cards().isTabletOutputForCardStage(output);
    }
    bool isManagedWindowForCardStage(const KWin::EffectWindow *window) const override
    {
        SessionScope scope(m_effect, m_session, m_key);
        return cards().isManagedWindowForCardStage(window);
    }
    bool mayHoldWindowForCardStage(const KWin::EffectWindow *window) const override
    {
        SessionScope scope(m_effect, m_session, m_key);
        return cards().mayHoldWindowForCardStage(window);
    }
    bool heldElsewhereForCardStage(const KWin::EffectWindow *window) const override
    {
        return m_effect->sessionHolding(window) != nullptr;
    }
    std::optional<double> inputPanelTopForCardStage(KWin::LogicalOutput *output) const override
    {
        SessionScope scope(m_effect, m_session, m_key);
        return cards().inputPanelTopForCardStage(output);
    }
    bool keyboardTypesIntoForCardStage(const KWin::EffectWindow *window) const override
    {
        SessionScope scope(m_effect, m_session, m_key);
        return cards().keyboardTypesIntoForCardStage(window);
    }
    KWin::RectF workAreaForCardStage(const KWin::LogicalOutput *output) const override
    {
        SessionScope scope(m_effect, m_session, m_key);
        return cards().workAreaForCardStage(output);
    }
    void setPagingShortcutsForCardStage(bool active) override
    {
        SessionScope scope(m_effect, m_session, m_key);
        cards().setPagingShortcutsForCardStage(active);
    }
    void cancelInputForCardStage() override
    {
        SessionScope scope(m_effect, m_session, m_key);
        m_effect->cancelInputOnStageDisplay();
    }
    // A card whose application asked a question as it was flicked closed
    // opens with it. The opening waits a turn of the event loop and then acts
    // only if the shown desktop's stage still has that card selected.
    void presentSelectedForCardStage() override
    {
        SessionScope scope(m_effect, m_session, m_key);
        cards().presentSelectedForCardStage();
    }
    // Each stage hears the gutter change and lays out its own.
    void cardGutterChangedForCardStage() override
    {
        SessionScope scope(m_effect, m_session, m_key);
        cards().cardGutterChangedForCardStage();
    }
    void connectManagedWindowForCardStage(KWin::EffectWindow *window) override
    {
        SessionScope scope(m_effect, m_session, m_key);
        cards().connectManagedWindowForCardStage(window);
    }
    void unredirectForCardStage(KWin::EffectWindow *window) override
    {
        SessionScope scope(m_effect, m_session, m_key);
        cards().unredirectForCardStage(window);
    }
    void cardActivatedBehindLayoutForCardStage(KWin::EffectWindow *window) override
    {
        SessionScope scope(m_effect, m_session, m_key);
        cards().cardActivatedBehindLayoutForCardStage(window);
    }
    bool holdPictureBeforeResizeForCardStage(KWin::EffectWindow *window,
        const std::function<void()> &resize) override
    {
        SessionScope scope(m_effect, m_session, m_key);
        return cards().holdPictureBeforeResizeForCardStage(window, resize);
    }
    void retireBentoProjectionForCardStage(const QList<QPointer<KWin::EffectWindow>> &windows) override
    {
        SessionScope scope(m_effect, m_session, m_key);
        cards().retireBentoProjectionForCardStage(windows);
    }
    bool connectedForCardStage() const override
    {
        SessionScope scope(m_effect, m_session, m_key);
        return cards().connectedForCardStage();
    }
    bool layoutLeavesRoomForCardStage() const override
    {
        SessionScope scope(m_effect, m_session, m_key);
        return cards().layoutLeavesRoomForCardStage();
    }
    bool reopenedAtSignInForCardStage(const KWin::EffectWindow *window) const override
    {
        SessionScope scope(m_effect, m_session, m_key);
        return cards().reopenedAtSignInForCardStage(window);
    }
    bool usedAtSignOutForCardStage(const KWin::EffectWindow *window) const override
    {
        SessionScope scope(m_effect, m_session, m_key);
        return cards().usedAtSignOutForCardStage(window);
    }
    bool holdsOwnCardsForCardStage(const KWin::LogicalOutput *output) const override
    {
        SessionScope scope(m_effect, m_session, m_key);
        return cards().holdsOwnCardsForCardStage(output);
    }
    bool moveCardToDisplayForCardStage(KWin::EffectWindow *card, KWin::LogicalOutput *to) override
    {
        SessionScope scope(m_effect, m_session, m_key);
        return cards().moveCardToDisplayForCardStage(card, to);
    }
    KWin::LogicalOutput *releaseDisplayForCardStage(const KWin::EffectWindow *window) const override
    {
        SessionScope scope(m_effect, m_session, m_key);
        return cards().releaseDisplayForCardStage(window);
    }
    bool admitCardToDesktopStage(KWin::EffectWindow *window, KWin::LogicalOutput *output,
        const KWin::RectF &geometry, const std::function<bool()> &commitSource,
        const std::function<void()> &releaseSource) override
    {
        SessionScope scope(m_effect, m_session, m_key);
        return cards().admitCardToDesktopStage(window, output, geometry, commitSource, releaseSource);
    }
    bool resumeBentoProjectionForCardStage(const BentoProjectionSession &projection,
        const std::function<bool()> &commitSource,
        const std::function<void()> &releaseSource) override
    {
        SessionScope scope(m_effect, m_session, m_key);
        return cards().resumeBentoProjectionForCardStage(projection, commitSource, releaseSource);
    }

private:
    CardStageHost &cards() const { return *m_effect; }

    Effect *m_effect;
    DesktopSession *m_session;
    QString m_key;
};

// The layouts of one desktop span every display, so each host call is
// answered for the display it names: there, that display's card stage is the
// one holding cards beside its layout. A call naming none is answered for the
// display named now.
class Effect::SessionHost final : public DesktopStageHost
{
public:
    SessionHost(Effect *effect, DesktopSession *session)
        : m_effect(effect)
        , m_session(session)
    {
    }

    bool isTabletOutputForDesktopStage(const KWin::LogicalOutput *output) const override
    {
        SessionScope scope(m_effect, m_session, on(output));
        return layouts().isTabletOutputForDesktopStage(output);
    }
    bool usesDrawnZonesForDesktopStage() const override
    {
        return layouts().usesDrawnZonesForDesktopStage();
    }
    bool allowsDesktopStageOnOutput(const KWin::LogicalOutput *output) const override
    {
        SessionScope scope(m_effect, m_session, on(output));
        return layouts().allowsDesktopStageOnOutput(output);
    }
    bool isManagedWindowForDesktopStage(const KWin::EffectWindow *window) const override
    {
        SessionScope scope(m_effect, m_session);
        return layouts().isManagedWindowForDesktopStage(window);
    }
    KWin::LogicalOutput *tabletOutputForDesktopStage() const override
    {
        SessionScope scope(m_effect, m_session);
        return layouts().tabletOutputForDesktopStage();
    }
    bool connectedOutputForDesktopStage(const KWin::LogicalOutput *output) const override
    {
        SessionScope scope(m_effect, m_session, on(output));
        return layouts().connectedOutputForDesktopStage(output);
    }
    long long lastUsedForDesktopStage(const KWin::EffectWindow *window) const override
    {
        SessionScope scope(m_effect, m_session);
        return layouts().lastUsedForDesktopStage(window);
    }
    bool holdsOwnCardsForDesktopStage(const KWin::LogicalOutput *output) const override
    {
        SessionScope scope(m_effect, m_session, on(output));
        return layouts().holdsOwnCardsForDesktopStage(output);
    }
    bool showsOneCardForDesktopStage(const KWin::LogicalOutput *output) const override
    {
        SessionScope scope(m_effect, m_session, on(output));
        return layouts().showsOneCardForDesktopStage(output);
    }
    bool releasesOnOwnDisplayForDesktopStage(const KWin::LogicalOutput *output) const override
    {
        SessionScope scope(m_effect, m_session, on(output));
        return layouts().releasesOnOwnDisplayForDesktopStage(output);
    }
    bool outputCanOwnCards(const KWin::LogicalOutput *output) const override
    {
        SessionScope scope(m_effect, m_session, on(output));
        return layouts().outputCanOwnCards(output);
    }
    KWin::Rect activeTargetForDesktopStage(KWin::LogicalOutput *output) const override
    {
        SessionScope scope(m_effect, m_session, on(output));
        return layouts().activeTargetForDesktopStage(output);
    }
    void prepareOutputForDesktopStage(KWin::LogicalOutput *output) override
    {
        SessionScope scope(m_effect, m_session, on(output));
        layouts().prepareOutputForDesktopStage(output);
    }
    void retireOutputFromDesktopStage(KWin::LogicalOutput *output) override
    {
        SessionScope scope(m_effect, m_session, on(output));
        layouts().retireOutputFromDesktopStage(output);
    }
    QRectF bentoPresentationRect(KWin::EffectWindow *window) const override
    {
        SessionScope scope(m_effect, m_session);
        return layouts().bentoPresentationRect(window);
    }
    void animateBentoLayout(KWin::LogicalOutput *output, const QList<QPointer<KWin::EffectWindow>> &windows,
        const QList<QRectF> &from, const QList<QRectF> &to) override
    {
        SessionScope scope(m_effect, m_session, on(output));
        layouts().animateBentoLayout(output, windows, from, to);
    }
    void paneYieldedForDesktopStage(KWin::EffectWindow *window, KWin::LogicalOutput *output,
                                    const QRectF &drawn) override
    {
        SessionScope scope(m_effect, m_session, on(output));
        layouts().paneYieldedForDesktopStage(window, output, drawn);
    }
    void lastPaneLeftLayoutForDesktopStage(KWin::EffectWindow *window, KWin::LogicalOutput *output,
                                           const QRectF &drawn) override
    {
        SessionScope scope(m_effect, m_session, on(output));
        layouts().lastPaneLeftLayoutForDesktopStage(window, output, drawn);
    }
    void layoutLeftForOtherDisplayForDesktopStage(KWin::LogicalOutput *output) override
    {
        SessionScope scope(m_effect, m_session, on(output));
        layouts().layoutLeftForOtherDisplayForDesktopStage(output);
    }
    void displacedCardMayReturnForDesktopStage(KWin::EffectWindow *window,
                                               KWin::LogicalOutput *output) override
    {
        SessionScope scope(m_effect, m_session, on(output));
        layouts().displacedCardMayReturnForDesktopStage(window, output);
    }
    // The record is the one the stage holding the window keeps.
    std::optional<NativeMoveSnapshot> activeRestoreForDesktopStage(KWin::EffectWindow *window) const override
    {
        const auto held = m_session->keyHolding(window);
        SessionScope scope(m_effect, m_session,
            held ? held : std::optional<QString>(on(window ? window->screen() : nullptr)));
        return layouts().activeRestoreForDesktopStage(window);
    }
    bool admitDisplacedPaneToTablet(KWin::EffectWindow *window, KWin::LogicalOutput *output,
        const std::function<bool()> &commitSource, const NativeMoveSnapshot *restore = nullptr) override
    {
        SessionScope scope(m_effect, m_session, on(output));
        return layouts().admitDisplacedPaneToTablet(window, output, commitSource, restore);
    }
    bool admitSleepingPaneToTablet(KWin::EffectWindow *window, KWin::LogicalOutput *output,
        const std::function<bool()> &commitSource, const NativeMoveSnapshot *restore = nullptr) override
    {
        SessionScope scope(m_effect, m_session, on(output));
        return layouts().admitSleepingPaneToTablet(window, output, commitSource, restore);
    }
    bool admitTransferredWindowToTablet(KWin::EffectWindow *window, KWin::LogicalOutput *output,
        const std::function<bool()> &commitSource, const NativeMoveSnapshot *restore = nullptr) override
    {
        SessionScope scope(m_effect, m_session, on(output));
        return layouts().admitTransferredWindowToTablet(window, output, commitSource, restore);
    }

private:
    DesktopStageHost &layouts() const { return *m_effect; }
    // The stage key of the display named, or of the display named now.
    QString on(const KWin::LogicalOutput *output) const
    {
        return output ? m_effect->stageKeyFor(output) : m_effect->m_scopedKey;
    }

    Effect *m_effect;
    DesktopSession *m_session;
};

template<typename F>
void Effect::forEachSession(F &&f)
{
    std::vector<DesktopSession *> sessions;
    sessions.reserve(m_sessions.size());
    for (const auto &entry : m_sessions) sessions.push_back(entry.second.get());
    for (DesktopSession *session : sessions) {
        SessionScope scope(this, session);
        f();
    }
}

template<typename F>
void Effect::forEachStage(F &&f)
{
    // Every desktop's stage on every display, each named in turn.
    std::vector<std::pair<DesktopSession *, QString>> stages;
    for (const auto &entry : m_sessions)
        for (const auto &display : entry.second->displays) stages.emplace_back(entry.second.get(), display.first);
    for (const auto &[session, key] : stages) {
        SessionScope scope(this, session, key);
        f();
    }
}

template<typename F>
void Effect::forEachDisplayStage(F &&f)
{
    // The scoped desktop's stage on every display, each named in turn.
    if (!m_scopedSession) return;
    DesktopSession *session = m_scopedSession;
    QStringList keys;
    for (const auto &display : session->displays) keys.append(display.first);
    for (const QString &key : std::as_const(keys)) {
        SessionScope scope(this, session, key);
        f(*m_cardStage);
    }
}

// Where an offered card's picture stands in its tile: in its own shape, as
// large as the tile allows, centred.
inline QRectF offerPicture(const QRectF &tile, const QSizeF &card)
{
    const QSizeF fitted = card.scaled(tile.size(), Qt::KeepAspectRatio);
    return QRectF(tile.center() - QPointF(fitted.width() / 2, fitted.height() / 2), fitted);
}

} // namespace Kadunce
