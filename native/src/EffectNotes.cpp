/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Notes in Spread: the notes surface, its hand-over, and stuck notes drawn on
// their cards.
#include "EffectInternal.h"

namespace Kadunce
{

// Gooseberry's notes over a window are a layer surface laid over that
// window; its quick-note card is one that covers the whole display. A layer
// surface carries no application id, so KWin names it by its program,
// gooseberry, rather than by Gooseberry's desktop file name; either is taken.
// The notes take the keys while they are open, so they are told from the card
// by their size, and, while live, by taking no keys when they take none. A
// closing surface keeps its size, so it is told the same way through its fade.
bool Effect::isNotesSurface(const KWin::EffectWindow *window) const
{
    if (!window || !window->window() || !window->window()->inherits("KWin::LayerShellV1Window")) return false;
    const QString identity = applicationIdentity(window);
    if (identity.compare(QStringLiteral("io.github.carlsonjm.Gooseberry"), Qt::CaseInsensitive) != 0
        && identity.compare(QStringLiteral("gooseberry"), Qt::CaseInsensitive) != 0) {
        return false;
    }
    if (!window->isDeleted() && !window->acceptsFocus()) return true;
    const auto *output = window->screen();
    return output && window->frameGeometry().toRect() != output->geometry();
}

bool Effect::isSearchSurface(const KWin::EffectWindow *window)
{
    if (!window || !window->window() || !window->window()->inherits("KWin::LayerShellV1Window")) return false;
    const QString identity = applicationIdentity(window);
    return identity.compare(QStringLiteral("io.github.carlsonjm.Tettegouche"), Qt::CaseInsensitive) == 0
        || identity.compare(QStringLiteral("tettegouche"), Qt::CaseInsensitive) == 0;
}

// While Spread is up the notes are drawn on the cards instead, so that
// surface is left undrawn from the frame Spread opens, and through its fade
// as it goes, rather than once Gooseberry answers the pause: the notes read
// as one set moving onto the card. As Spread closes, Spread's picture of them
// stays over the Active card until the surface stands there (syncNotesHandover).
bool Effect::notesSurfaceStepsAside(const KWin::EffectWindow *window) const
{
    return (m_notesSpreadShown || m_notesHandover) && isNotesSurface(window);
}

void Effect::syncNotesHandover(bool spreadClosed)
{
    if (spreadClosed) {
        m_notesHandover.reset();
        auto *card = m_cardStage->presentation() == CardPresentation::Active
            ? m_cardStage->activeCardIdentity() : nullptr;
        const auto *entry = card ? m_stuckNotes.entryFor(card->internalId()) : nullptr;
        const bool asked = card && !m_notesAskedFor.isNull() && m_notesAskedFor == card->internalId();
        m_notesAskedFor = {};
        if (entry && (entry->shown || asked) && m_stuckNotesWatcher && m_stuckNotesWatcher->present()) {
            m_notesHandover = NotesHandover{card, {}, asked};
            m_notesHandover->since.start();
        }
    }
    if (!m_notesHandover) return;
    auto *card = m_notesHandover->card.data();
    const auto *entry = card && !card->isDeleted() ? m_stuckNotes.entryFor(card->internalId()) : nullptr;
    bool done = !entry || (!entry->shown && !m_notesHandover->asked) || !m_cardStage->isActive()
        || m_cardStage->presentation() != CardPresentation::Active || m_cardStage->activeCardIdentity() != card
        || m_notesHandover->since.elapsed() > NotesHandoverLimitMs;
    if (!done) {
        // Gooseberry's surface stands over the window once it is laid at the
        // window's place and size, as Gooseberry lays it once the desktop says
        // where the window is; from this frame it is drawn and Spread's
        // picture is not.
        const QRectF frame(card->frameGeometry());
        const QRectF display = card->screen() ? QRectF(card->screen()->geometry()) : QRectF();
        for (KWin::EffectWindow *window : KWin::effects->stackingOrder()) {
            if (!window || window->isDeleted() || !window->window() || !window->window()->readyForPainting()
                || !isNotesSurface(window)) continue;
            if (notesSurfaceStandsOn(QRectF(window->frameGeometry()), frame, display)) {
                done = true;
                break;
            }
        }
    }
    if (done) {
        const bool late = m_notesHandover->since.elapsed() > NotesHandoverLimitMs;
        if (late) qInfo() << "Kadunce gave the Active card's notes back to Gooseberry without its surface in place";
        m_notesHandover.reset();
        KWin::effects->addRepaintFull();
    }
}

void Effect::paintNotesHandover(const KWin::RenderTarget &renderTarget, const KWin::RenderViewport &viewport,
                                QList<QRectF> &drawn)
{
    if (!m_notesHandover) return;
    auto *card = m_notesHandover->card.data();
    const auto *entry = card && !card->isDeleted() ? m_stuckNotes.entryFor(card->internalId()) : nullptr;
    if (!entry) return;
    for (const auto &mark : windowNoteMarks(QRectF(card->frameGeometry()), *entry)) {
        if (mark.kind == CardNoteMark::Kind::Dot) {
            m_noteStackRenderer.renderDot(renderTarget, viewport, mark.rect, entry->notes.first().colourHex,
                                          mark.count);
        } else {
            m_noteStackRenderer.renderNote(renderTarget, viewport, mark.rect, entry->notes.at(mark.note), 1.0);
        }
        drawn << mark.rect;
    }
}

// The window a note with the keys is stuck to: the latest used of those whose
// notes Gooseberry says are up, or, where it has not said yet, the window last
// in use, which the note was opened over.
KWin::EffectWindow *Effect::notesSurfaceHost(const KWin::EffectWindow *window) const
{
    if (!isNotesSurface(window)) return nullptr;
    KWin::EffectWindow *host = nullptr;
    quint64 latest = 0;
    for (KWin::EffectWindow *candidate : KWin::effects->stackingOrder()) {
        if (!isApplicationWindow(candidate) || candidate->isMinimized()
            || !m_stuckNotes.shown(candidate->internalId())) {
            continue;
        }
        const quint64 used = m_activationOrder.value(windowIdentity(candidate));
        if (!host || used > latest) {
            host = candidate;
            latest = used;
        }
    }
    if (host) return host;
    return isApplicationWindow(m_lastActiveApplication) ? m_lastActiveApplication.data() : nullptr;
}

QList<NotesCard> Effect::notesCards() const
{
    QList<NotesCard> cards;
    KWin::LogicalOutput *tablet = tabletOutput();
    if (!tablet) return cards;
    const auto &model = m_cardStage->model();
    const auto &windows = m_cardStage->liveCards();
    auto *selected = m_cardStage->selectedWindow();
    // The centred card first, as a tap finds cards, then the other faces.
    for (int pass = 0; pass < 2; ++pass) {
        for (int index = 0; index < windows.size(); ++index) {
            auto *window = windows.at(index).data();
            if (!window || window->isDeleted() || (window == selected) != (pass == 0)
                || m_cardStage->paintSlot(window) == 99 || m_cardStage->thrownAway(window)) continue;
            const int cardId = index + 1;
            if (model.stackPositionForId(cardId) != model.stackActivePositionForId(cardId)) continue;
            // Where the card was last drawn, as the notes on it were; a card
            // not drawn yet stands where Spread puts it.
            const auto drawn = m_cardLabelTargets.constFind(window);
            const QRectF rect = drawn != m_cardLabelTargets.cend()
                ? *drawn : QRectF(m_cardStage->posedTargetForWindow(tablet, window));
            cards.append({window->internalId(), rect, window->frameGeometry().size(),
                          m_cardFaceRects.value(window)});
        }
    }
    return cards;
}

KWin::EffectWindow *Effect::notesCardWindow(const QUuid &id) const
{
    for (const auto &window : m_cardStage->liveCards())
        if (window && !window->isDeleted() && window->internalId() == id) return window.data();
    return nullptr;
}

bool Effect::pressNotesFromInput(const QPointF &position)
{
    if (!notesShownInSpread() || !m_cardStage->rowStill()) return false;
    const bool taken = m_stuckNotes.press(position, notesCards());
    if (taken) KWin::effects->addRepaintFull();
    return taken;
}

void Effect::moveNotesFromInput(const QPointF &position)
{
    m_stuckNotes.move(position);
    if (m_stuckNotes.carrying()) KWin::effects->addRepaintFull();
}

void Effect::holdNotesFromInput()
{
    if (notesShownInSpread() && m_stuckNotes.hold()) KWin::effects->addRepaintFull();
}

void Effect::releaseNotesFromInput(const QPointF &position, bool still)
{
    const auto cards = notesCards();
    const auto request = m_stuckNotes.release(position, still, cards);
    KWin::effects->addRepaintFull();
    auto *window = request ? notesCardWindow(request->window) : nullptr;
    if (!window || !m_stuckNotesWatcher) return;
    // Gooseberry acts and says so; what Spread draws follows its word.
    const QString windowId = window->internalId().toString();
    using Kind = StuckNotesSpread::Request::Kind;
    if (request->kind == Kind::Stick) {
        m_stuckNotesWatcher->stickTo(request->noteId, windowId, window->caption(),
                                     applicationIdentity(window));
        return;
    }
    // A stuck note is typed on its window, never in Spread: the notes are
    // asked up over the window, a tapped note ready to type, and the card
    // opens as a tap on it anywhere else would. Gooseberry stays paused until
    // the card is Active, and then shows them there.
    if (request->kind == Kind::Write)
        m_stuckNotesWatcher->write(request->noteId, windowId, window->caption(), applicationIdentity(window));
    else
        m_stuckNotesWatcher->show(windowId, window->caption(), applicationIdentity(window));
    m_notesAskedFor = request->window;
    const auto reaches = [this](const QPointF &at) {
        auto *card = m_cardStage->cardUnderPoint(at);
        return card ? card->internalId() : QUuid();
    };
    if (const auto at = StuckNotesSpread::openingPoint(request->window, position, cards, reaches))
        tapSpreadFromInput(*at);
}

void Effect::syncNotesPause()
{
    // Gooseberry's notes over the windows step aside while Spread covers them,
    // or while a window is carried, which they cannot follow under the hand,
    // and come back as it closes or lands, still out.
    if (!m_stuckNotesWatcher || !m_stuckNotesWatcher->present()) {
        m_notesPaused = false;
        return;
    }
    const bool aside = m_notesSpreadShown || bool(m_carriedWindow);
    if (m_notesPaused == aside) return;
    m_notesPaused = aside;
    m_stuckNotesWatcher->pause(m_notesPaused);
}

void Effect::cancelNotesFromInput()
{
    m_stuckNotes.cancel();
    KWin::effects->addRepaintFull();
}

void Effect::paintStuckNotes(const KWin::RenderTarget &renderTarget, const KWin::RenderViewport &viewport,
                             const KWin::Region &deviceRegion, KWin::LogicalOutput *screen,
                             const QRectF &heldRect, QList<QRectF> &drawn)
{
    if (m_stuckNotes.isEmpty() || m_cardStage->launcherGuestActive()) return;
    const auto &model = m_cardStage->model();
    const auto &windows = m_cardStage->liveCards();
    auto *held = m_cardStage->cardGrabActive() ? m_cardStage->selectedWindow() : nullptr;
    const auto carried = m_stuckNotes.carriedNote();
    for (int index = 0; index < windows.size(); ++index) {
        auto *window = windows.at(index).data();
        if (!window || window->isDeleted() || m_cardStage->paintSlot(window) == 99) continue;
        const int cardId = index + 1;
        if (model.stackPositionForId(cardId) != model.stackActivePositionForId(cardId)) continue;
        const auto *entry = m_stuckNotes.entryFor(window->internalId());
        const auto target = m_cardLabelTargets.constFind(window);
        if (!entry || target == m_cardLabelTargets.cend()) continue;
        // Worked out afresh every frame from where the card and its window
        // are drawn this frame, so the notes ride the card through every
        // motion. A held card is above the row, so no note of the row shows
        // through it.
        const auto marks = cardNoteMarks(*target, m_cardFaceRects.value(window), window->frameGeometry().size(),
                                         *entry);
        for (const auto &mark : marks) {
            if (window != held && !heldRect.isEmpty() && mark.reach.intersects(heldRect)) continue;
            if (mark.kind == CardNoteMark::Kind::Dot) {
                m_noteStackRenderer.renderDot(renderTarget, viewport, mark.rect, entry->notes.first().colourHex,
                                              mark.count);
                drawn << mark.rect;
                continue;
            }
            if (mark.kind == CardNoteMark::Kind::Stack) {
                QStringList colours;
                for (const int note : mark.sheetNotes) colours.append(entry->notes.at(note).colourHex);
                m_noteStackRenderer.renderStack(renderTarget, viewport, mark.sheets, colours,
                                                 entry->notes.at(mark.note).title, mark.count);
                drawn << mark.sheets;
                continue;
            }
            const auto &note = entry->notes.at(mark.note);
            // The note in hand leaves a faint place behind; one standing in
            // its place is faint while its notes are put away.
            const bool lifted = carried && carried->id == note.id;
            const bool putAway = !m_stuckNotes.shown(window->internalId());
            m_noteStackRenderer.renderNote(renderTarget, viewport, mark.rect, note,
                                           lifted ? 0.35 : putAway ? NoteGeometry::PutAwayOpacity : 1.0);
            drawn << mark.rect;
        }
    }
    if (carried) {
        if (auto *target = notesCardWindow(m_stuckNotes.dropTarget(notesCards()));
            target && m_destinationShader) {
            if (const auto box = m_cardLabelTargets.constFind(target); box != m_cardLabelTargets.cend()) {
                drawDestinationOutline(renderTarget, viewport, deviceRegion, screen, *box);
                drawn.append(*box);
            }
        }
        m_noteStackRenderer.renderNote(renderTarget, viewport, m_stuckNotes.carriedRect(), *carried, 1.0);
        drawn.append(m_stuckNotes.carriedRect());
    }
}

} // namespace Kadunce
