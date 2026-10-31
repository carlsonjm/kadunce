/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Where a new, carried or dropped window lands in a layout.
#include "DesktopStageInternal.h"

namespace Kadunce
{

bool DesktopStageController::handleWindowAdded(KWin::EffectWindow *window)
{
    if (m_restoring) return false;
    if (ownsWindow(window)) return true;
    if (!window || !m_host->isManagedWindowForDesktopStage(window)) {
        return false;
    }
    Session *session = sessionForOutput(window->screen());
    if (!session) {
        return false;
    }
    for (const auto &snapshot : std::as_const(session->snapshots))
        if (snapshot.window == window) return true;
    return admitArrival(session, window, makeSnapshot(window));
}

bool DesktopStageController::admitArrival(Session *session, KWin::EffectWindow *window,
                                          const RestoreSnapshot &snapshot)
{
    // CARD-LIFECYCLE.md §8: a display showing a live layout answers an arrival
    // with that layout. Where the layout can grow it grows; where it cannot, a
    // pane yields, because the arrival shown on top of live panes is the one
    // state §2 does not name. Which pane yields is decided by fit first --
    // only a slot the arrival's minimum size satisfies can hold it -- and by
    // how recently the resident was used second, which planSession expresses
    // as candidate order. The whole decision runs on a value copy: nothing
    // leaves until this admission has published.
    const QString key = session->outputName;
    const auto grows = [&](Session &probe) {
        probe.snapshots.append(snapshot);
        QList<QPointer<KWin::EffectWindow>> unshowable;
        return reflowSession(probe, window, true, false, &unshowable)
            && unshowable.isEmpty();
    };
    const Session live = *session;
    Session candidate = live;
    bool visible = grows(candidate);
    if (!visible && live.side) {
        candidate = live;
        candidate.side.reset();
        candidate.sideWindow.clear();
        visible = grows(candidate);
    }
    if (!visible && m_host->showsOneCardForDesktopStage(outputForKey(key))) {
        // §11, Every display: the arrival is the one card this display shows,
        // and the card it replaces waits in Spread.
        candidate = live;
        candidate.snapshots.append(snapshot);
        QList<QPointer<KWin::EffectWindow>> waiting;
        if (!reflowSession(candidate, window, true, false, &waiting)) return false;
        candidate.snapshots.removeIf([&waiting](const RestoreSnapshot &saved) {
            return saved.window && waiting.contains(saved.window);
        });
        visible = true;
    }
    if (!visible) {
        // Full: the arrival takes the one slot it fits, and the rest of the
        // layout keeps its shape and its places.
        const auto slot = slotForArrival(live, window);
        candidate = live;
        if (slot) {
            if (!takeSlotAtCap(candidate, *slot, snapshot)) return false;
        } else if (autoFillYielding(candidate, snapshot)) {
            // §11, Connected: what opens there fills the layout, laid out
            // again, once the panes used longest ago leave it room.
        } else if (parksOverflow(key)) {
            // An application too big for any slot has to appear because the
            // person just opened it: it takes the Active size alone and the
            // layout waits in the dock.
            candidate = live;
            for (auto &saved : candidate.snapshots)
                if (!saved.userMinimized) saved.parked = true;
            candidate.snapshots.append(snapshot);
            candidate.side.reset();
            candidate.sideWindow.clear();
            if (!reflowSession(candidate, window, true, false)) return false;
        } else {
            return false;
        }
    }
    m_applicationGuard.invalidate();
    const auto pending = captureEvictions(live, candidate, window);
    *session = std::move(candidate);
    if (applySession(*session, true)) scheduleSettle();
    // Last, because adopting reenters this controller: nothing below may hold
    // a session reference or expect this one to still be here.
    publishEvictions(pending);
    minimizeParked(key);
    return true;
}

void DesktopStageController::handleWindowClosed(KWin::EffectWindow *window)
{
    removeWindow(window, false);
}

void DesktopStageController::releaseWindow(KWin::EffectWindow *window)
{
    removeWindow(window, true);
}

void DesktopStageController::handleWindowMinimizedChanged(KWin::EffectWindow *window)
{
    if (m_restoring || m_parking || !window || !window->window()) return;
    QString key;
    std::optional<RestoreSnapshot> picked;
    for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
        auto &session = it.value();
        // Kadunce's own placement is not a user participation change.
        if (session.applying) continue;
        auto saved = std::find_if(session.snapshots.begin(), session.snapshots.end(),
            [window](const auto &s) { return s.window == window; });
        if (saved == session.snapshots.end()) continue;
        key = it.key();
        // On a display that cannot own cards, a window picked from the dock
        // comes back as an arrival, so a resident makes room for it rather
        // than it being sent straight back.
        if (!window->isMinimized() && (saved->parked || saved->userMinimized)
            && parksOverflow(key) && !m_interactionWindow) {
            RestoreSnapshot record = *saved;
            record.parked = false;
            record.userMinimized = false;
            session.snapshots.erase(saved);
            picked = record;
            break;
        }
        saved->userMinimized = window->isMinimized();
        saved->parked = false;
        session.participationDirty = true;
        m_applicationGuard.invalidate();
        break;
    }
    if (picked) {
        auto session = m_sessions.find(key);
        if (session != m_sessions.end()) admitArrival(&session.value(), window, *picked);
        return;
    }
    if (key.isEmpty() || m_interactionWindow) return;
    // CARD-LIFECYCLE.md §7 leaves a sleeping window owned without showing it,
    // so waking one can ask for a pane the layout has no room for. This runs
    // after the loop because adopting reenters this controller.
    settleParticipation(key);
    // §7: the window the user just minimized leaves Bento at once, and §5
    // retains no minimized pane, so it is handed to card ownership asleep.
    // The reflow above has already stopped showing it, so this gives up a
    // record rather than a pane. Where card ownership cannot take it the
    // session keeps it, which is §5's answer when nothing can hold a card.
    // DECISIONS.md § A display without cards organizes everything it shows:
    // there the minimized window waits in the dock, owned, rather than
    // crossing to the display that holds cards.
    if (window->isMinimized() && m_sessions.contains(key) && !parksOverflow(key)
        && !evictToTablet(key, window, {}, EvictedAs::SleepingCard)) {
        qInfo() << "Kadunce" << Revision
                << "kept a minimized pane; no display can hold a sleeping card";
    }
    // Shedding can leave one visible pane, and §5 ends Bento there. It could
    // not be reached while a sleeping snapshot stayed behind.
    endLayoutIntoCardOwnership(key);
    auto session = m_sessions.find(key);
    if (session == m_sessions.end()) return;
    if (applySession(session.value(), false)) scheduleSettle();
}

bool DesktopStageController::admitCardWindow(
    KWin::EffectWindow *window, KWin::LogicalOutput *output,
    const KWin::RectF &geometry)
{
    if (m_restoring) return false;
    Session *session = sessionForOutput(output);
    if (!session || !window || window->isDeleted() || !window->window()) {
        return false;
    }
    for (const auto &existing : std::as_const(session->snapshots)) {
        if (existing.window == window) return session->windows.contains(window);
    }
    const RestoreSnapshot snapshot{WindowPlace{.geometry = geometry, .outputName = outputKey(output),
        .outputGeometry = output ? QRectF(output->geometry()) : QRectF()}, window, true};
    // CARD-LIFECYCLE.md §5: a full layout yields a pane to the arrival instead
    // of parking it. Which pane yields is decided on a value copy and nothing
    // leaves until this admission has published, so a rejection disturbs
    // neither the destination's windows nor its restore records.
    const Session live = *session;
    Session candidate = live;
    if (!shortenToShowable(candidate, snapshot, {}, nullptr)) return false;
    candidate.snapshots.append(snapshot);
    QList<QPointer<KWin::EffectWindow>> unshowable;
    if (!reflowSession(candidate, window, true, true, &unshowable)
        || !unshowable.isEmpty()) return false;
    const auto pending = captureEvictions(live, candidate, window);
    *session = std::move(candidate);
    if (applySession(*session, true)) scheduleSettle();
    // Last, because adopting reenters this controller: nothing below may hold
    // a session reference or expect this one to still be here.
    publishEvictions(pending);
    return true;
}

std::optional<int> DesktopStageController::slotForArrival(
    const Session &session, KWin::EffectWindow *arrival) const
{
    KWin::LogicalOutput *output = outputForKey(session.outputName);
    if (!output || !arrival || arrival->isDeleted() || !arrival->window()
        || session.windows.isEmpty()
        || session.windows.size() != int(session.rects.size())) return std::nullopt;
    const KWin::Rect area = stageArea(output);
    const auto pixels = sessionPixels(session, area);
    if (pixels.size() != session.rects.size()) return std::nullopt;
    const QSizeF minimum = arrival->window()->minSize();
    // CARD-LIFECYCLE.md §11: a Connected display gives up the window used
    // longest ago, where §8 elsewhere gives up the smallest slot that fits.
    // The card a side snap placed never gives way. A slot the arrival does
    // not fit is never given up for it: the arrival could not take it, and
    // would be shed after the pane it displaced (autoFillYielding).
    int slot = -1;
    if (m_host->connectedOutputForDesktopStage(output)) {
        std::vector<long long> lastUsed;
        for (const auto &window : session.windows)
            lastUsed.push_back(m_host->lastUsedForDesktopStage(window.data()));
        const int snapped = session.snappedWindow
            ? int(session.windows.indexOf(session.snappedWindow)) : -1;
        slot = bentoSlotUsedLongestAgo(pixels, lastUsed, minimum.width(), minimum.height(),
                                       snapped);
    } else {
        slot = bentoSlotForArrival(pixels, minimum.width(), minimum.height());
    }
    if (slot < 0 || slot >= session.windows.size() || !session.windows.at(slot))
        return std::nullopt;
    return slot;
}

bool DesktopStageController::takeSlotAtCap(Session &candidate, int slot,
                                           const RestoreSnapshot &arrival) const
{
    if (slot < 0 || slot >= candidate.windows.size() || !arrival.valid
        || !arrival.window) return false;
    const QPointer<KWin::EffectWindow> yielding = candidate.windows.at(slot);
    if (!yielding || yielding == arrival.window) return false;
    candidate.windows[slot] = arrival.window;
    // §11, Connected: the arrival stands where the card it replaces stood.
    if (candidate.placed)
        for (auto &column : candidate.placed->columns)
            for (auto &place : column)
                if (!place.open && place.window == yielding) place.window = arrival.window;
    candidate.snapshots.removeIf(
        [&yielding](const auto &saved) { return saved.window == yielding; });
    candidate.snapshots.append(arrival);
    // The shape is kept, so a side intent naming the window that just left no
    // longer describes anything on this display.
    if (candidate.sideWindow == yielding) {
        candidate.side.reset();
        candidate.sideWindow.clear();
    }
    // A full Connected layout never offers the snapped card's slot, so the one
    // way it yields here is its Stack stepping in its place, which keeps it.
    if (candidate.snappedWindow == yielding) candidate.snappedWindow = arrival.window;
    return true;
}

bool DesktopStageController::autoFillYielding(Session &candidate,
                                              const RestoreSnapshot &arrival) const
{
    KWin::LogicalOutput *output = outputForKey(candidate.outputName);
    if (!output || !m_host->connectedOutputForDesktopStage(output)
        || m_host->showsOneCardForDesktopStage(output) || !arrival.valid || !arrival.window)
        return false;
    // The panes in the order they leave, used longest ago first. The snapped
    // card is never among them, and a window asleep or waiting is no pane.
    QList<QPointer<KWin::EffectWindow>> yielding;
    for (const auto &window : std::as_const(candidate.windows))
        if (window && window != candidate.snappedWindow && window != arrival.window)
            yielding.append(window);
    std::stable_sort(yielding.begin(), yielding.end(),
        [this](const QPointer<KWin::EffectWindow> &a, const QPointer<KWin::EffectWindow> &b) {
            return m_host->lastUsedForDesktopStage(a.data())
                < m_host->lastUsedForDesktopStage(b.data());
        });
    // Each pane that leaves gives up its record, so the solve no longer owns
    // it and the publisher makes it a card in Spread.
    Session remaining = candidate;
    for (const auto &window : std::as_const(yielding)) {
        remaining.snapshots.removeIf(
            [&window](const RestoreSnapshot &saved) { return saved.window == window; });
        Session plan = remaining;
        plan.snapshots.append(arrival);
        QList<QPointer<KWin::EffectWindow>> unshowable;
        if (planSession(plan, arrival.window, true, &unshowable) && unshowable.isEmpty()
            && plan.windows.contains(arrival.window)) {
            candidate = std::move(plan);
            return true;
        }
    }
    return false;
}

std::optional<int> DesktopStageController::paneCap(const KWin::LogicalOutput *output) const
{
    // A display that plays cards has a sizing, and the sizing names the cap.
    if (m_host->connectedOutputForDesktopStage(output))
        return sizingPaneCap(DisplaySizing::Connected);
    if (m_host->isTabletOutputForDesktopStage(output) || m_host->holdsOwnCardsForDesktopStage(output))
        return sizingPaneCap(DisplaySizing::Solo);
    return std::nullopt;
}

bool DesktopStageController::admitCardToLiveBento(KWin::EffectWindow *window,
    const std::function<bool()> &commitSource, bool growOnly, bool takesFocus)
{
    // Why a card was refused a pane, for the journal.
    const auto refuse = [window](const char *why) {
        qInfo() << "Kadunce" << Revision << "refused"
                << (window ? window->caption() : QString()) << "a pane:" << why;
        return false;
    };
    if (m_restoring || !commitSource || !window || window->isDeleted()
        || !window->window() || window->isUserMove() || window->isUserResize()
        || m_interactionWindow || m_railDrag) return refuse("busy");
    Session *session = sessionForOutput(window->screen());
    if (!session || session->applying) return refuse(session ? "applying" : "no layout there");
    for (const auto &existing : std::as_const(session->snapshots))
        if (existing.window == window)
            return session->windows.contains(window) || refuse("a stale record of it");
    // makeSnapshot reads the record card ownership is still holding, so the
    // card keeps the geometry it had before any of this and release returns it
    // there. §5 decides which pane yields on a value copy; nothing leaves and
    // no ownership moves until the whole plan is proved.
    const RestoreSnapshot snapshot = makeSnapshot(window);
    if (!snapshot.valid) return refuse("no record");
    const Session live = *session;
    // §8: grow first. A layout with room takes the arrival beside everything it
    // already shows, and an empty remainder is what says so.
    Session candidate = live;
    candidate.snapshots.append(snapshot);
    QList<QPointer<KWin::EffectWindow>> unshowable;
    const bool grew = reflowSession(candidate, window, true, true, &unshowable)
        && unshowable.isEmpty();
    if (!grew && growOnly) return false;
    if (!grew) {
        // The layout is full, so the arrival takes one slot and only that
        // slot's occupant leaves. Re-solving instead would move the panes the
        // user did not touch, which is the rearrangement §8 is avoiding.
        // §11, Connected: a card no slot fits is laid out with the rest once
        // the panes used longest ago leave it room, and never into a slot it
        // cannot take, which would shed it after the pane it displaced.
        const auto slot = slotForArrival(live, window);
        candidate = live;
        if (slot) {
            if (!takeSlotAtCap(candidate, *slot, snapshot)) return refuse("no slot at the cap");
        } else if (!autoFillYielding(candidate, snapshot)) {
            return refuse("no room");
        }
    }
    // OwnershipHandoff: destination acceptance precedes source removal. The
    // plan above is the acceptance; card ownership gives the window up here,
    // and only a published plan follows, so no window is ever named by both.
    if (!commitSource()) return refuse("its card was kept");
    const auto pending = captureEvictions(live, candidate, window);
    *session = std::move(candidate);
    if (applySession(*session, takesFocus)) scheduleSettle();
    // Last, because adopting reenters this controller: nothing below may hold
    // a session reference or expect this one to still be here.
    publishEvictions(pending);
    return true;
}

bool DesktopStageController::replacePaneWithCard(KWin::EffectWindow *pane,
    KWin::EffectWindow *card, const std::function<bool()> &commitSource, bool closing)
{
    if (m_restoring || !commitSource || !pane || !card || pane == card
        || card->isDeleted() || !card->window() || card->isUserMove() || card->isUserResize()
        || m_interactionWindow || m_railDrag) return false;
    Session *session = nullptr;
    for (Session &candidate : m_sessions)
        if (candidate.windows.contains(pane)) { session = &candidate; break; }
    if (!session || session->applying) return false;
    const int slot = session->windows.indexOf(pane);
    if (slot < 0) return false;
    for (const auto &existing : std::as_const(session->snapshots))
        if (existing.window == card) return false;
    KWin::LogicalOutput *output = outputForKey(session->outputName);
    if (!output) return false;
    const auto pixels = sessionPixels(*session, stageArea(output));
    if (slot >= int(pixels.size())) return false;
    const QSizeF minimum = card->window()->minSize();
    if (minimum.width() > pixels[slot].width || minimum.height() > pixels[slot].height) return false;
    const RestoreSnapshot snapshot = makeSnapshot(card);
    if (!snapshot.valid) return false;
    const Session live = *session;
    Session candidate = live;
    if (!takeSlotAtCap(candidate, slot, snapshot)) return false;
    if (!commitSource()) return false;
    // A closing pane has no card to become; it only gives up its slot.
    const QList<PendingEviction> pending = closing ? QList<PendingEviction>{}
                                                   : captureEvictions(live, candidate);
    *session = std::move(candidate);
    if (applySession(*session, true)) scheduleSettle();
    if (!closing) publishEvictions(pending);
    return true;
}

bool DesktopStageController::stackPaneOnto(KWin::EffectWindow *pane, KWin::EffectWindow *onto,
    KWin::EffectWindow *next, const std::function<bool()> &commitSource)
{
    if (m_restoring || !commitSource || !pane || !onto || pane == onto || next == pane
        || next == onto || pane->isDeleted() || !pane->window() || pane->isUserMove()
        || pane->isUserResize() || m_interactionWindow || m_railDrag) return false;
    if (next && (next->isDeleted() || !next->window() || next->isUserMove() || next->isUserResize()))
        return false;
    QString key;
    for (auto it = m_sessions.cbegin(); it != m_sessions.cend(); ++it)
        if (it->windows.contains(pane) && it->windows.contains(onto)) { key = it.key(); break; }
    if (key.isEmpty()) return false;
    Session *session = &m_sessions[key];
    if (session->applying) return false;
    for (const auto &existing : std::as_const(session->snapshots))
        if (next && existing.window == next) return false;
    KWin::LogicalOutput *output = outputForKey(key);
    if (!output) return false;
    const auto pixels = sessionPixels(*session, stageArea(output));
    const int from = session->windows.indexOf(pane);
    const int to = session->windows.indexOf(onto);
    if (from < 0 || to < 0 || from >= int(pixels.size()) || to >= int(pixels.size())) return false;
    const auto fits = [&pixels](KWin::EffectWindow *window, int slot) {
        const QSizeF minimum = window->window()->minSize();
        return minimum.width() <= pixels[slot].width && minimum.height() <= pixels[slot].height;
    };
    if (!fits(pane, to) || (next && !fits(next, from))) return false;
    const Session live = *session;
    Session candidate = live;
    // The pane takes the slot it was let go on, in the place that slot had.
    const QPointer<KWin::EffectWindow> paneHandle(pane);
    if (candidate.placed) {
        if (!next) bentoColumnsRemove(*candidate.placed, paneHandle);
        for (auto &column : candidate.placed->columns)
            for (auto &place : column)
                if (!place.open && place.window == pane) place.window = next;
        for (auto &column : candidate.placed->columns)
            for (auto &place : column)
                if (!place.open && place.window == onto) place.window = paneHandle;
        if (candidate.placed->columns.empty()) candidate.placed.reset();
    }
    candidate.windows[to] = paneHandle;
    if (next) candidate.windows[from] = next;
    else {
        candidate.windows.removeAt(from);
        if (from < int(candidate.rects.size())) candidate.rects.erase(candidate.rects.begin() + from);
    }
    if (candidate.sideWindow == onto) candidate.sideWindow = paneHandle;
    else if (candidate.sideWindow == pane) {
        candidate.side.reset();
        candidate.sideWindow.clear();
    }
    // The snapped place keeps the Stack standing in it: the pane now in front
    // of the one it was let go on, or the next card of its own.
    if (candidate.snappedWindow == onto) candidate.snappedWindow = paneHandle;
    else if (candidate.snappedWindow == pane) {
        candidate.snappedWindow = next;
        if (!next) candidate.snappedSide.reset();
    }
    // The pane's own record moves to the slot's place in the order, so a
    // layout laid out again keeps it where the slot stood.
    const auto paneSaved = std::find_if(candidate.snapshots.cbegin(), candidate.snapshots.cend(),
        [pane](const auto &saved) { return saved.window == pane; });
    const auto ontoSaved = std::find_if(candidate.snapshots.cbegin(), candidate.snapshots.cend(),
        [onto](const auto &saved) { return saved.window == onto; });
    if (paneSaved == candidate.snapshots.cend() || ontoSaved == candidate.snapshots.cend()) return false;
    const int paneRecord = int(paneSaved - candidate.snapshots.cbegin());
    const int ontoRecord = int(ontoSaved - candidate.snapshots.cbegin());
    candidate.snapshots.move(paneRecord, ontoRecord);
    candidate.snapshots.removeIf([onto](const auto &saved) { return saved.window == onto; });
    if (next) {
        const RestoreSnapshot snapshot = makeSnapshot(next);
        if (!snapshot.valid) return false;
        candidate.snapshots.append(snapshot);
    } else {
        // Its room given up, the layout is laid out again as after a closure.
        QList<QPointer<KWin::EffectWindow>> unshowable;
        if (!reflowSession(candidate, nullptr, false, false, &unshowable) || !unshowable.isEmpty()
            || !candidate.windows.contains(paneHandle)) return false;
    }
    if (!commitSource()) return false;
    const auto pending = captureEvictions(live, candidate);
    *session = std::move(candidate);
    if (applySession(*session, true)) scheduleSettle();
    publishEvictions(pending);
    // §5: a layout left with one pane ends, as after a closure.
    if (!next) {
        endLayoutIntoCardOwnership(key);
        auto surviving = m_sessions.find(key);
        if (surviving != m_sessions.end() && applySession(surviving.value(), false))
            scheduleSettle();
    }
    return true;
}

KWin::EffectWindow *DesktopStageController::paneAt(const KWin::LogicalOutput *output,
    QPointF position, const KWin::EffectWindow *except) const
{
    const auto *session = sessionForOutput(output);
    if (!session || session->applying) return nullptr;
    for (const auto &window : session->windows) {
        if (!window || window == except || window->isDeleted() || window->isMinimized()) continue;
        if (QRectF(window->frameGeometry()).contains(position)) return window;
    }
    return nullptr;
}

std::optional<DesktopStageController::PreparedDrop> DesktopStageController::prepareCardDrop(
    KWin::EffectWindow *window, KWin::LogicalOutput *output, const KWin::RectF &geometry,
    CardDropIntent intent, std::optional<BentoSidePlacement> side,
    KWin::EffectWindow *pairPartner, bool alone) const
{
    if (m_restoring || !window || window->isDeleted() || !window->window()
        || !output || m_retiredOutputs.contains(output)
        || !KWin::effects->screens().contains(output) || !geometry.isValid()
        || !std::isfinite(geometry.x()) || !std::isfinite(geometry.y())
        || !std::isfinite(geometry.width()) || !std::isfinite(geometry.height())
        || !m_host->allowsDesktopStageOnOutput(output)
        || !m_host->isManagedWindowForDesktopStage(window)
        || (intent != CardDropIntent::OpenSpace && intent != CardDropIntent::ActivateBento
            && intent != CardDropIntent::NativeDesktop))
        return std::nullopt;
    // An ordinary source may finish its held carry on a free external output.
    // Keep managed departures and occupied/tablet destinations on their
    // workspace admission path; NativeDesktop must not bypass that ownership.
    if (intent == CardDropIntent::NativeDesktop && window->screen() != output
        && (managesWindow(window) || sessionForOutput(output)
            || m_host->isTabletOutputForDesktopStage(output)))
        return std::nullopt;
    PreparedDrop drop;
    drop.window = window;
    drop.output = output;
    drop.geometry = geometry;
    drop.outputGeometry = output->geometry();
    drop.area = stageArea(output);
    drop.generation = m_applicationGuard.generation();
    drop.owner = m_carrySourceIdentity;
    drop.intent = intent;
    drop.side = side;
    drop.hadSession = sessionForOutput(output) != nullptr;
    drop.leavingBento = managesWindow(window);
    // §11, Connected: a card placed alone begins a layout of one, so nothing
    // else on the display is an input either.
    if (alone) {
        if (drop.hadSession || pairPartner || !side) return std::nullopt;
        drop.alone = true;
    } else if (pairPartner && !drop.hadSession) {
        // §3: a pairing names its partner, so nothing else on the display is
        // an input to the solve, the preview or the revalidation.
        if (pairPartner == window || pairPartner->isDeleted() || !pairPartner->window()
            || pairPartner->screen() != output || pairPartner->isMinimized()
            || !m_host->isManagedWindowForDesktopStage(pairPartner)) return std::nullopt;
        drop.pairPartner = pairPartner;
        drop.residents = {QPointer<KWin::EffectWindow>(pairPartner)};
    } else {
        drop.residents = collectWindows(output, window);
    }
    auto inputs = drop.residents;
    if (!inputs.contains(window)) inputs.append(window);
    for (const auto &input : inputs) {
        if (!input || !input->window()) return std::nullopt;
        drop.minimumSizes.append(input->window()->minSize());
        drop.sourceGeometries.append(input->frameGeometry());
    }
    return drop;
}

std::optional<DesktopStageController::PreparedDrop> DesktopStageController::prepareLocalCardDrop(
    KWin::EffectWindow *window, KWin::LogicalOutput *output,
    const KWin::RectF &geometry, QPointF contact, std::optional<double> columnShare) const
{
    auto drop = prepareCardDrop(window, output, geometry);
    const auto *session = sessionForOutput(output);
    if (!drop || !session || !session->windows.contains(window)) return std::nullopt;
    // §11, Connected: resized by the wheel, the pane keeps its place.
    if (columnShare && session->placed) {
        drop->localTarget = window;
        drop->columnShare = columnShare;
        return prepareLocalPlacement(*drop) ? drop : std::nullopt;
    }
    const auto pixels = sessionPixels(*session, drop->area);
    for (int i = 0; i < session->windows.size() && i < static_cast<int>(pixels.size()); ++i) {
        const auto &p = pixels.at(i);
        if (!QRectF(p.x, p.y, p.width, p.height).contains(contact)) continue;
        drop->localTarget = session->windows.at(i);
        return prepareLocalPlacement(*drop) ? drop : std::nullopt;
    }
    return std::nullopt;
}

bool DesktopStageController::holdsInPlacedColumns(const KWin::LogicalOutput *output,
                                                  const KWin::EffectWindow *window) const
{
    const auto *session = output ? sessionForOutput(output) : nullptr;
    return session && session->placed
        && bentoColumnsHold(*session->placed, QPointer<KWin::EffectWindow>(const_cast<KWin::EffectWindow *>(window)));
}

std::optional<DesktopStageController::Session> DesktopStageController::prepareLocalPlacement(
    const PreparedDrop &drop) const
{
    const auto *session = sessionForOutput(drop.output);
    if (!session || session->applying) return std::nullopt;
    if (drop.side && session->windows.contains(drop.window)) {
        Session plan = *session;
        plan.side = drop.side;
        plan.sideWindow = drop.window;
        QList<QPointer<KWin::EffectWindow>> unshowable;
        if (!planSession(plan, drop.window, true, &unshowable)) return std::nullopt;
        // CARD-LIFECYCLE.md §5: a resident this shape cannot keep leaves rather
        // than staying owned and unshown. Dropping its snapshot here is what
        // makes the plan nameable; the publisher hands the window over after.
        if (!unshowable.isEmpty() && !canPlaceEvictedCard()) return std::nullopt;
        for (const auto &yielding : std::as_const(unshowable)) {
            plan.snapshots.removeIf(
                [&yielding](const auto &saved) { return saved.window == yielding; });
        }
        return plan;
    }
    if (!drop.localTarget) return std::nullopt;
    if (drop.columnShare && drop.localTarget == drop.window) {
        if (!session->placed || !drop.output) return std::nullopt;
        Session plan = *session;
        const double span = std::max(1, drop.area.width());
        const bool resized = bentoColumnsResize(*plan.placed, QPointer<KWin::EffectWindow>(drop.window),
            *drop.columnShare, [span](const QPointer<KWin::EffectWindow> &held) {
                return held && held->window() ? held->window()->minSize().width() / span : 0.0;
            });
        if (!resized) return std::nullopt;
        plan.side.reset();
        plan.sideWindow.clear();
        QList<QPointer<KWin::EffectWindow>> unshowable;
        if (!planSession(plan, drop.window, true, &unshowable) || !unshowable.isEmpty()
            || !plan.windows.contains(drop.window)) return std::nullopt;
        return plan;
    }
    const int from = session->windows.indexOf(drop.window);
    const int to = session->windows.indexOf(drop.localTarget);
    const auto pixels = sessionPixels(*session, drop.area);
    if (from < 0 || to < 0 || from >= static_cast<int>(pixels.size())
        || to >= static_cast<int>(pixels.size())) return std::nullopt;
    const auto fits = [&](const auto &window, int index) {
        if (!window || window->isDeleted() || !window->window()
            || window->isUserMove() || window->isUserResize()) return false;
        const auto size = window->window()->minSize();
        return size.width() <= pixels.at(index).width && size.height() <= pixels.at(index).height;
    };
    if (!fits(drop.window, to) || !fits(drop.localTarget, from)) return std::nullopt;
    Session result = *session;
    result.side.reset();
    result.sideWindow.clear();
    // Moved by hand, neither card stands where a snap put it any longer.
    if (from != to && (result.snappedWindow == drop.window
                       || result.snappedWindow == drop.localTarget)) {
        result.snappedWindow.clear();
        result.snappedSide.reset();
    }
    result.windows.swapItemsAt(from, to);
    // The columns a side snap placed trade the two as well, so a later snap
    // reads the places the panes now have.
    if (result.placed) {
        for (auto &column : result.placed->columns)
            for (auto &place : column) {
                if (place.open) continue;
                if (place.window == drop.window) place.window = drop.localTarget;
                else if (place.window == drop.localTarget) place.window = drop.window;
            }
    }
    // Carry the original restore records with their identities; never replace
    // them with pane geometry. Keep collection order aligned for later reflow.
    auto a = std::find_if(result.snapshots.begin(), result.snapshots.end(),
        [&](const auto &s) { return s.window == drop.window; });
    auto b = std::find_if(result.snapshots.begin(), result.snapshots.end(),
        [&](const auto &s) { return s.window == drop.localTarget; });
    if (a == result.snapshots.end() || b == result.snapshots.end()) return std::nullopt;
    std::iter_swap(a, b);
    return result;
}

bool DesktopStageController::cardDropValid(const PreparedDrop &drop) const
{
    if (!drop.consumed || *drop.consumed || drop.owner.lock() != m_carrySourceIdentity)
        return false;
    const auto current = prepareCardDrop(drop.window, drop.output, drop.geometry,
        drop.intent, drop.side, drop.pairPartner, drop.alone);
    if (!current || current->generation != drop.generation
        || current->pairPartner != drop.pairPartner
        || current->outputGeometry != drop.outputGeometry || current->area != drop.area
        || current->hadSession != drop.hadSession
        || current->leavingBento != drop.leavingBento
        || current->residents != drop.residents) return false;
    const bool sameInputs = current->minimumSizes == drop.minimumSizes
        && current->sourceGeometries == drop.sourceGeometries;
    // First-layout edge entry is re-solved by transferCardWindow immediately
    // before publication. A configure acknowledgement or still-feasible minimum
    // hint update must not invalidate otherwise unchanged receiver ownership.
    const bool refreshableFirstEdge = drop.intent == CardDropIntent::ActivateBento
        && !drop.hadSession && !drop.leavingBento && !drop.localTarget;
    return (sameInputs || refreshableFirstEdge)
        && (!drop.localTarget || prepareLocalPlacement(drop).has_value());
}

bool DesktopStageController::transferNativeCarryToDesktop(
    const PreparedCarrySource &source, const PreparedDrop &drop)
{
    if (!nativeCarrySourceValid(source) || !cardDropValid(drop)
        || source.m_window != drop.window
        || drop.window->isUserMove() || drop.window->isUserResize()
        || !drop.outputGeometry.intersects(drop.geometry.toRect())) return false;
    if (drop.intent == CardDropIntent::NativeDesktop && !source.isDesktopWindow()) {
        if (source.origin().output != outputKey(drop.output)) return false;
        *drop.consumed = true;
        return handoffWindowToOutput(drop.window, drop.output, drop.geometry, drop.intent);
    }
    if (source.isDesktopWindow()) {
        if (drop.localTarget) return false;
        // No source layout exists to remove from. Preserve the pickup snapshot
        // if this admission creates Bento; carried geometry is presentation only.
        PreparedDrop validation = drop;
        validation.consumed = std::make_shared<bool>(false);
        *drop.consumed = true;
        if (m_host->isTabletOutputForDesktopStage(drop.output) && !drop.hadSession
            && drop.intent == CardDropIntent::OpenSpace
            && source.origin().output != outputKey(drop.output)) {
            bool committed = false;
            m_host->admitTransferredWindowToTablet(drop.window, drop.output, [&] {
                if (committed || !nativeCarrySourceValid(source) || !cardDropValid(validation)) return false;
                committed = true;
                return true;
            });
            return committed;
        }
        return transferCardWindow(drop.window, drop.output, drop.geometry,
            [&] { return nativeCarrySourceValid(source) && cardDropValid(validation); },
            [] {}, drop.intent, &source.restoreSnapshot(), drop.side);
    }
    if (source.origin().output == outputKey(drop.output)) {
        const auto placement = prepareLocalPlacement(drop);
        if (!placement) return false;
        *drop.consumed = true;
        if (!drop.side && drop.localTarget == drop.window) return true; // Already physically home.
        auto *session = sessionForOutput(drop.output);
        if (!session) return false;
        const auto pending = captureEvictions(*session, *placement);
        *session = *placement; // Publish before native callbacks can intervene.
        if (applySession(*session, false)) scheduleSettle();
        publishEvictions(pending);
        return true; // Publication cannot become a stale source rollback.
    }
    if (drop.localTarget) return false;
    // The existing Bento transaction solves source and receiver on value copies
    // and publishes both without callbacks/native writes between them. Reserve
    // that exact transaction, not a second departure/placement implementation.
    *drop.consumed = true;
    return handoffWindowToOutput(drop.window, drop.output, drop.geometry, drop.intent, drop.side);
}

std::optional<KWin::RectF> DesktopStageController::cardDropPreview(const PreparedDrop &drop)
{
    if (drop.intent == CardDropIntent::NativeDesktop)
        return cardDropValid(drop) ? std::optional<KWin::RectF>(drop.geometry) : std::nullopt;
    if (!cardDropValid(drop) || (m_host->isTabletOutputForDesktopStage(drop.output)
        && drop.intent == CardDropIntent::OpenSpace && !drop.hadSession))
        return std::nullopt; // Tablet arrival has its own presentation owner.
    if (!drop.hadSession && drop.intent == CardDropIntent::OpenSpace) return drop.geometry;
    const bool local = drop.localTarget || (drop.side && managesWindow(drop.window)
        && drop.window->screen() == drop.output);
    const auto plan = local ? prepareLocalPlacement(drop)
        : prepareCardAdmission(drop.window, drop.output, drop.geometry, nullptr,
                               drop.side, drop.pairPartner, nullptr, drop.alone);
    const auto index = plan ? plan->windows.indexOf(drop.window) : -1;
    const auto pixels = plan ? sessionPixels(*plan, drop.area) : std::vector<BentoPixelRect>{};
    std::optional<KWin::RectF> preview;
    if (index >= 0 && index < static_cast<int>(pixels.size())) {
        const auto &pixel = pixels.at(index);
        preview = KWin::RectF(pixel.x, pixel.y, pixel.width, pixel.height);
    }
    // A display without cards answers a carry with its whole layout, and the
    // answer turns on minimum sizes nothing else reports. Each new answer is
    // logged with what decided it, so a drag seen on hardware can be replayed.
    if (drop.hadSession && parksOverflow(outputKey(drop.output)) && drop.window->window()) {
        const auto minimum = drop.window->window()->minSize();
        QString residents;
        for (const auto &resident : drop.residents)
            if (resident && resident->window())
                residents += QStringLiteral(" %1x%2").arg(resident->window()->minSize().width())
                    .arg(resident->window()->minSize().height());
        const QString answer = QStringLiteral("%1 min %2x%3 side %4 residents%5 -> %6")
            .arg(logNameOf(drop.window)).arg(minimum.width()).arg(minimum.height())
            .arg(!drop.side ? QStringLiteral("none")
                 : drop.side->right ? QStringLiteral("right") : QStringLiteral("left"))
            .arg(residents)
            .arg(!preview ? QStringLiteral("refused")
                 : QStringLiteral("%1 panes, pane %2x%3 at %4,%5").arg(plan->windows.size())
                       .arg(preview->width()).arg(preview->height())
                       .arg(preview->x()).arg(preview->y()));
        if (answer != m_lastOverflowPreview) {
            m_lastOverflowPreview = answer;
            qInfo() << "Kadunce carry onto" << drop.output->name() << qPrintable(answer);
        }
    }
    return preview;
}

bool DesktopStageController::activatePreparedTabletDrop(const PreparedDrop &drop,
    const NativeMoveSnapshot *restore, const std::function<bool()> &commitSource)
{
    if (m_restoring || !commitSource || !cardDropValid(drop)
        || !m_host->isTabletOutputForDesktopStage(drop.output)
        || drop.intent != CardDropIntent::ActivateBento || drop.hadSession
        || !drop.side || (!drop.pairPartner && !drop.alone) || drop.localTarget) return false;
    // CARD-LIFECYCLE.md §3: Bento begins with the carried window on the side it
    // was released into and the Active card opposite it. No other window joins,
    // so the plan is solved from exactly those two and is refused if it is not
    // two panes. Nothing else on the display is read, moved or minimized.
    // A window arriving from the desktop brings its own record; a card already
    // owned here has one the card stage holds, which `makeSnapshot` reads.
    // §11, Connected: a card placed alone is the plan's one pane.
    const auto plan = prepareCardAdmission(drop.window, drop.output, drop.geometry,
        restore, drop.side, drop.pairPartner, nullptr, drop.alone);
    if (!plan || plan->windows.size() != (drop.alone ? 1 : 2)
        || !plan->windows.contains(drop.window)
        || (!drop.alone && !plan->windows.contains(drop.pairPartner))
        || (drop.alone && !plan->placed && plan->zones.isEmpty())) return false;
    std::erase_if(m_restoredMinimizations, [&drop](const auto &pending) {
        return pending->output() == drop.output
            || pending->result() != RestoredMinimization::Result::Pending;
    });
    *drop.consumed = true;
    // Destination acceptance is complete before the source gives the pair up,
    // and publication follows with no callback in between.
    if (!commitSource()) return false;
    m_sessions.insert(plan->outputName, *plan);
    Session &stored = m_sessions[plan->outputName];
    if (applySession(stored, true)) scheduleSettle();
    if (drop.alone)
        qInfo() << "Kadunce" << Revision << "placed a card alone, the rest of"
                << plan->outputName << "left open";
    else qInfo() << "Kadunce" << Revision << "paired two cards into output-local Bento";
    return true;
}

std::optional<DesktopStageController::Session> DesktopStageController::prepareCardAdmission(
    KWin::EffectWindow *window, KWin::LogicalOutput *output, const KWin::RectF &geometry,
    const NativeMoveSnapshot *restore, std::optional<BentoSidePlacement> side,
    KWin::EffectWindow *pairPartner, QList<QPointer<KWin::EffectWindow>> *unadopted,
    bool alone)
{
    const QString key = outputKey(output);
    const auto managed = m_host->activeRestoreForDesktopStage(window);
    if (!restore && managed) restore = &*managed;
    RestoreSnapshot incoming{restore ? restore->place()
        : WindowPlace{.geometry = geometry, .floatingGeometry = geometry, .outputName = key,
              .outputGeometry = output ? QRectF(output->geometry()) : QRectF()}, window, true};
    // CARD-LIFECYCLE.md §11, Connected: a card paired into a display with no
    // layout, picked, newly opened or snapped, stands with its partner as two
    // even columns, the arrival in the one its side names, and the columns
    // hold their places while the next card fills, joins or widens them. A
    // snap makes the arrival the snapped card; a pick or an opened window
    // snaps nothing. Where the two cannot hold their minimums side by side,
    // the solve lays them out as any layout past its minimum sizes.
    std::optional<BentoPlacedColumns> pair;
    if (pairPartner && side && side->connected && !alone && !sessionForOutput(output)
        && m_host->connectedOutputForDesktopStage(output)
        && !m_host->showsOneCardForDesktopStage(output)) {
        const BentoColumnSlot<QPointer<KWin::EffectWindow>> arriving{window, false};
        const BentoColumnSlot<QPointer<KWin::EffectWindow>> partner{pairPartner, false};
        pair = BentoPlacedColumns{};
        pair->columns = {{partner}, {arriving}};
        pair->paired = true;
        if (!side->right) std::swap(pair->columns.front(), pair->columns.back());
        // Two of known kinds stand at the sizes they want (NaturalSize.h),
        // what they leave open beside them, the arrival on its side.
        const auto needOf = [this, output](const QPointer<KWin::EffectWindow> &held) {
            return held ? m_host->naturalNeedForDesktopStage(held.data(), output).value_or(NaturalNeed{})
                        : NaturalNeed{};
        };
        const NaturalNeed partnerNeed = needOf(QPointer<KWin::EffectWindow>(pairPartner));
        const NaturalNeed arrivalNeed = needOf(QPointer<KWin::EffectWindow>(window));
        if (partnerNeed.width > 0.0 && arrivalNeed.width > 0.0) {
            BentoPlacedColumns natural;
            natural.columns = {{partner}};
            natural.natural = true;
            natural.shares = {{QPointer<KWin::EffectWindow>(pairPartner), partnerNeed.width}};
            const auto fit = bentoColumnsFitNatural(natural, QPointer<KWin::EffectWindow>(window),
                                                    arrivalNeed, needOf, 0);
            if (fit == NaturalFit::Opened || fit == NaturalFit::Squeezed) {
                if (!side->right) std::rotate(natural.columns.rbegin(), natural.columns.rbegin() + 1,
                                              natural.columns.rend());
                pair = natural;
            }
        }
    }
    const auto planner = [this, side, window, alone, pair](Session &plan,
                                                           const auto &preferred, bool required) {
        if (pair) {
            plan.placed = pair;
            if (side->snaps) { plan.snappedWindow = window; plan.snappedSide = side; }
        } else if (side) {
            plan.side = side;
            plan.sideWindow = window;
        }
        plan.placeAlone = alone;
        return reflowSession(plan, preferred, required, false);
    };
    if (const auto *session = sessionForOutput(output)) {
        if (alone) return std::nullopt;
        // CARD-LIFECYCLE.md §5: a full layout yields the pane the solve cannot
        // keep. Shortening the value copy is what makes the admission one the
        // layout can show in full; the caller publishes it and then hands the
        // yielded window over, so a preview reports the rect a commit would
        // produce without any owner moving to find that out.
        Session shortened = *session;
        if (!shortenToShowable(shortened, incoming, side, window)) return std::nullopt;
        return prepareBentoAdmission(shortened, QPointer<KWin::EffectWindow>(window),
                                     incoming, planner);
    }
    Session empty;
    empty.outputName = key;
    QList<RestoreSnapshot> owned;
    // §3: first entry pairs the arrival with one named card and fills no other
    // pane. Without a partner this is an ordinary display-wide activation.
    if (pairPartner) {
        owned.append(makeSnapshot(pairPartner));
    } else if (!alone) {
        for (const auto &resident : collectWindows(output, window)) owned.append(makeSnapshot(resident));
        // §5: a resident this first layout cannot show is not adopted at all.
        // It is still Native, so nothing is taken from it, and the caller gives
        // it to card ownership once the layout is published.
        Session probe = empty;
        probe.snapshots = owned;
        probe.snapshots.append(incoming);
        if (side) { probe.side = side; probe.sideWindow = window; }
        QList<QPointer<KWin::EffectWindow>> unshowable;
        if (planSession(probe, window, true, &unshowable)) {
            for (const auto &resident : std::as_const(unshowable)) {
                if (!resident || resident == window) continue;
                if (unadopted && !unadopted->contains(resident)) unadopted->append(resident);
                owned.removeIf([&resident](const auto &saved) { return saved.window == resident; });
            }
        }
    }
    return prepareBentoActivationWithArrival(empty, owned,
        QPointer<KWin::EffectWindow>(window), incoming, planner);
}

bool DesktopStageController::transferPreparedCard(const PreparedDrop &drop,
    const std::function<bool()> &commitSource, const std::function<void()> &releaseSource)
{
    if (!commitSource || !releaseSource || !cardDropValid(drop)) return false;
    // Consume before any caller callback, but revalidate the original receiver
    // immediately before source commitment. Solving still occurs on value copies.
    PreparedDrop validation = drop;
    validation.consumed = std::make_shared<bool>(false);
    *drop.consumed = true;
    return transferCardWindow(drop.window, drop.output, drop.geometry, [&] {
        return cardDropValid(validation) && commitSource();
    }, releaseSource, drop.intent, nullptr, drop.side);
}

bool DesktopStageController::placePaneBySide(KWin::EffectWindow *window,
    KWin::LogicalOutput *output, BentoSidePlacement side)
{
    if (!window || !output || window->screen() != output || !managesWindow(window)) return false;
    const auto drop = prepareCardDrop(window, output, KWin::RectF(window->frameGeometry()),
                                      CardDropIntent::OpenSpace, side);
    if (!drop) return false;
    const auto placement = prepareLocalPlacement(*drop);
    auto *session = sessionForOutput(output);
    if (!placement || !session) return false;
    const auto pending = captureEvictions(*session, *placement);
    *session = *placement;
    if (applySession(*session, false)) scheduleSettle();
    publishEvictions(pending);
    return true;
}

bool DesktopStageController::swapPanes(KWin::EffectWindow *window, KWin::EffectWindow *other)
{
    if (!window || !other || window == other || !managesWindow(window) || !managesWindow(other))
        return false;
    KWin::LogicalOutput *output = window->screen();
    if (!output || other->screen() != output) return false;
    auto drop = prepareCardDrop(window, output, KWin::RectF(window->frameGeometry()));
    if (!drop) return false;
    drop->localTarget = other;
    const auto placement = prepareLocalPlacement(*drop);
    auto *session = sessionForOutput(output);
    if (!placement || !session) return false;
    const auto pending = captureEvictions(*session, *placement);
    *session = *placement;
    if (applySession(*session, false)) scheduleSettle();
    publishEvictions(pending);
    return true;
}

bool DesktopStageController::transferCardWindow(KWin::EffectWindow *window, KWin::LogicalOutput *output,
    const KWin::RectF &geometry, const std::function<bool()> &commitSource,
    const std::function<void()> &releaseSource, CardDropIntent intent,
    const NativeMoveSnapshot *restore, std::optional<BentoSidePlacement> side)
{
    if (m_restoring || !commitSource || !releaseSource || !window || window->isDeleted() || !window->window()
        || window->isUserMove() || window->isUserResize() || !output || !geometry.isValid()
        || m_retiredOutputs.contains(output) || !KWin::effects->screens().contains(output)) return false;
    if (intent != CardDropIntent::OpenSpace && intent != CardDropIntent::ActivateBento
        && intent != CardDropIntent::NativeDesktop) return false;
    if (intent == CardDropIntent::NativeDesktop && managesWindow(window)) return false;
    // §11, Every display: no window stands loose on a display holding its own
    // cards, so a card set down there joins its layout, or begins one.
    if (intent == CardDropIntent::OpenSpace && m_host->holdsOwnCardsForDesktopStage(output))
        intent = CardDropIntent::ActivateBento;
    if (intent == CardDropIntent::ActivateBento
        && (!m_host->allowsDesktopStageOnOutput(output)
            || !m_host->isManagedWindowForDesktopStage(window))) return false;
    const QString key = outputKey(output);
    std::optional<Session> candidate;
    QList<QPointer<KWin::EffectWindow>> unadopted;
    QList<PendingEviction> pending;
    if (intent != CardDropIntent::NativeDesktop
        && (sessionForOutput(output) || intent == CardDropIntent::ActivateBento)) {
        // CARD-LIFECYCLE.md §5: a full destination yields a pane to the arrival
        // rather than parking it. The yield is chosen while solving on a value
        // copy and only leaves once both owners have published, so a refusal
        // here — including this caller's own revalidation below — finds the
        // destination exactly as it was.
        const auto *existing = sessionForOutput(output);
        const Session live = existing ? *existing : Session{};
        candidate = prepareCardAdmission(window, output, geometry, restore, side,
                                         nullptr, &unadopted);
        if (!candidate) return false;
        if (existing) pending = captureEvictions(live, *candidate, window);
    }
    // Existing Bento takes priority. Its rejection must never become a native
    // desktop fallback. Neither source nor destination is mutated during solve.
    if (!commitSource()) return false;
    // Once the shortened plan is published the yielded pane has no owner until
    // card ownership takes it, and several paths below return early on an
    // invalidation. Tie the hand-over to publication rather than to reaching
    // the end of this function.
    bool publishedPlan = false;
    const auto handOver = qScopeGuard([&] { if (publishedPlan) publishEvictions(pending); });
    if (candidate) { m_sessions[key] = std::move(*candidate); publishedPlan = true; }
    const auto token = m_applicationGuard.issue();
    QPointer<KWin::EffectWindow> arrival = window;
    QPointer<KWin::Window> client = window->window();
    QPointer<KWin::LogicalOutput> target = output;
    releaseSource();
    const auto valid = [&] {
        return m_applicationGuard.accepts(token) && !m_restoring && arrival && !arrival->isDeleted()
            && client && arrival->window() == client && target && !m_retiredOutputs.contains(target)
            && KWin::effects->screens().contains(target.data())
            && !arrival->isUserMove() && !arrival->isUserResize();
    };
    if (!valid()) return true; // Logical commit happened; never replay source removal.
    if (candidate) {
        if (restore && restore->output != target && m_host->isTabletOutputForDesktopStage(target)) {
            // Committed ordinary monitor arrival into existing tablet Bento has
            // the same receiver-origin contract as Spread adoption (A1).
            // Ask KWin for tablet placement before overwriting it with a pane.
            client->sendToOutput(target);
            if (!valid()) return true;
            auto session = m_sessions.find(key);
            if (session == m_sessions.end()) return true;
            auto saved = std::find_if(session->snapshots.begin(), session->snapshots.end(),
                [&](const auto &s) { return s.window == arrival; });
            if (saved == session->snapshots.end()) return true;
            *saved = {placeOf(client.data(), client->moveResizeGeometry(), target.data()), arrival, true};
        }
        auto session = m_sessions.find(key);
        if (session != m_sessions.end() && applySession(session.value(), true)) scheduleSettle();
        // §5: the residents a first layout never adopted are taken as they
        // stand. The pane that yielded leaves through the scope guard above,
        // which runs on every exit that published the plan.
        for (const auto &resident : std::as_const(unadopted)) {
            if (!resident || resident->isDeleted() || !resident->window()) continue;
            if (parksOverflow(key)) {
                parkWindow(key, resident);
                continue;
            }
            m_host->admitTransferredWindowToTablet(resident, outputForKey(key), [] { return true; });
        }
    } else {
        if (!applyNativePlacement(client.data(), target.data(), geometry, valid)) return true;
        KWin::workspace()->raiseWindow(client);
        if (valid()) KWin::workspace()->activateWindow(client, true);
    }
    return true;
}

} // namespace Kadunce
