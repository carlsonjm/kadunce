/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Planning a display's layout, settling it, and putting it back.
#include "DesktopStageInternal.h"

namespace Kadunce
{

namespace
{
// A pane in a KWin zone sits where KWin rounds the zone's rectangle, which can
// differ from this rounding by a pixel. At a fractional scale a client sizes
// its frame in whole device pixels, so a pane outside a zone can also miss its
// rect by part of a pixel (504.571 for 504 at 1.75), and is still on it.
bool onPixel(const KWin::RectF &frame, const BentoPixelRect &pixel)
{
    return std::abs(frame.x() - pixel.x) <= 1.5 && std::abs(frame.y() - pixel.y) <= 1.5
        && std::abs(frame.width() - pixel.width) <= 1.5 && std::abs(frame.height() - pixel.height) <= 1.5;
}
}

bool DesktopStageController::activate(KWin::LogicalOutput *output,
                           KWin::EffectWindow *preferred, std::optional<BentoSidePlacement> side)
{
    if (m_restoring) return false;
    if (!output || !m_host->allowsDesktopStageOnOutput(output)
        || sessionForOutput(output)) {
        return false;
    }
    std::erase_if(m_restoredMinimizations, [output](const auto &pending) {
        return pending->output() == output || pending->result() != RestoredMinimization::Result::Pending;
    });
    if (m_host->isTabletOutputForDesktopStage(output)) {
        qInfo() << "Kadunce" << Revision
                << "using the tablet Desktop Stage fallback with no external display";
    }
    // Capture every retained card restore before releasing Card Stage. Native
    // configure acknowledgement may lag behind that release.
    QHash<KWin::EffectWindow *, NativeMoveSnapshot> cardRestores;
    const QList<QPointer<KWin::EffectWindow>> eligible =
        collectWindows(output, preferred);
    for (const auto &window : eligible) {
        if (const auto saved = m_host->activeRestoreForDesktopStage(window))
            cardRestores.insert(window, *saved);
    }
    if (eligible.isEmpty()) {
        qInfo() << "Kadunce" << Revision
                << "found no monitor windows for Bento on" << output->name();
        return false;
    }
    // CARD-LIFECYCLE.md §5: Bento takes the combination it can show and nothing
    // else. One solve on a throwaway value names what it cannot show, and it
    // happens before Card Stage is released: a batch found unshowable after
    // that release would leave the display with no owner at all.
    Session probe;
    probe.outputName = outputKey(output);
    for (const auto &window : eligible) probe.snapshots.append(makeSnapshot(window));
    if (side) { probe.side = side; probe.sideWindow = preferred; }
    QList<QPointer<KWin::EffectWindow>> unshowable;
    if (!planSession(probe, preferred, false, &unshowable)) return false;

    m_host->prepareOutputForDesktopStage(output);
    const QList<QPointer<KWin::EffectWindow>> owned =
        collectWindows(output, preferred);
    if (owned.isEmpty()) {
        qInfo() << "Kadunce" << Revision
                << "found no monitor windows for Bento on" << output->name();
        return false;
    }

    Session session;
    session.outputName = outputKey(output);
    QList<RestoreSnapshot> snapshots;
    for (const QPointer<KWin::EffectWindow> &window : owned) {
        // §5 gives a window this layout cannot show to card ownership instead,
        // so it is never adopted here and has nothing to give up if it stays.
        if (unshowable.contains(window)) continue;
        auto saved = makeSnapshot(window);
        const auto retained = cardRestores.constFind(window);
        const auto activeRestore = retained == cardRestores.cend()
            ? std::optional<NativeMoveSnapshot>() : std::optional<NativeMoveSnapshot>(*retained);
        if (activeRestore && window && window->window() == activeRestore->window) {
            // Restoration was requested, but the client may still report Active
            // bounds. Carry the source owner's record across this boundary,
            // with the display it was taken on, so a window that came from
            // another display goes back there when the layout lets it go.
            static_cast<WindowPlace &>(saved) = activeRestore->place();
        }
        snapshots.append(saved);
    }
    // Releasing Card Stage can change a window's current size, and the solve
    // ranks candidates by it, so ask again with the batch that is actually
    // being published. Shortening until it fits is what keeps a late change
    // from refusing here, which would leave the display with neither owner.
    for (int pass = 0; pass < snapshots.size(); ++pass) {
        Session recheck;
        recheck.outputName = session.outputName;
        recheck.snapshots = snapshots;
        if (side) { recheck.side = side; recheck.sideWindow = preferred; }
        QList<QPointer<KWin::EffectWindow>> late;
        if (!planSession(recheck, preferred, false, &late) || late.isEmpty()) break;
        for (const auto &window : std::as_const(late)) {
            if (!window || unshowable.contains(window)) continue;
            unshowable.append(window);
            snapshots.removeIf([&window](const auto &saved) { return saved.window == window; });
        }
    }
    if (snapshots.isEmpty()) return false;
    auto prepared = prepareBentoActivation(session, snapshots,
        QPointer<KWin::EffectWindow>(preferred), false,
        [this, side, preferred](Session &candidate, const auto &lead, bool required) {
            if (side) { candidate.side = side; candidate.sideWindow = preferred; }
            return reflowSession(candidate, lead, required);
        });
    if (!prepared) return false;
    // Publish only a complete plan. Callers must prepare their arrival/source
    // transaction before invoking this mutating entry point.
    m_sessions.insert(session.outputName, std::move(*prepared));
    Session &stored = m_sessions[session.outputName];
    if (applySession(stored, true)) scheduleSettle();
    qInfo() << "Kadunce" << Revision << "activated output-local Bento";
    // §5: every window the layout could not show becomes an awake individual
    // card on the display that can hold one. These were never adopted here, so
    // there is no source membership to give up; where the card stage held one
    // already, the release above restored its own record before this adopts it.
    // Where no display can hold a card, nothing leaves and they stay put.
    for (const auto &window : std::as_const(unshowable)) {
        if (!window || window->isDeleted() || !window->window()) continue;
        if (parksOverflow(session.outputName)) {
            parkWindow(session.outputName, window);
            continue;
        }
        // The record captured before Card Stage was released is the
        // authoritative one: a client may still report Active bounds after
        // that release, so reading its live geometry now would hand card
        // ownership the size Kadunce gave it rather than the one it began on.
        const auto saved = cardRestores.constFind(window.data());
        bool adopted = false;
        m_host->admitTransferredWindowToTablet(window, outputForKey(session.outputName),
            [&] { adopted = true; return true; },
            saved == cardRestores.cend() ? nullptr : &saved.value());
        if (!adopted) {
            qInfo() << "Kadunce" << Revision
                    << "left a window the layout cannot show where it is;"
                    << "no display can hold it as a card";
        }
    }
    return true;
}

bool DesktopStageController::reflowSession(Session &session,
                                KWin::EffectWindow *preferred, bool requirePreferred,
                                bool invalidateApplication,
                                QList<QPointer<KWin::EffectWindow>> *evicted)
{
    if (invalidateApplication) m_applicationGuard.invalidate();
    return planSession(session, preferred, requirePreferred, evicted);
}

bool DesktopStageController::planSession(Session &session,
    KWin::EffectWindow *preferred, bool requirePreferred,
    QList<QPointer<KWin::EffectWindow>> *evicted) const
{
    KWin::LogicalOutput *output = outputForKey(session.outputName);
    if (!output) {
        return false;
    }
    // CARD-LIFECYCLE.md §5: report every owned window the solve could not
    // place. Nothing retains it; the caller gives it to card ownership, and a
    // preview passes nullptr because a solve never moves an owner.
    const auto report = [evicted](const QList<QPointer<KWin::EffectWindow>> &windows) {
        if (!evicted) return;
        for (const auto &window : windows)
            if (window && !evicted->contains(window)) evicted->append(window);
    };
    QList<QPointer<KWin::EffectWindow>> owned;
    for (const RestoreSnapshot &snapshot : std::as_const(session.snapshots)) {
        if (snapshot.valid && !snapshot.userMinimized && !snapshot.parked && snapshot.window
            && !snapshot.window->isDeleted()
            && snapshot.window->window() && !owned.contains(snapshot.window)) {
            owned.append(snapshot.window);
        }
    }
    if (preferred && owned.removeAll(preferred) > 0) {
        owned.prepend(preferred);
    }
    // The display's own pane cap, which its sizing decides (DisplaySizing.h).
    const std::optional<int> cap = paneCap(output);
    // §11, Connected: the layout plays cards in by places, columns while every
    // card holds its minimum and laid out whole past them.
    const bool connected = m_host->connectedOutputForDesktopStage(output)
        && !m_host->showsOneCardForDesktopStage(output);
    // The snapped card is a Connected layout's, and only while it is shown.
    if (!connected || !owned.contains(session.snappedWindow)) {
        session.snappedWindow.clear();
        session.snappedSide.reset();
    }
    // A Connected snap that places its card makes it the snapped card.
    const auto noteSnapped = [&session] {
        if (!session.side || !session.side->connected || !session.sideWindow) return;
        session.snappedWindow = session.sideWindow;
        session.snappedSide = session.side;
    };
    // CARD-LIFECYCLE.md §11, Every display: a display sized Solo that holds
    // its own cards shows one of them, the preferred one first, whole; the
    // rest wait in Spread.
    if (m_host->showsOneCardForDesktopStage(output) && !owned.isEmpty()) {
        if (requirePreferred && preferred && owned.first() != preferred) return false;
        session.zones.clear();
        session.side.reset();
        session.sideWindow.clear();
        session.lone = false;
        session.windows = {owned.first()};
        session.rects = {BentoRect{0.0, 0.0, 1.0, 1.0}};
        report(owned.mid(1));
        return true;
    }
    // CARD-LIFECYCLE.md §11: a display without cards, or one sized Connected,
    // takes the zones drawn for it in KDE's tile editor, when there are any. A
    // window keeps the zone it has while it fits, and one that fits no zone
    // left waits as remainder: in the dock, or as a card in Spread.
    if (canTakeDrawnZones(session.outputName) && m_host->usesDrawnZonesForDesktopStage()) {
        const QList<DrawnZone> drawn = drawnZones(output);
        if (!drawn.isEmpty()) {
            const QList<QPointer<KWin::EffectWindow>> considered = owned.mid(0, 10);
            std::vector<BentoPixelRect> zonePixels;
            for (const auto &zone : drawn) zonePixels.push_back(pixelRect(zone.window));
            std::vector<BentoCandidate> candidates;
            std::vector<int> current;
            for (int index = 0; index < considered.size(); ++index) {
                KWin::EffectWindow *window = considered.at(index);
                const QSizeF minimum = window->window()->minSize();
                const KWin::RectF geometry = window->frameGeometry();
                candidates.push_back({minimum.width(), minimum.height(),
                                      geometry.width(), geometry.height(), index == 0});
                const int held = session.windows.indexOf(window);
                QObject *tile = held >= 0 && held < session.zones.size() ? session.zones.at(held).data() : nullptr;
                int zone = -1;
                for (int z = 0; tile && z < drawn.size(); ++z)
                    if (drawn.at(z).tile == tile) zone = z;
                current.push_back(zone);
            }
            const int required = requirePreferred ? int(considered.indexOf(preferred)) : -1;
            if (requirePreferred && required < 0) return false;
            auto admission = chooseZoneAdmission(candidates, zonePixels, current, required);
            if (!admission) return false;
            // A side snap takes the zone at that edge, where its card fits:
            // the card holding it trades zones, or gives way, as it fits.
            const int snapped = session.side && session.sideWindow
                ? int(considered.indexOf(session.sideWindow)) : -1;
            const int edge = snapped >= 0
                ? bentoPaneOnSide(zonePixels, session.side->right, session.side->large || session.side->over) : -1;
            const auto fits = [&](int candidate, int zone) {
                return candidates[candidate].minimumWidth <= zonePixels[zone].width + 1
                    && candidates[candidate].minimumHeight <= zonePixels[zone].height + 1;
            };
            if (edge >= 0 && fits(snapped, edge)) {
                auto &chosen = admission->candidateIndices;
                auto &zoned = admission->zoneIndices;
                const auto at = [](const std::vector<int> &list, int value) {
                    const auto it = std::find(list.cbegin(), list.cend(), value);
                    return it == list.cend() ? -1 : int(it - list.cbegin());
                };
                const int mine = at(chosen, snapped);
                const int holder = at(zoned, edge);
                if (mine >= 0 && holder >= 0 && mine != holder) {
                    if (fits(chosen[holder], zoned[mine])) std::swap(zoned[mine], zoned[holder]);
                    else { chosen.erase(chosen.begin() + holder); zoned.erase(zoned.begin() + holder);
                           zoned[holder < mine ? mine - 1 : mine] = edge; }
                } else if (mine >= 0 && holder < 0) {
                    zoned[mine] = edge;
                } else if (mine < 0 && holder >= 0) {
                    chosen[holder] = snapped;
                } else if (mine < 0) {
                    chosen.push_back(snapped);
                    zoned.push_back(edge);
                }
            }
            const KWin::Rect area = stageArea(output);
            session.windows.clear();
            session.zones.clear();
            session.rects.clear();
            for (std::size_t k = 0; k < admission->candidateIndices.size(); ++k) {
                const auto &zone = drawn.at(admission->zoneIndices.at(k));
                session.windows.append(considered.at(admission->candidateIndices.at(k)));
                session.zones.append(zone.tile);
                session.rects.push_back({(zone.window.x() - area.x()) / std::max(1, area.width()),
                                         (zone.window.y() - area.y()) / std::max(1, area.height()),
                                         zone.window.width() / std::max(1, area.width()),
                                         zone.window.height() / std::max(1, area.height())});
            }
            session.side.reset();
            session.sideWindow.clear();
            session.lone = false;
            QList<QPointer<KWin::EffectWindow>> remainder;
            for (const auto &window : std::as_const(owned))
                if (!session.windows.contains(window)) remainder.append(window);
            report(remainder);
            return true;
        }
    }
    // CARD-LIFECYCLE.md §11, Connected: a side snap names a place, not a size.
    // Beside is a full-height column at that edge, under the bottom of the
    // column there, and the layout it places leaves room open until arrivals
    // fill it.
    if (connected) {
        const KWin::Rect area = stageArea(output);
        std::optional<BentoPlacedColumns> columns = session.placed;
        if (columns) {
            bentoColumnsKeep(*columns, [&owned](const QPointer<KWin::EffectWindow> &window) {
                return window && owned.contains(window);
            });
            // Columns a pair began leave no room open (BentoColumns.h).
            if (columns->columns.empty() || (columns->paired && columns->columns.size() < 2))
                columns.reset();
        }
        const bool snapped = session.side && session.side->connected && session.sideWindow
            && preferred == session.sideWindow && owned.contains(session.sideWindow);
        const auto needOf = [this, output](const QPointer<KWin::EffectWindow> &window) {
            return window ? m_host->naturalNeedForDesktopStage(window.data(), output)
                                .value_or(NaturalNeed{}) : NaturalNeed{};
        };
        // §11, Connected: a card of a known kind arriving where panes stand
        // fits in among them at its natural size, as in columns placed.
        const QPointer<KWin::EffectWindow> naturalArrival = !snapped && preferred
                && owned.contains(preferred) && !session.windows.contains(preferred)
                && !session.windows.isEmpty() && needOf(QPointer(preferred)).width > 0.0
            ? QPointer(preferred) : nullptr;
        if ((snapped || naturalArrival) && !columns) {
            if (snapped && session.placeAlone) {
                columns = BentoPlacedColumns{};
            } else {
                // A layout already standing in columns takes the snap as a
                // place too; any other shape keeps the side grammar below.
                std::vector<QPointer<KWin::EffectWindow>> standing;
                std::vector<BentoRect> standingRects;
                bool whole = session.windows.size() == int(session.rects.size());
                for (int i = 0; whole && i < session.windows.size(); ++i) {
                    whole = owned.contains(session.windows.at(i));
                    standing.push_back(session.windows.at(i));
                    standingRects.push_back(session.rects[i]);
                }
                for (const auto &window : std::as_const(owned))
                    whole = whole && (window == (snapped ? session.sideWindow : naturalArrival)
                                      || session.windows.contains(window));
                if (whole) columns = bentoColumnsFromRects(standing, standingRects);
                // Any other shape is dealt into columns first, so a snap on a
                // Connected display always names a place.
                if (whole && !columns) columns = bentoColumnsDealt(standing);
            }
        }
        if (columns) {
            if (snapped) {
                bentoColumnsPlace(*columns, QPointer(session.sideWindow), *session.side);
                columns->paired = false;
                // Each column stands at the width its cards want, not an even
                // share; a width the wheel gave the snapped card stays.
                bentoColumnsSettleNatural(*columns, needOf, session.side->share > 0.0
                    ? QPointer(session.sideWindow) : QPointer<KWin::EffectWindow>());
            }
            // The shorter side of the smallest pane, when every pane holds
            // its card's minimum.
            const auto smallestSide = [&area](const BentoPlacedColumns &layout)
                -> std::optional<double> {
                std::vector<QPointer<KWin::EffectWindow>> held;
                std::vector<BentoRect> places;
                bentoColumnsLayout(layout, held, places);
                double smallest = std::numeric_limits<double>::max();
                for (std::size_t i = 0; i < held.size(); ++i) {
                    const double width = places[i].width * area.width();
                    const double height = places[i].height * area.height();
                    const QSizeF least = held[i]->window()->minSize();
                    if (least.width() > width + 1 || least.height() > height + 1) return std::nullopt;
                    smallest = std::min({smallest, width, height});
                }
                return smallest;
            };
            // §11, Connected: a card of a known kind fits in at its natural
            // size (NaturalSize.h); one that fits nowhere at the sizes the
            // others want is unshowable here, so the pane used longest ago
            // yields its room to it (shortenToShowable).
            int inUse = -1;
            long long latest = -1;
            for (std::size_t c = 0; c < columns->columns.size(); ++c)
                for (const auto &slot : columns->columns[c])
                    if (!slot.open && m_host->lastUsedForDesktopStage(slot.window.data()) > latest) {
                        latest = m_host->lastUsedForDesktopStage(slot.window.data());
                        inUse = int(c);
                    }
            QList<QPointer<KWin::EffectWindow>> unfitted;
            for (const auto &window : std::as_const(owned)) {
                if (bentoColumnsHold(*columns, window)) continue;
                if (const auto need = m_host->naturalNeedForDesktopStage(window.data(), output);
                    need && need->width > 0.0) {
                    if (bentoColumnsFitNatural(*columns, window, *need, needOf, inUse) == NaturalFit::None)
                        unfitted.append(window);
                    continue;
                }
                if (bentoColumnsFill(*columns, window)) continue;
                // Past the open room a card joins the column holding fewest or
                // starts a column at the right, whichever leaves the smallest
                // pane largest.
                auto joined = *columns;
                bentoColumnsJoin(joined, window);
                auto widened = *columns;
                widened.columns.push_back({{window, false}});
                const auto joinedSide = smallestSide(joined);
                const auto widenedSide = smallestSide(widened);
                *columns = widenedSide && (!joinedSide || *widenedSide > *joinedSide)
                    ? widened : joined;
                // Four cards stand in even quarters, two columns of two in
                // the order they stood, where every quarter holds its card.
                if (auto quarters = bentoColumnsQuartered(*columns);
                    !columns->natural && quarters && smallestSide(*quarters))
                    *columns = *quarters;
            }
            std::vector<QPointer<KWin::EffectWindow>> handles;
            std::vector<BentoRect> rects;
            const auto holdsMinimums = [&] {
                bentoColumnsLayout(*columns, handles, rects);
                return smallestSide(*columns).has_value();
            };
            const bool fits = holdsMinimums();
            if (fits) {
                // The same columns keep the places dividers gave them.
                const bool same = session.placed && *session.placed == *columns
                    && session.rects.size() == rects.size()
                    && std::equal(handles.cbegin(), handles.cend(), session.windows.cbegin(),
                                  session.windows.cend());
                session.placed = columns;
                session.windows = QList<QPointer<KWin::EffectWindow>>(handles.cbegin(), handles.cend());
                if (!same) session.rects = rects;
                session.zones.clear();
                if (snapped) noteSnapped();
                session.side.reset();
                session.sideWindow.clear();
                session.lone = false;
                session.placeAlone = false;
                report(unfitted);
                return true;
            }
            // Columns that cannot hold every card at its minimum give way to
            // the whole layout laid out again, a snapped card kept at its
            // side; only a card even that cannot hold goes to Spread.
        }
        session.placed.reset();
    }
    session.placeAlone = false;
    session.zones.clear();
    // §11, Connected: laid out whole, the layout keeps the snapped card at the
    // side it was snapped to, as a column of its own there, whatever arrived
    // since. It is held at that side, never split into another column.
    std::optional<BentoSidePlacement> side = session.side;
    QPointer<KWin::EffectWindow> sideWindow = session.sideWindow;
    const bool kept = connected && !(side && owned.contains(sideWindow))
        && session.snappedWindow && session.snappedSide;
    if (kept) {
        side = session.snappedSide;
        sideWindow = session.snappedWindow;
    }
    if (side && owned.contains(sideWindow)) {
        // Existing edge occupancy outranks a new whole-display side preset.
        // This operates on the same value plan for preview and commit.
        auto splitWindows = session.windows;
        const int existingIndex = splitWindows.indexOf(sideWindow);
        if (existingIndex < 0) splitWindows.append(sideWindow);
        std::vector<BentoCandidate> splitCandidates;
        bool validSplitInputs = true;
        for (const auto &w : splitWindows) {
            if (!owned.contains(w)) { validSplitInputs = false; break; }
            const auto minimum = w->window()->minSize();
            splitCandidates.push_back({minimum.width(),minimum.height(),1,1,false});
        }
        const auto splitArea = stageArea(output);
        // §11, Connected: the middle of an edge puts the card beside, in a
        // column of its own, so only its top and bottom quarters split one.
        const bool beside = side->connected && side->large;
        // Over splits the column with the card in its upper part, as the
        // larger placement does on a display that is not Connected.
        BentoSidePlacement splitSide = *side;
        if (splitSide.over) splitSide.large = true;
        const auto split = validSplitInputs && !beside && !kept ? splitBentoColumn(session.rects,
            splitCandidates, existingIndex < 0 ? session.windows.size() : existingIndex, splitSide,
            splitArea.width(), splitArea.height(), cap)
            : std::nullopt;
        if (split && (!requirePreferred || splitWindows.contains(preferred))) {
            session.windows = splitWindows;
            session.rects = *split;
            QList<QPointer<KWin::EffectWindow>> remainder = owned;
            for (const auto &w : splitWindows) remainder.removeAll(w);
            report(remainder);
            noteSnapped();
            session.side.reset(); session.sideWindow.clear();
            return true;
        }
        owned.removeAll(sideWindow);
        owned.prepend(sideWindow);
        const auto candidate = [](KWin::EffectWindow *w) {
            const auto minimum = w->window()->minSize();
            return BentoCandidate{minimum.width(), minimum.height(), 1, 1, false};
        };
        const auto area = stageArea(output);
        std::vector<BentoCandidate> candidates;
        for (int i = 0; i < std::min(BentoPaneBound, int(owned.size())); ++i)
            candidates.push_back(candidate(owned[i]));
        const auto admission = chooseBentoSideAdmission(*side, area.width(), area.height(),
            candidates, requirePreferred ? owned.indexOf(preferred) : 0, cap);
        if (!admission) return false;
        if (!kept) noteSnapped();
        if (owned.size() > 1) session.lone = false;
        else if (session.windows.isEmpty() && parksOverflow(session.outputName)) session.lone = true;
        session.windows.clear();
        QList<QPointer<KWin::EffectWindow>> remainder = owned;
        for (const int index : admission->candidateIndices) {
            session.windows.append(owned[index]);
            remainder.removeAll(owned[index]);
        }
        session.rects = admission->rects;
        if (session.lone && session.windows.size() == 1) {
            const auto minimum = session.windows.first()->window()->minSize();
            if (minimum.width() <= area.width() / 2.0)
                session.rects = {side->right ? BentoRect{0.5, 0.0, 0.5, 1.0}
                                             : BentoRect{0.0, 0.0, 0.5, 1.0}};
        }
        report(remainder);
        return true;
    }
    // The curated library reads its eight panes and two alternates; past it an
    // even grid reads the rest, so minimum sizes decide how many panes a large
    // display shows. BentoPaneBound keeps that finite, and how many this
    // display shows is the display's pane cap, which the edge path reads too.
    // On Connected, where columns share the width evenly, two panes may also
    // stand in even halves side by side (BentoLayout.h).
    const QList<QPointer<KWin::EffectWindow>> considered = owned.mid(0, BentoPaneBound);
    const KWin::Rect area = stageArea(output);
    std::vector<BentoCandidate> candidates;
    candidates.reserve(considered.size());
    for (int index = 0; index < considered.size(); ++index) {
        KWin::EffectWindow *window = considered.at(index);
        const QSizeF minimum = window->window()->minSize();
        const KWin::RectF geometry = window->frameGeometry();
        candidates.push_back({minimum.width(), minimum.height(),
                              geometry.width(), geometry.height(),
                              index == 0});
    }
    const auto required = requirePreferred
        ? chooseBentoTransferAdmission(candidates, considered.indexOf(preferred),
            area.width(), area.height(), BentoPaneBound, cap, connected)
        : std::optional<BentoAdmission>{};
    if (requirePreferred && !required) return false;
    const BentoAdmission admission = required ? *required : chooseBentoAdmission(
        candidates, area.width(), area.height(), BentoPaneBound, cap, connected);
    session.windows.clear();
    session.rects = admission.rects;
    QSet<KWin::EffectWindow *> admitted;
    for (const int index : admission.candidateIndices) {
        if (index >= 0 && index < considered.size()) {
            session.windows.append(considered.at(index));
            admitted.insert(considered.at(index));
        }
    }
    QList<QPointer<KWin::EffectWindow>> remainder;
    for (const QPointer<KWin::EffectWindow> &window : owned) {
        if (!admitted.contains(window)) remainder.append(window);
    }
    report(remainder);
    return true;
}

bool DesktopStageController::evictToTablet(const QString &sourceKey,
                                           KWin::EffectWindow *window,
                                           const std::function<bool()> &sourceValid,
                                           EvictedAs destination)
{
    if (m_restoring || !window || window->isDeleted() || !window->window()
        || window->isUserMove() || window->isUserResize()
        || !m_sessions.contains(sourceKey)) return false;
    // A live carry's source identity is stamped with the application guard's
    // generation, so every advance of it reads as a carry that has gone stale.
    // Read the carry first, before this eviction touches the guard at all, or
    // it invalidates the very carry it is committing and refuses its own
    // window. The token below covers the rest of the span: anything that
    // advances the generation between here and commit fails accepts(token)
    // where a re-read of the carry would have failed.
    const bool carrySourceHeld = !sourceValid || sourceValid();
    // Prove the source can give the window up before the destination is asked.
    // The departure re-plans on a value copy; nothing is published by it, so it
    // does not invalidate anything either. Planning is not a workspace change.
    const auto departure = prepareBentoDeparture(m_sessions.value(sourceKey),
        QPointer<KWin::EffectWindow>(window),
        [this](Session &session, const auto &preferred, bool required) {
            return reflowSession(session, preferred, required, false);
        });
    if (!departure) return false;
    QPointer<KWin::EffectWindow> arrival = window;
    const auto token = m_applicationGuard.issue();
    bool committed = false;
    // The awake door reads this window's authoritative pre-Bento record for
    // itself while it is still a pane, so the card it becomes still releases to
    // the display and geometry it began on. It reads it by preparing a carry,
    // which refuses a minimized window, so the sleeping door is handed the same
    // record out of the session that still holds it. Without it the state §13
    // restores the window to would be the one the user just asked for, and
    // release would re-minimize a window it had woken.
    std::optional<NativeMoveSnapshot> sleepingRecord;
    if (destination != EvictedAs::AwakeCard) {
        const auto session = m_sessions.constFind(sourceKey);
        const auto saved = std::find_if(session->snapshots.cbegin(),
            session->snapshots.cend(),
            [window](const auto &s) { return s.window == window && s.valid; });
        if (saved == session->snapshots.cend()) return false;
        sleepingRecord = NativeMoveSnapshot{*saved, window->window(), outputForKey(saved->outputName)};
    }
    const auto admit = [&](KWin::EffectWindow *departing, const std::function<bool()> &commit) {
        KWin::LogicalOutput *from = outputForKey(sourceKey);
        if (destination == EvictedAs::HomedCard)
            return m_host->admitDisplacedPaneToTablet(departing, from, commit, &*sleepingRecord);
        return destination == EvictedAs::SleepingCard
            ? m_host->admitSleepingPaneToTablet(departing, from, commit, &*sleepingRecord)
            : m_host->admitTransferredWindowToTablet(departing, from, commit);
    };
    const bool accepted = carrySourceHeld && admit(window, [&] {
        if (committed || !m_applicationGuard.accepts(token) || m_restoring
            || !arrival || arrival->isDeleted() || !arrival->window()
            || arrival->isUserMove() || arrival->isUserResize()
            || !m_sessions.contains(sourceKey)) return false;
        if (departure->snapshots.isEmpty()) m_sessions.remove(sourceKey);
        else m_sessions[sourceKey] = *departure;
        committed = true;
        return true;
    });
    // §11, Connected: a placed card that was the layout's last pane leaves the
    // display presenting none.
    if (committed && departure->snapshots.isEmpty() && departure->placed)
        if (KWin::LogicalOutput *output = outputForKey(sourceKey))
            m_host->retireOutputFromDesktopStage(output);
    // Acceptance is permission to commit, not proof of placement. Never report
    // failure after the source has published, or a caller replays a stale plan.
    return committed && accepted;
}

bool DesktopStageController::canPlaceEvictedCard() const
{
    return m_host->tabletOutputForDesktopStage() != nullptr;
}

bool DesktopStageController::parksOverflow(const QString &key) const
{
    KWin::LogicalOutput *output = outputForKey(key);
    return output && !m_host->outputCanOwnCards(output);
}

bool DesktopStageController::canTakeDrawnZones(const QString &key) const
{
    KWin::LogicalOutput *output = outputForKey(key);
    return output && (!m_host->outputCanOwnCards(output)
        || (m_host->connectedOutputForDesktopStage(output)
            && !m_host->showsOneCardForDesktopStage(output)));
}

void DesktopStageController::parkWindow(const QString &key, KWin::EffectWindow *window,
                                        const RestoreSnapshot *record)
{
    auto session = m_sessions.find(key);
    if (session == m_sessions.end() || !window || window->isDeleted() || !window->window())
        return;
    auto saved = std::find_if(session->snapshots.begin(), session->snapshots.end(),
        [window](const auto &s) { return s.window == window; });
    if (saved == session->snapshots.end()) {
        session->snapshots.append(record ? *record : makeSnapshot(window));
        saved = std::prev(session->snapshots.end());
    }
    saved->parked = true;
    saved->userMinimized = false;
    const int index = session->windows.indexOf(window);
    if (index >= 0) {
        if (index < static_cast<int>(session->rects.size()))
            session->rects.erase(session->rects.begin() + index);
        if (index < session->zones.size()) session->zones.removeAt(index);
        session->windows.removeAt(index);
    }
    minimizeParked(key);
}

void DesktopStageController::minimizeParked(const QString &key)
{
    const auto session = m_sessions.constFind(key);
    if (session == m_sessions.cend()) return;
    QList<QPointer<KWin::EffectWindow>> pending;
    for (const auto &saved : session->snapshots) {
        if (saved.parked && saved.window && !saved.window->isDeleted()
            && saved.window->window() && !saved.window->isMinimized())
            pending.append(saved.window);
    }
    // Kadunce's own minimize is not the person's, so the handler that turns a
    // minimize into a participation change must not see it.
    const bool wasParking = std::exchange(m_parking, true);
    for (const auto &window : std::as_const(pending))
        if (window && window->window()) window->window()->setMinimized(true);
    m_parking = wasParking;
}

QList<DesktopStageController::PendingEviction> DesktopStageController::captureEvictions(
    const Session &live, const Session &published, KWin::EffectWindow *arrival) const
{
    QList<PendingEviction> pending;
    for (const auto &saved : live.snapshots) {
        if (!saved.valid || !saved.window) continue;
        if (std::any_of(published.snapshots.cbegin(), published.snapshots.cend(),
                [&saved](const auto &kept) { return kept.window == saved.window; })) continue;
        pending.append({saved.window,
            NativeMoveSnapshot{saved, saved.window->window(), outputForKey(saved.outputName)},
            live.outputName, saved, arrival});
    }
    return pending;
}

void DesktopStageController::publishEvictions(const QList<PendingEviction> &pending)
{
    for (const auto &eviction : pending) {
        const auto window = eviction.window;
        if (!window || window->isDeleted() || !window->window()) continue;
        if (parksOverflow(eviction.sourceKey)) {
            parkWindow(eviction.sourceKey, window, &eviction.snapshot);
            continue;
        }
        // The plan that stopped naming this window is already published, so
        // there is no source membership left to give up and nothing that a
        // refusal here could roll back. The record travels with it because the
        // session that held it no longer does.
        // §5 makes this a nonselected card, never the Active one: the display
        // is still presenting the panes this window just left.
        const QRectF drawn = m_host->bentoPresentationRect(window);
        if (!m_host->admitDisplacedPaneToTablet(window, outputForKey(eviction.sourceKey),
                [] { return true; }, &eviction.record)) {
            qWarning() << "Kadunce" << Revision
                       << "could not give a window the layout cannot show to card ownership:"
                       << logNameOf(window);
        } else if (auto *output = outputForKey(eviction.sourceKey)) {
            m_host->paneYieldedForDesktopStage(window, output, drawn);
            // A pane pushed out in its turn no longer stands where it was
            // taken in, so what it pushed out waits for nothing it does.
            m_displaced.removeIf([&window](const auto &held) {
                return held.yielded == window || held.arrival == window;
            });
            if (eviction.arrival && eviction.arrival != window)
                m_displaced.append({eviction.sourceKey, eviction.arrival, window});
        }
    }
}

void DesktopStageController::returnDisplacedCards(const QString &key)
{
    if (m_restoring || m_interactionWindow || m_railDrag || m_displaced.isEmpty()) return;
    const auto session = m_sessions.constFind(key);
    if (session != m_sessions.cend() && session->applying) return;
    // A record outlives neither its layout nor its card, nor a card that is a
    // pane again; one whose arrival still stands in the layout waits.
    QList<Displacement> due;
    m_displaced.removeIf([&](const Displacement &held) {
        if (held.outputName != key) return false;
        if (session == m_sessions.cend() || !held.yielded || held.yielded->isDeleted()
            || session->windows.contains(held.yielded)) return true;
        if (held.arrival && !held.arrival->isDeleted() && session->windows.contains(held.arrival))
            return false;
        due.append(held);
        return true;
    });
    for (const auto &held : std::as_const(due)) {
        // Taking a card in reenters this controller, so the layout is found
        // again each time.
        KWin::LogicalOutput *output = outputForKey(key);
        if (!output || !m_sessions.contains(key)) return;
        if (held.yielded && !held.yielded->isDeleted())
            m_host->displacedCardMayReturnForDesktopStage(held.yielded, output);
    }
}

bool DesktopStageController::shortenToShowable(Session &session, const RestoreSnapshot &arrival,
    std::optional<BentoSidePlacement> side, KWin::EffectWindow *sideWindow) const
{
    const auto plan = [&](const Session &base, QList<QPointer<KWin::EffectWindow>> *unshowable) {
        Session probe = base;
        probe.snapshots.append(arrival);
        if (side) { probe.side = side; probe.sideWindow = sideWindow; }
        return planSession(probe, arrival.window, true, unshowable);
    };
    QList<QPointer<KWin::EffectWindow>> unshowable;
    if (!plan(session, &unshowable) || unshowable.isEmpty())
        return true;
    // §5: the yield needs somewhere to go. Where nothing can hold it, nothing
    // leaves, and refusing here is what keeps the combination the layout has.
    if (!canPlaceEvictedCard() && !parksOverflow(session.outputName)) return false;
    const auto yield = [&session](const QPointer<KWin::EffectWindow> &yielding) {
        const int index = session.windows.indexOf(yielding);
        if (index >= 0 && index < static_cast<int>(session.rects.size()))
            session.rects.erase(session.rects.begin() + index);
        if (index >= 0 && index < session.zones.size()) session.zones.removeAt(index);
        session.windows.removeAll(yielding);
        session.snapshots.removeIf(
            [&yielding](const auto &saved) { return saved.window == yielding; });
    };
    // §5: a side release displaces the pane occupying the side it was released
    // into, which is the pane the preview covers while dragging. The solve
    // decides nothing about that pane; it names only what the arrival still
    // leaves unshowable, as it does for an arrival that names no side.
    KWin::LogicalOutput *output = outputForKey(session.outputName);
    // §11, Connected: the snap names a place among columns, and only what
    // the solve cannot hold leaves.
    if (side && !side->connected && output) {
        const auto pixels = sessionPixels(session, stageArea(output));
        const int index = pixels.size() == std::size_t(session.windows.size())
            ? bentoPaneOnSide(pixels, side->right, side->large) : -1;
        const QPointer<KWin::EffectWindow> occupant =
            index >= 0 ? session.windows.at(index) : nullptr;
        if (occupant && occupant != arrival.window) {
            yield(occupant);
            unshowable.clear();
            if (!plan(session, &unshowable)) return true;
        }
    }
    // §11, Connected: a full layout gives up the pane used longest ago, as it
    // does for a pick or a window opening there, not the one the solve drops.
    if (output && m_host->connectedOutputForDesktopStage(output) && (!side || side->connected)) {
        for (int left = int(session.windows.size()); left > 0 && !unshowable.isEmpty(); --left) {
            const auto slot = slotForArrival(session, arrival.window.data());
            const QPointer<KWin::EffectWindow> occupant = slot ? session.windows.at(*slot) : nullptr;
            if (!occupant || occupant == arrival.window) break;
            yield(occupant);
            unshowable.clear();
            if (!plan(session, &unshowable)) return true;
        }
    }
    for (const auto &yielding : std::as_const(unshowable)) {
        if (!yielding || yielding == arrival.window) continue;
        yield(yielding);
    }
    return true;
}

void DesktopStageController::settleParticipation(const QString &key)
{
    if (!m_sessions.contains(key) || !m_sessions.value(key).participationDirty) return;
    // §5: a wake can ask for a pane the layout has no room for. Shedding uses
    // the live session because nothing else is in flight here, and it is
    // re-found by key afterwards because adopting reenters this controller.
    shedUnshowable(key, m_sessions.value(key), nullptr, false);
    auto session = m_sessions.find(key);
    if (session == m_sessions.end()) return;
    QList<QPointer<KWin::EffectWindow>> unshowable;
    if (!reflowSession(session.value(), nullptr, false, true, &unshowable)) return;
    session->participationDirty = false;
    // §5: shedding can leave one visible pane, and Bento ends there.
    endLayoutIntoCardOwnership(key);
}

bool DesktopStageController::settleAfterInteraction(const QString &key)
{
    const auto session = m_sessions.constFind(key);
    if (session != m_sessions.cend() && session->participationDirty) {
        QList<QPointer<KWin::EffectWindow>> sleeping;
        for (const auto &saved : std::as_const(session->snapshots))
            if (saved.userMinimized && saved.window && saved.window->isMinimized()) sleeping.append(saved.window);
        settleParticipation(key);
        // As handleWindowMinimizedChanged gives a minimized pane up, except
        // where the display keeps it waiting in the dock.
        for (const auto &window : std::as_const(sleeping)) {
            const auto held = m_sessions.constFind(key);
            if (!window || held == m_sessions.cend() || parksOverflow(key)) break;
            if (std::none_of(held->snapshots.cbegin(), held->snapshots.cend(),
                    [&window](const auto &saved) { return saved.window == window; })) continue;
            (void)evictToTablet(key, window, {}, EvictedAs::SleepingCard);
        }
    }
    // A departure held on any display is asked about again, this one too.
    const QSet<QString> held = std::exchange(m_heldDepartures, {});
    for (const QString &departed : held) endLayoutIntoCardOwnership(departed);
    return !m_sessions.contains(key);
}

bool DesktopStageController::layoutEndsIntoCards(const Session &session) const
{
    if (!bentoEndsAtOnePane(session)) return false;
    // §11, Connected: a placed card keeps its place, and its room stays open.
    // Columns a pair began leave no room, so their last pane ends as §5 says.
    if (session.placed && !session.placed->paired) return false;
    KWin::LogicalOutput *output = outputForKey(session.outputName);
    // CARD-LIFECYCLE.md §5 hands the remaining pane to card ownership. Where
    // the display cannot hold one there is nothing for Bento to end into, so
    // the session keeps its combination rather than returning it to Plasma:
    // §13 reserves that for release and disable.
    if (!output || !m_host->outputCanOwnCards(output)) return false;
    // §11, Every display: a display holding its own cards shows its last card
    // whole, as its layout of one.
    return !m_host->holdsOwnCardsForDesktopStage(output);
}

void DesktopStageController::endLayoutIntoCardOwnership(const QString &key)
{
    if (m_restoring) return;
    if (m_interactionWindow || m_railDrag) {
        m_heldDepartures.insert(key);
        return;
    }
    // Every departure from a layout asks here whether it ends, so it is also
    // where a card the departing pane pushed out comes back, before a layout
    // that would fall to one pane without it is ended.
    returnDisplacedCards(key);
    const auto session = m_sessions.constFind(key);
    if (session == m_sessions.cend() || session->applying
        || !layoutEndsIntoCards(session.value())) return;
    KWin::LogicalOutput *output = outputForKey(key);
    const QPointer<KWin::EffectWindow> remaining =
        session->windows.isEmpty() ? nullptr : session->windows.first();
    const QRectF drawn = remaining ? m_host->bentoPresentationRect(remaining) : QRectF();
    if (remaining && !evictToTablet(key, remaining)) return;
    // Eviction removes the session as it empties, and adopting reenters this
    // controller, so anything found under the key now was put there after the
    // departure and is not the layout that ended.
    const auto ended = m_sessions.constFind(key);
    if (ended != m_sessions.cend()) {
        if (!ended->snapshots.isEmpty()) return;
        m_sessions.remove(key);
    }
    // The display no longer has a layout for anything else to present behind.
    m_host->retireOutputFromDesktopStage(output);
    if (remaining && !remaining->isDeleted())
        m_host->lastPaneLeftLayoutForDesktopStage(remaining, output, drawn);
    KWin::effects->addRepaintFull();
}

bool DesktopStageController::extractPaneToCards(KWin::EffectWindow *window,
    const std::function<bool()> &sourceValid, bool waitBehind)
{
    if (m_restoring || !window) return false;
    QString sourceKey;
    for (auto it = m_sessions.cbegin(); it != m_sessions.cend(); ++it) {
        if (std::any_of(it->snapshots.cbegin(), it->snapshots.cend(),
                [window](const auto &saved) { return saved.window == window; })) {
            sourceKey = it.key();
            break;
        }
    }
    KWin::LogicalOutput *output = outputForKey(sourceKey);
    if (sourceKey.isEmpty() || !output || !m_host->outputCanOwnCards(output)) return false;
    // The departure is prepared and published by the same eviction §5 uses for
    // a window a layout cannot show, so a refused extraction leaves the layout
    // exactly as it was and an accepted one carries the pre-Bento record.
    // §8: nothing is shown on top of live panes, so a pane leaving a layout
    // that goes on presenting one waits behind it, however few are left.
    Session rest = m_sessions.value(sourceKey);
    rest.windows.removeAll(window);
    rest.snapshots.removeIf([window](const auto &saved) { return saved.window == window; });
    const bool outlasts = waitBehind && !rest.windows.isEmpty() && !layoutEndsIntoCards(rest);
    if (!evictToTablet(sourceKey, window, sourceValid,
            outlasts ? EvictedAs::HomedCard : EvictedAs::AwakeCard)) return false;
    // §5 may end what is left before it is laid out again, so only a layout
    // that survives the departure is asked to place its panes.
    endLayoutIntoCardOwnership(sourceKey);
    auto shortened = m_sessions.find(sourceKey);
    if (shortened != m_sessions.end() && applySession(shortened.value(), false))
        scheduleSettle();
    return true;
}

bool DesktopStageController::yieldPaneToSpread(KWin::EffectWindow *window)
{
    if (m_restoring || !window || window->isMinimized()) return false;
    QString sourceKey;
    for (auto it = m_sessions.cbegin(); it != m_sessions.cend(); ++it) {
        if (it->windows.contains(window)) {
            sourceKey = it.key();
            break;
        }
    }
    KWin::LogicalOutput *output = outputForKey(sourceKey);
    if (sourceKey.isEmpty() || !output || !m_host->holdsOwnCardsForDesktopStage(output)
        || !evictToTablet(sourceKey, window, {}, EvictedAs::HomedCard)) return false;
    returnDisplacedCards(sourceKey);
    auto shortened = m_sessions.find(sourceKey);
    if (shortened != m_sessions.end() && applySession(shortened.value(), false))
        scheduleSettle();
    return true;
}

bool DesktopStageController::shedUnshowable(const QString &key, Session probe,
    KWin::EffectWindow *preferred, bool requirePreferred)
{
    QList<QPointer<KWin::EffectWindow>> unshowable;
    if (!planSession(probe, preferred, requirePreferred, &unshowable)) return false;
    for (const auto &window : std::as_const(unshowable)) {
        if (!window) continue;
        // Re-found by key on every pass: adopting reenters this controller and
        // can remove, replace or repopulate the session between evictions.
        const auto session = m_sessions.constFind(key);
        if (session == m_sessions.cend()) return false;
        const auto &snapshots = session->snapshots;
        // An arrival the caller has not published yet owns nothing here, so
        // there is nothing for it to leave; only a window this session holds
        // can be handed on.
        if (std::none_of(snapshots.cbegin(), snapshots.cend(),
                [&window](const auto &saved) { return saved.window == window; })) continue;
        if (parksOverflow(key)) {
            parkWindow(key, window);
            continue;
        }
        if (!evictToTablet(key, window)) return false;
    }
    return true;
}

bool DesktopStageController::applySession(Session &session, bool activateLead,
                                          KWin::EffectWindow *focus)
{
    if (session.participationDirty) {
        // §7's participation change is settled by whoever observed it, because
        // it can owe an eviction and this is already committed to placing
        // panes. Re-solving here is a last resort, and it says so.
        qWarning() << "Kadunce" << Revision << "publishing" << session.outputName
                   << "with an unsettled participation change";
        if (!reflowSession(session)) return false;
        session.participationDirty = false;
    }
    const Session plan = session;
    const QString key = plan.outputName;
    QPointer<KWin::LogicalOutput> output = outputForKey(key);
    if (!output) return false;
    // CARD-LIFECYCLE.md §5 and §14: a session shows every window it owns awake.
    // A snapshot with no pane and no §7 sleep is the state the deleted overflow
    // container used to hold, and `owned` is rebuilt from snapshots on every
    // solve, so it would come back forever while appearing nowhere in the
    // ownership view. Report it here rather than let it go unnamed.
    for (const auto &saved : plan.snapshots) {
        if (!saved.valid || saved.userMinimized || saved.parked
            || plan.windows.contains(saved.window))
            continue;
        qWarning() << "Kadunce" << Revision << "Bento on" << key
                   << "owns a window it neither shows nor put to sleep";
        break;
    }
    std::erase_if(m_restoredMinimizations, [output](const auto &pending) {
        return pending->output() == output || pending->result() != RestoredMinimization::Result::Pending;
    });
    const quint64 token = m_applicationGuard.issue();
    session.applying = true;
    session.applicationToken = token;
    const auto cleanup = qScopeGuard([&] {
        auto current = m_sessions.find(key);
        if (current != m_sessions.end() && current->applicationToken == token)
            current->applying = false;
    });
    const auto current = [&] {
        return m_applicationGuard.accepts(token) && output
            && outputForKey(key) == output && m_sessions.contains(key);
    };
    const KWin::Rect area = stageArea(output);
    const std::vector<BentoPixelRect> pixels = sessionPixels(plan, area);
    QList<QRectF> motionFrom, motionTo;
    for (int i = 0; i < plan.windows.size() && i < int(pixels.size()); ++i) {
        motionFrom.append(m_host->bentoPresentationRect(plan.windows[i]));
        const auto &p = pixels[i];
        motionTo.append(QRectF(p.x,p.y,p.width,p.height));
    }
    for (int index = 0;
         index < plan.windows.size()
            && index < static_cast<int>(pixels.size());
         ++index) {
        QPointer<KWin::EffectWindow> window = plan.windows.at(index);
        if (!current()) return false;
        if (!window || window->isDeleted() || !window->window()) {
            continue;
        }
        QPointer<KWin::Window> client = window->window();
        const auto valid = [&] {
            return current() && window && !window->isDeleted() && client
                && window->window() == client && !window->isUserMove() && !window->isUserResize();
        };
        if (!valid()) return false;
        const BentoPixelRect &pixel = pixels.at(index);
        const KWin::RectF target(pixel.x, pixel.y, pixel.width, pixel.height);
        if (!applyNativePlacement(client.data(), output.data(), target, valid)) return false;
        // KWin's zone takes the pane from here: its geometry, and the edges
        // it shares with the panes beside it.
        if (index < plan.zones.size() && plan.zones.at(index) && valid()
            && placeInZone(plan.zones.at(index), client.data()) && !m_zoned.contains(client))
            m_zoned.append(client);
    }
    releaseStrayZones();
    const QPointer<KWin::EffectWindow> lead = focus && plan.windows.contains(focus)
        ? QPointer<KWin::EffectWindow>(focus) : plan.windows.value(0);
    if (activateLead && current() && lead && !lead->isDeleted() && lead->window()) {
        KWin::workspace()->raiseWindow(lead->window());
        if (!current() || !lead || lead->isDeleted() || !lead->window()) return false;
        KWin::workspace()->activateWindow(lead->window(), true);
    }
    if (!current()) return false;
    m_host->animateBentoLayout(output, plan.windows, motionFrom, motionTo);
    KWin::effects->addRepaintFull();
    return true;
}

void DesktopStageController::scheduleSettle()
{
    if (m_sessions.isEmpty() || m_interactionWindow) return;
    m_settleTimer.start();
}

void DesktopStageController::settleSessions()
{
    if (m_interactionWindow) {
        return;
    }
    const QStringList keys = m_sessions.keys();
    // Placement is requested once. Observe asynchronous results without issuing
    // more resizes; reconcile unresolved sessions once after the bounded grace.
    for (const QString &key : keys) {
        if (m_sessions.contains(key) && m_sessions.value(key).participationDirty) {
            // CARD-LIFECYCLE.md §5: a wake deferred past an interaction can ask
            // for a pane the layout has no room for, so settle before applying.
            settleParticipation(key);
            auto pending = m_sessions.find(key);
            if (pending != m_sessions.end() && applySession(pending.value(), false))
                scheduleSettle();
            continue; // Give the new configure its own acknowledgement grace.
        }
        const auto session = m_sessions.constFind(key);
        if (session == m_sessions.cend() || sessionGeometryMatches(session.value())) continue;
        // A client can change its own frame while the grace runs, so the first
        // unsettled reading asks for the layout once more instead of deciding
        // on it. The token the session was published with bounds that to one
        // retry per placement, and a later placement earns its own.
        auto pending = m_sessions.find(key);
        if (pending == m_sessions.end()) continue;
        if (m_settleRetries.value(key) != pending->applicationToken) {
            if (applySession(pending.value(), false)) {
                const auto placed = m_sessions.constFind(key);
                if (placed != m_sessions.cend())
                    m_settleRetries.insert(key, placed->applicationToken);
                scheduleSettle();
            }
            continue;
        }
        shedUnsettledPanes(key);
    }
}

// CARD-LIFECYCLE.md §5: a window the layout cannot show leaves it for card
// ownership, and a pane that will not fit its rect twice is one. The panes
// that did settle keep theirs, and §5 ends a layout that falls to one. §13
// reserves the native desktop for release and disable, so a settle never
// returns a session to Plasma however little of it arrived. Where no display
// can own a card nothing leaves, which is §5's own answer, and the layout
// keeps the combination it has. A pane that stops short of its rect, as a
// terminal keeping whole rows does or an application slow to wake answering
// late, covers nothing beside it: it stays, short of its place.
void DesktopStageController::shedUnsettledPanes(const QString &key)
{
    const auto session = m_sessions.constFind(key);
    auto *output = outputForKey(key);
    if (session == m_sessions.cend() || session->applying || !output
        || session->windows.size() != static_cast<int>(session->rects.size())) return;
    const auto area = stageArea(output);
    const auto pixels = sessionPixels(*session, area);
    QList<QPointer<KWin::EffectWindow>> unsettled;
    for (int index = 0; index < session->windows.size(); ++index) {
        const auto &window = session->windows.at(index);
        if (!window || window->isDeleted() || !window->window()) continue;
        // §7 owns a minimized pane and has its own door for it. Nothing is
        // read here as the layout failing to place a window that is asleep.
        if (window->isMinimized()) continue;
        const auto &pixel = pixels.at(std::size_t(index));
        if (window->screen() == output && withinPixel(window->frameGeometry(), pixel)) continue;
        qInfo() << "Kadunce" << Revision << "sheds" << logNameOf(window) << "from" << key
                << "at" << window->frameGeometry() << "for its pane"
                << KWin::RectF(pixel.x, pixel.y, pixel.width, pixel.height)
                << (window->isUserMove() ? "while KWin holds a move of it" : "");
        unsettled.append(window);
    }
    // DECISIONS.md § A display without cards organizes everything it shows:
    // there a window the layout cannot show waits in the dock, and the panes
    // left are laid out again without it.
    if (parksOverflow(key)) {
        // The last window shown stays, short of its place or over it, so the
        // display never shows nothing for a window too large for it.
        for (const auto &window : std::as_const(unsettled)) {
            const auto held = m_sessions.constFind(key);
            if (held == m_sessions.cend() || held->windows.size() < 2) break;
            if (window && !window->isDeleted()) parkWindow(key, window);
        }
        auto parked = m_sessions.find(key);
        if (parked != m_sessions.end() && !unsettled.isEmpty() && reflowSession(parked.value())
            && applySession(parked.value(), false))
            scheduleSettle();
        return;
    }
    for (const auto &window : std::as_const(unsettled)) {
        // Re-found by key on every pass: a departure reenters this controller
        // and can end the layout, or empty it, under this loop.
        if (!m_sessions.contains(key) || !window || window->isDeleted()) return;
        if (!extractPaneToCards(window, {}, true)) {
            qWarning() << "Kadunce" << Revision << "could not place a pane on"
                       << key << "and has nowhere to send it";
            return;
        }
    }
}

bool DesktopStageController::replacePaneShortOfPlace(KWin::EffectWindow *window)
{
    if (!window || window->isDeleted() || window->isMinimized() || m_interactionWindow || m_railDrag
        || window->isUserMove() || window->isUserResize()) return false;
    for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
        const int index = it->windows.indexOf(window);
        if (index < 0) continue;
        auto *output = outputForKey(it.key());
        if (it->applying || !output || window->screen() != output
            || it->windows.size() != static_cast<int>(it->rects.size())) return false;
        const auto pixels = sessionPixels(*it, stageArea(output));
        const auto &pixel = pixels.at(std::size_t(index));
        if (onPixel(window->frameGeometry(), pixel) || !withinPixel(window->frameGeometry(), pixel))
            return false;
        qInfo() << "Kadunce" << Revision << "asks" << logNameOf(window) << "for its pane on" << it.key()
                << "again, at" << window->frameGeometry() << "short of"
                << KWin::RectF(pixel.x, pixel.y, pixel.width, pixel.height);
        if (applySession(it.value(), false)) scheduleSettle();
        return true;
    }
    return false;
}

bool DesktopStageController::sessionGeometryMatches(const Session &session, bool shortOfPlace) const
{
    auto *output = outputForKey(session.outputName);
    if (!output || session.windows.size() != static_cast<int>(session.rects.size())) return false;
    const auto area = stageArea(output);
    const auto pixels = sessionPixels(session, area);
    for (int index = 0; index < session.windows.size(); ++index) {
        const auto &window = session.windows.at(index);
        if (!window || window->isDeleted() || !window->window()) return false;
        const auto &pixel = pixels.at(index);
        if (window->screen() != output || window->isMinimized()
            || !(shortOfPlace ? withinPixel : onPixel)(window->frameGeometry(), pixel)) return false;
    }
    return true;
}

KWin::RectF DesktopStageController::fittedIntoWorkArea(const KWin::RectF &geometry,
    KWin::LogicalOutput *output)
{
    const KWin::RectF work = KWin::effects->clientArea(KWin::MaximizeArea, output);
    const double width = std::min(geometry.width(), work.width());
    const double height = std::min(geometry.height(), work.height());
    const double right = std::max(work.x(), work.right() - width);
    const double bottom = std::max(work.y(), work.bottom() - height);
    return KWin::RectF(std::clamp(geometry.x(), work.x(), right),
        std::clamp(geometry.y(), work.y(), bottom), width, height);
}

KWin::RectF DesktopStageController::givenBackPlace(const RestoreSnapshot &snapshot,
    KWin::LogicalOutput *target, bool ownDisplay)
{
    if (!target) return snapshot.geometry;
    const QList<ReleaseDisplay> displays = releaseDisplays();
    const ReleaseRecord record = snapshot.releaseRecord();
    // A place already on `target`, taken on a display still there, is that
    // display's own and only kept clear of panels, as a card's is.
    const ReleaseDisplay *holding = releaseDisplayAt(displays, record.place.center());
    const bool originGone = !record.origin.isEmpty() && !releaseDisplayNamed(displays, record.origin);
    if (!ownDisplay && holding && holding->name == target->name() && !originGone) ownDisplay = true;
    if (ownDisplay) return snapshot.floating() ? fittedIntoWorkArea(snapshot.geometry, target)
                                               : snapshot.geometry;
    const QRectF work(KWin::effects->clientArea(KWin::MaximizeArea, target));
    return KWin::RectF(carriedPlace(QRectF(snapshot.geometry),
        releaseCarriedFrom(displays, record), work));
}

bool DesktopStageController::reminimizeRestored(KWin::Window *client, const RestoreSnapshot &snapshot)
{
    if (!client || (!snapshot.minimized && !snapshot.userMinimized)) return false;
    if (client->frameGeometry() == client->moveResizeGeometry()
        && client->maximizeMode() == snapshot.maximizeMode
        && client->isFullScreen() == snapshot.fullScreen
        && client->quickTileMode() == snapshot.quickTileMode) {
        client->setMinimized(true);
        return true;
    }
    // Observe the final native target, including maximize/tiling.
    // Re-minimizing in the same configure can leave stale client bounds.
    m_restoredMinimizations.push_back(std::make_unique<RestoredMinimization>(
        client, RestoredMinimization::Target{client->moveResizeGeometry(),
            snapshot.maximizeMode, snapshot.quickTileMode, snapshot.fullScreen}));
    return false;
}

void DesktopStageController::restoreSession(const QString &key, bool outputRemoving)
{
    std::erase_if(m_restoredMinimizations, [&](const auto &pending) {
        return pending->result() != RestoredMinimization::Result::Pending
            || pending->output() == outputForKey(key);
    });
    QScopedValueRollback<bool> restoring(m_restoring, true);
    m_applicationGuard.invalidate();
    m_displaced.removeIf([&key](const auto &held) { return held.outputName == key; });
    const auto it = m_sessions.find(key);
    if (it == m_sessions.end()) {
        return;
    }
    const Session session = it.value();
    m_sessions.erase(it);
    if (outputRemoving) m_givenBack.clear();
    QPointer<KWin::LogicalOutput> fallbackOutput = outputForKey(key);
    QPointer<KWin::LogicalOutput> replacementOutput;
    if (outputRemoving) {
        KWin::LogicalOutput *tablet = m_host->tabletOutputForDesktopStage();
        if (tablet && outputKey(tablet) != key) {
            replacementOutput = tablet;
        } else {
            const QList<KWin::LogicalOutput *> outputs = KWin::effects->screens();
            const auto replacement = std::find_if(
                outputs.cbegin(), outputs.cend(),
                [this, &key](const KWin::LogicalOutput *output) {
                    return output && outputKey(output) != key;
                });
            if (replacement != outputs.cend()) {
                replacementOutput = *replacement;
            }
        }
    }
    for (const RestoreSnapshot &snapshot : session.snapshots) {
        if (!snapshot.valid || !snapshot.window
            || snapshot.window->isDeleted() || !snapshot.window->window()) {
            continue;
        }
        QPointer<KWin::Window> client = snapshot.window->window();
        // Release gives the window back as it was before the layout, which
        // was outside any KWin zone Kadunce placed it in.
        if (m_zoned.contains(client)) {
            m_zoned.removeAll(client);
            leaveZone(client);
        }
        QPointer<KWin::LogicalOutput> originalOutput = outputForKey(snapshot.outputName);
        // §11, Every display: a window is given back on the display holding it.
        const bool regiven = !outputRemoving && fallbackOutput && originalOutput != fallbackOutput
            && m_host->releasesOnOwnDisplayForDesktopStage(fallbackOutput);
        if (regiven) originalOutput = fallbackOutput;
        const auto clientValid = [&] {
            return snapshot.window && !snapshot.window->isDeleted() && client
                && snapshot.window->window() == client
                && !snapshot.window->isUserMove() && !snapshot.window->isUserResize();
        };
        QList<QPointer<KWin::LogicalOutput>> candidates;
        const auto append = [&](KWin::LogicalOutput *output) {
            if (output && !candidates.contains(output)) candidates.append(output);
        };
        append(outputRemoving ? replacementOutput.data()
                             : (originalOutput ? originalOutput.data() : fallbackOutput.data()));
        append(m_host->tabletOutputForDesktopStage());
        for (auto *output : KWin::effects->screens()) append(output);
        const auto available = [&](const QPointer<KWin::LogicalOutput> &output) {
            return output && !m_retiredOutputs.contains(output)
                && (!outputRemoving || outputKey(output) != key)
                && KWin::effects->screens().contains(output.data());
        };
        const auto result = restoreOnSurvivingOutput(candidates, available,
            [&](const QPointer<KWin::LogicalOutput> &targetOutput) {
        if (!clientValid()) return RestoreResult::Aborted;
        const bool preserveGeometry = !outputRemoving && !regiven && targetOutput == originalOutput;
        const auto valid = [&] { return clientValid() && available(targetOutput); };
        // A floating place is fitted into the room panels leave even on its
        // own display: one taken while a panel's room was smaller, or larger
        // than the room left, would stand under the panel.
        const KWin::RectF restoreGeometry = givenBackPlace(snapshot, targetOutput, preserveGeometry);
        if (restoreGeometry != snapshot.geometry)
            qInfo() << "Kadunce" << Revision << client->resourceClass() << "is given back at"
                    << restoreGeometry << "instead of" << snapshot.geometry
                    << (targetOutput == originalOutput ? ": its place lies under a panel's room"
                                                       : ": it is given back on another display");
        client->sendToOutput(targetOutput);
        // A window Kadunce parked in the dock was awake before it was parked.
        if (snapshot.parked && client->isMinimized()) client->setMinimized(false);
        // One the person put away in the layout stays put away: its place is
        // given back without bringing it up, so nothing has to minimize it
        // again, which a release that unloads Kadunce would not wait for.
        const bool keptAsleep = client->isMinimized() && !snapshot.parked
            && (snapshot.userMinimized || snapshot.minimized);
        if (valid() && restoreWindowStateChecked(client.data(), snapshot, restoreGeometry,
                           preserveGeometry, !keptAsleep, false, valid)) {
            if (outputRemoving) m_givenBack.append({snapshot.window, restoreGeometry});
            if (!keptAsleep && reminimizeRestored(client.data(), snapshot))
                return valid() ? RestoreResult::Completed
                    : (clientValid() ? RestoreResult::OutputLost : RestoreResult::Aborted);
            return RestoreResult::Completed;
        }
        return clientValid() ? RestoreResult::OutputLost : RestoreResult::Aborted;
        });
        if (result == RestoreResult::OutputLost)
            qWarning() << "Kadunce restore has no surviving candidate output; leaving placement to KWin";
    }
    m_host->retireOutputFromDesktopStage(fallbackOutput.data());
    KWin::effects->addRepaintFull();
    qInfo() << "Kadunce" << Revision << "restored output-local Bento on"
            << key << "during removal" << outputRemoving;
}

bool DesktopStageController::transferTabletSessionToSpread(KWin::LogicalOutput *output,
    const std::function<bool(const BentoProjectionSession &,
                             const std::function<bool()> &)> &accept)
{
    // Each refusal says why: from the keys, a refusal is otherwise a key
    // that does nothing.
    const auto refuse = [output](const char *why) {
        qInfo() << "Kadunce" << Revision << "kept the layout on"
                << (output ? output->name() : QString()) << "out of Spread:" << why;
        return false;
    };
    if (m_restoring || !output || !m_host->isTabletOutputForDesktopStage(output)
        || m_interactionWindow || m_railDrag) return refuse("busy, or not Spread's display");
    const auto *session = sessionForOutput(output);
    if (!session || session->applying || session->windows.isEmpty())
        return refuse("no settled layout");
    // Preserve pane order alongside the authoritative rect vector. The largest
    // real pane is selected as the group lead without reordering that vector.
    auto ordered = session->windows;
    int lead = 0;
    for (int i = 1; i < int(session->rects.size()); ++i)
        if (session->rects[i].width * session->rects[i].height
            > session->rects[lead].width * session->rects[lead].height) lead = i;
    // §5: the group carries the visible combination. §7's sleeping windows ride
    // with it because the session owns their records and nothing else can hold
    // them; any other unshown snapshot is the stranded state applySession
    // reports, and it must not be rebuilt into a group member here.
    for (const auto &saved : session->snapshots)
        if (saved.userMinimized && !ordered.contains(saved.window))
            ordered.append(saved.window);
    BentoProjectionSession projection;
    projection.output = output;
    projection.outputName = outputKey(output);
    projection.workspaceArea = workspaceArea(output);
    projection.rects = session->rects;
    projection.side = session->side;
    projection.sideWindow = session->sideWindow;
    projection.placed = session->placed;
    // §11, Connected: the snapped card keeps its standing while in Spread.
    if (session->windows.contains(session->snappedWindow)) {
        projection.snappedWindow = session->snappedWindow;
        projection.snappedSide = session->snappedSide;
    }
    for (const auto &window : ordered) {
        const auto saved = std::find_if(session->snapshots.cbegin(), session->snapshots.cend(),
            [&](const auto &s) { return s.window == window; });
        if (!window || window->isDeleted() || !window->window() || window->screen() != output
            || saved == session->snapshots.cend() || !saved->valid)
            return refuse("a pane is gone, elsewhere or has no record");
        BentoProjectionMember member{window, NativeMoveSnapshot{*saved, window->window(), output},
            window->isMinimized()};
        member.restore.minimized = saved->minimized || saved->userMinimized;
        if (session->windows.contains(window)) projection.panes.append(member);
        else projection.sleeping.append(member);
    }
    projection.lead = session->windows.value(lead);
    for (auto *stacked : KWin::effects->stackingOrder()) {
        if (ordered.contains(stacked)) projection.stackingOrder.append(stacked);
    }
    for (const auto &member : ordered) {
        if (!projection.stackingOrder.contains(member))
            projection.stackingOrder.append(member);
    }
    // A side snap whose window has since gone names no window; the panes
    // left keep their places, and the group carries no side.
    if (projection.side && !ordered.contains(projection.sideWindow)) {
        projection.side.reset();
        projection.sideWindow.clear();
    }
    if (!validBentoProjectionShape(projectionShape(projection))) return refuse("its shape does not hold");
    const auto generation = m_applicationGuard.generation();
    const QString key = outputKey(output);
    bool committed = false;
    const bool accepted = accept(projection, [&] {
        if (committed || generation != m_applicationGuard.generation()
            || !m_sessions.contains(key)) return false;
        m_sessions.remove(key);
        m_applicationGuard.invalidate();
        std::erase_if(m_restoredMinimizations, [output](const auto &pending) {
            return pending->output() == output;
        });
        committed = true;
        return true;
    });
    if (!accepted || !committed) return refuse("Spread did not take it");
    return true;
}

bool DesktopStageController::resumeProjectedSession(
    const BentoProjectionSession &projection,
    const std::function<bool()> &commitSource,
    const std::function<void()> &releaseSource)
{
    // Each refusal says why: a pick or a key that resumes nothing is
    // otherwise a press that did nothing.
    const auto refuse = [&projection](const char *why) {
        qInfo() << "Kadunce" << Revision << "kept the layout on" << projection.outputName
                << "in Spread:" << why;
        return false;
    };
    if (m_restoring || m_interactionWindow || m_railDrag)
        return refuse(m_interactionWindow ? "a pane is being moved or resized" : "busy");
    if (!projection.output || outputForKey(projection.outputName) != projection.output
        || !m_host->isTabletOutputForDesktopStage(projection.output))
        return refuse("its display is gone or no longer holds Spread");
    if (sessionForOutput(projection.output)) return refuse("the display already shows a layout");
    if (!validBentoProjectionShape(projectionShape(projection))) return refuse("its shape is not whole");
    Session candidate;
    candidate.outputName = projection.outputName;
    candidate.rects = projection.rects;
    candidate.side = projection.side;
    candidate.sideWindow = projection.sideWindow;
    candidate.placed = projection.placed;
    // The snapped card resumes as the snapped card while it is still a pane.
    if (projection.snappedSide
        && std::any_of(projection.panes.cbegin(), projection.panes.cend(),
               [&projection](const BentoProjectionMember &member) {
                   return member.window && member.window == projection.snappedWindow;
               })) {
        candidate.snappedWindow = projection.snappedWindow;
        candidate.snappedSide = projection.snappedSide;
    }
    const auto append = [&](const BentoProjectionMember &member, bool pane) {
        if (!member.window || member.window->isDeleted() || !member.window->window()
            || member.restore.window != member.window->window()
            || member.window->screen() != projection.output
            || member.window->isMinimized() != member.minimized) {
            qInfo() << "Kadunce" << Revision << "kept the layout on" << projection.outputName
                    << "in Spread:" << logNameOf(member.window) << "is gone, on another display,"
                    << "or woke or slept";
            return false;
        }
        if (pane) candidate.windows.append(member.window);
        // §7's intent crosses back with the window. Without it the next solve
        // would offer a sleeping window a pane, which §7 forbids.
        candidate.snapshots.append(RestoreSnapshot{member.restore.place(), member.window, true, !pane});
        return true;
    };
    for (const auto &member : projection.panes) {
        if (member.minimized || !append(member, true)) return false;
    }
    for (const auto &member : projection.sleeping) {
        if (!member.minimized || !append(member, false)) return false;
    }
    if (workspaceArea(projection.output) != projection.workspaceArea) {
        qInfo() << "Kadunce" << Revision << "kept the layout on" << projection.outputName
                << "in Spread: the room for windows changed from" << projection.workspaceArea
                << "to" << workspaceArea(projection.output);
        return false;
    }
    const KWin::Rect area = stageArea(projection.output);
    const auto pixels = makePixelBentoLayout(candidate.rects,
        area.x(), area.y(), area.width(), area.height());
    if (pixels.size() != std::size_t(candidate.windows.size())) return refuse("its panes do not fit its places");
    // A pane's client can change its own frame while the group is projected, so
    // requiring every pane to already sit on the layout turned a pixel of drift
    // into a permanent refusal. CARD-LIFECYCLE.md §6 resumes the group whatever
    // the panes did meanwhile. The stored rects stay authoritative and no
    // layout is solved; a pane that left is put back on the rect it left, and
    // only after the handback commits, so a refused resume still writes nothing.
    bool panesOnLayout = true;
    for (int index = 0; index < candidate.windows.size() && panesOnLayout; ++index) {
        const auto &pixel = pixels[std::size_t(index)];
        panesOnLayout = candidate.windows[index]->frameGeometry().toRect()
            == KWin::Rect(pixel.x, pixel.y, pixel.width, pixel.height);
    }
    const QString resumedKey = candidate.outputName;
    if (!commitResumeHandback(
            [&] { return commitSource && commitSource(); },
            [&] {
                // A resumed layout is a placed one: its panes carry by their
                // title bars as at any other placement, which a session with
                // no application token refuses.
                candidate.applicationToken = m_applicationGuard.issue();
                m_sessions.insert(candidate.outputName, std::move(candidate));
            },
            [&] { if (releaseSource) releaseSource(); })) {
        return false;
    }
    if (!panesOnLayout) {
        qWarning() << "Kadunce" << Revision << "placing" << resumedKey
                   << "back on its stored layout; a pane had left it";
        const auto resumed = m_sessions.find(resumedKey);
        // Placement here is the stored layout and never a solve: a session just
        // rebuilt from its projection has no participation change owing, and
        // applySession is the one path that writes exactly session.rects.
        if (resumed != m_sessions.end() && !resumed->participationDirty
            && applySession(resumed.value(), false))
            scheduleSettle();
    }
    // Each pane grows from where its card was drawn to its place in the
    // layout, replacing any motion placing it gave.
    if (const auto resumed = m_sessions.constFind(resumedKey); resumed != m_sessions.cend()
        && projection.drawnPanes.size() == resumed->windows.size()) {
        QList<QRectF> to;
        for (const auto &window : resumed->windows)
            to.append(window && !window->isDeleted() && window->window()
                ? QRectF(window->window()->moveResizeGeometry()) : QRectF());
        m_host->animateBentoLayout(projection.output, resumed->windows, projection.drawnPanes, to);
    }
    // §11, Connected: the keys go back to the pane used last, not the largest,
    // so resuming is no use of any pane and the one used longest ago stays so.
    KWin::EffectWindow *focus = projection.lead && !projection.lead->isDeleted()
        ? projection.lead.data() : nullptr;
    if (m_host->connectedOutputForDesktopStage(projection.output)) {
        for (const auto &member : projection.panes) {
            KWin::EffectWindow *pane = member.window.data();
            if (pane && !pane->isDeleted() && (!focus
                    || m_host->lastUsedForDesktopStage(pane) > m_host->lastUsedForDesktopStage(focus)))
                focus = pane;
        }
    }
    if (focus && !focus->isDeleted() && focus->window()) {
        KWin::workspace()->raiseWindow(focus->window());
        KWin::workspace()->activateWindow(focus->window(), true);
    }
    KWin::effects->addRepaintFull();
    return true;
}

bool DesktopStageController::holdOutput(KWin::LogicalOutput *output, KWin::EffectWindow *preferred)
{
    if (!output || !m_host->holdsOwnCardsForDesktopStage(output)) return false;
    return activate(output, preferred);
}

QList<QPointer<KWin::EffectWindow>> DesktopStageController::panesOn(const QString &outputName) const
{
    const auto session = m_sessions.constFind(outputName);
    return session == m_sessions.cend() ? QList<QPointer<KWin::EffectWindow>>{} : session->windows;
}

void DesktopStageController::endLonePanes()
{
    const QStringList keys = m_sessions.keys();
    for (const QString &key : keys) endLayoutIntoCardOwnership(key);
}

void DesktopStageController::restoreAllSessions()
{
    QScopedValueRollback<bool> restoring(m_restoring, true);
    stopPendingSettle();
    const QStringList keys = m_sessions.keys();
    for (const QString &key : keys) {
        restoreSession(key);
    }
}

void DesktopStageController::relayAllSessions()
{
    if (m_interactionWindow) return;
    bool placed = false;
    const QStringList keys = m_sessions.keys();
    for (const QString &key : keys) {
        // Re-found by key: placing one layout can reenter this controller.
        auto session = m_sessions.find(key);
        if (session == m_sessions.end() || session->applying) continue;
        placed = applySession(session.value(), false) || placed;
    }
    if (placed) scheduleSettle();
}

void DesktopStageController::cancelRestoredMinimizations()
{
    m_restoredMinimizations.clear();
}

} // namespace Kadunce
