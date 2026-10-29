/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Room offers: the cards offered for an open place, and the pair offer.
#include <KLocalizedString>
#include "EffectInternal.h"

namespace Kadunce
{

void Effect::askPairOffer(KWin::LogicalOutput *output, KWin::EffectWindow *card, BentoSidePlacement side)
{
    const int ask = ++m_roomOfferAsk;
    if (m_pairOfferAsked && m_pairOfferAsked->card != card) releasePairOfferPlace(m_pairOfferAsked->card);
    m_pairOfferAsked = PairOfferAsked{card, side};
    const QPointer<KWin::LogicalOutput> landed = output;
    QTimer::singleShot(motion(320), this, [this, landed, ask] {
        if (landed) offerOpenRoom(landed, ask);
    });
}

QList<KWin::EffectWindow *> Effect::sleepingCardsOn(const KWin::LogicalOutput *output) const
{
    QList<KWin::EffectWindow *> sleeping;
    for (const auto &card : m_cardStage->liveCards()) {
        if (!card || card->isDeleted() || !card->isMinimized() || card->screen() != output
            || m_desktopStage->managesWindow(card) || m_cardStage->paneHolding(card)) continue;
        const auto stack = m_cardStage->stackFrontToBack(card);
        if (!stack.isEmpty() && stack.first() != card) continue;
        sleeping.append(card.data());
    }
    return sleeping;
}

QRectF Effect::pairOfferRoom(KWin::LogicalOutput *output, const BentoSidePlacement &side) const
{
    // The share the snapped card leaves, beside the share it names: two
    // thirds from the upper half of the edge, one third from the lower, as a
    // pair on Solo stands (bentoProportionalSideLayout).
    const QRectF whole(m_cardStage->activeTarget(output));
    // The Visual sheet's gutter between two panes.
    const double gap = 10.0;
    const double named = (whole.width() - gap) * (side.large ? 2.0 / 3.0 : 1.0 / 3.0);
    const double room = whole.width() - gap - named;
    return QRectF(side.right ? whole.x() : whole.x() + named + gap, whole.y(), room, whole.height());
}

void Effect::releasePairOfferPlace(KWin::EffectWindow *card, bool refit)
{
    if (!card || card->isDeleted()) return;
    SessionScope scope(this, m_currentSession, stageKeyFor(card->screen()));
    if (!m_cardStage->holdActivePlace(card, std::nullopt) || !refit) return;
    // Moved after the frame that let the offer go, never during its paint.
    const QPointer<KWin::EffectWindow> held = card;
    QTimer::singleShot(0, this, [this, held] {
        if (!held || held->isDeleted()) return;
        SessionScope scope(this, m_currentSession, stageKeyFor(held->screen()));
        (void)m_cardStage->refitActive(held);
    });
}

void Effect::askRoomOffer(KWin::LogicalOutput *output)
{
    const int ask = ++m_roomOfferAsk;
    if (m_pairOfferAsked) releasePairOfferPlace(m_pairOfferAsked->card);
    m_pairOfferAsked.reset();
    if (!output || !connectedOutputForDesktopStage(output)) return;
    const QPointer<KWin::LogicalOutput> landed = output;
    QTimer::singleShot(motion(320), this, [this, landed, ask] {
        if (landed) offerOpenRoom(landed, ask);
    });
}

void Effect::offerOpenRoom(KWin::LogicalOutput *output, int ask, int tries)
{
    if (ask != m_roomOfferAsk) return;
    const auto pair = m_pairOfferAsked;
    // The card a new pair offer stands beside keeps the place it now holds.
    if (pair && m_roomOffer && m_roomOffer->pairWith == pair->card) m_roomOffer->pairSide.reset();
    leaveRoomOffer();
    // A pair offer that does not open gives the card its own place back.
    const auto giveUp = [this, pair] {
        if (pair) releasePairOfferPlace(pair->card);
    };
    if (!output || m_releasing || !KWin::effects->screens().contains(output)) {
        giveUp();
        return;
    }
    SessionScope scope(this, m_currentSession, stageKeyFor(output));
    const bool settled = !m_carriedWindow && (pair
        ? pair->card && m_cardStage->activeCardIdentity() == pair->card
            && m_cardStage->presentation() == CardPresentation::Active
        : connectedOutputForDesktopStage(output) && m_cardStage->presentsBento());
    const auto room = !settled ? std::nullopt
        : pair ? std::optional<QRectF>(pairOfferRoom(output, pair->side)) : m_desktopStage->openRoomOn(output);
    if (!room) {
        // A layout the snap began shows once its card has glided and grown
        // into place.
        const QPointer<KWin::LogicalOutput> later = output;
        if (tries > 0)
            QTimer::singleShot(60, this, [this, later, ask, tries] {
                if (later) offerOpenRoom(later, ask, tries - 1);
            });
        else {
            qInfo() << "Kadunce" << Revision << "found no room open on" << output->name() << "to offer;"
                    << "carrying" << bool(m_carriedWindow) << "connected" << connectedOutputForDesktopStage(output)
                    << "Bento" << m_cardStage->presentsBento();
            giveUp();
        }
        return;
    }
    // The fronts of the display's other cards, in Spread's order, asleep ones
    // too; a card waiting behind a pane stays there.
    RoomOfferShown offer{output, *room, {}, {}, -1, {}, {}};
    if (pair) {
        offer.pairWith = pair->card;
        offer.pairSide = pair->side;
    }
    QList<QSizeF> sizes;
    for (const auto &card : m_cardStage->liveCards()) {
        if (!card || card->isDeleted() || card->screen() != output || card == offer.pairWith
            || m_desktopStage->managesWindow(card) || m_cardStage->paneHolding(card)) continue;
        // Beside the Active card on Solo, only sleeping cards: an awake one
        // would have been its partner.
        if (pair && !card->isMinimized()) continue;
        const auto stack = m_cardStage->stackFrontToBack(card);
        if (!stack.isEmpty() && stack.first() != card) continue;
        offer.cards.append(card);
        sizes.append(QRectF(card->frameGeometry()).size());
    }
    offer.tiles = roomOfferTiles(*room, sizes);
    offer.cards = offer.cards.mid(0, offer.tiles.size());
    if (offer.tiles.isEmpty()) {
        giveUp();
        return;
    }
    for (const auto &card : std::as_const(offer.cards))
        m_roomOfferShown.insert(card.data(), KWin::EffectWindowVisibleRef(card.data(),
            KWin::EffectWindow::PAINT_DISABLED | KWin::EffectWindow::PAINT_DISABLED_BY_MINIMIZE
            | KWin::EffectWindow::PAINT_DISABLED_BY_DESKTOP));
    qInfo() << "Kadunce" << Revision << "offers" << offer.cards.size() << "cards in the room open on"
            << output->name();
    m_roomOffer = std::move(offer);
    m_roomOfferInput->active = true;
    KWin::effects->addRepaintFull();
    Q_EMIT workspaceContextChanged();
}

QString Effect::roomOfferHint() const
{
    return i18n("Pick one for this side \u00b7 Esc leaves it open");
}

void Effect::leaveRoomOffer(bool restore)
{
    if (m_roomOfferInput) m_roomOfferInput->active = false;
    if (!m_roomOffer) return;
    QPointer<KWin::EffectWindow> beside;
    if (m_roomOffer->pairSide) beside = m_roomOffer->pairWith;
    m_roomOffer.reset();
    if (beside) releasePairOfferPlace(beside, restore);
    m_roomOfferShown.clear();
    m_offerPictures.clear();
    KWin::effects->addRepaintFull();
    Q_EMIT workspaceContextChanged();
}

int Effect::roomOfferPictureAt(QPointF position) const
{
    if (!m_roomOffer || !m_roomOffer->output) return -2;
    for (int i = 0; i < m_roomOffer->tiles.size(); ++i)
        if (m_roomOffer->tiles.at(i).adjusted(0, 0, 0, RoomOfferNameRow).contains(position)) return i;
    return m_roomOffer->room.contains(position) ? -1 : -2;
}

void Effect::pickFromRoomOffer(int index)
{
    if (!m_roomOffer || index < 0 || index >= m_roomOffer->cards.size()) return;
    const QPointer<KWin::EffectWindow> card = m_roomOffer->cards.at(index);
    const QPointer<KWin::LogicalOutput> output = m_roomOffer->output;
    const QPointer<KWin::EffectWindow> pairWith = m_roomOffer->pairWith;
    const auto pairSide = m_roomOffer->pairSide;
    leaveRoomOffer(!pairSide);
    if (pairSide) {
        // §11, Solo: the card picked wakes and pairs with the Active card,
        // which keeps the side it was snapped to.
        QTimer::singleShot(0, this, [this, card, output, pairWith, pairSide] {
            if (!card || card->isDeleted() || !output || !pairWith || pairWith->isDeleted()) return;
            SessionScope scope(this, m_currentSession, stageKeyFor(output));
            if (m_cardStage->activeCardIdentity() != pairWith || m_cardStage->liveCardIndex(card) < 0) {
                qInfo() << "Kadunce" << Revision << "could not pair" << logNameOf(pairWith) << "with" << logNameOf(card)
                        << "picked beside it: it is no longer Active, or the pick is no card";
                (void)m_cardStage->refitActive(pairWith);
                return;
            }
            if (card->isMinimized()) card->unminimize();
            if (!card || card->isMinimized() || !pairCardsIntoBento(output, pairWith, card, *pairSide)) {
                qInfo() << "Kadunce" << Revision << "could not pair" << logNameOf(pairWith) << "with"
                        << (card ? logNameOf(card) : QStringLiteral("a closed card"));
                (void)m_cardStage->refitActive(pairWith);
                return;
            }
            qInfo() << "Kadunce" << Revision << "paired" << logNameOf(pairWith) << "with" << logNameOf(card)
                    << "picked beside it";
            observeCardOwnership();
            Q_EMIT workspaceContextChanged();
        });
        return;
    }
    // After the press that picked it, as a pick in Spread is: the card fills
    // the room, its Stack waiting behind it.
    QTimer::singleShot(0, this, [this, card, output] {
        if (!card || card->isDeleted() || !output) return;
        SessionScope scope(this, m_currentSession, stageKeyFor(output));
        if (!m_cardStage->presentsBento() || m_cardStage->liveCardIndex(card) < 0
            || !m_desktopStage->hasSessionOnOutput(output->name())) return;
        // A card picked asleep wakes into the room.
        if (card->isMinimized()) card->unminimize();
        if (!card) return;
        const QList<KWin::EffectWindow *> behind = m_cardStage->stackFrontToBack(card).mid(1);
        if (!m_desktopStage->admitCardToLiveBento(card,
                [this, card] { return card && m_cardStage->releaseCardToLiveBento(card); }, true)) return;
        if (!behind.isEmpty()) m_cardStage->notePaneStack(card, behind);
        m_cardStage->settlePaneStacks();
        KWin::effects->activateWindow(card);
        qInfo() << "Kadunce" << Revision << "filled the open room with" << logNameOf(card);
        observeCardOwnership();
        Q_EMIT workspaceContextChanged();
    });
}

void Effect::drawRoomOffer(const KWin::RenderTarget &renderTarget, const KWin::RenderViewport &viewport,
    const KWin::Region &deviceRegion, KWin::LogicalOutput *screen, QList<QRectF> &overlaid)
{
    if (!m_roomOffer || !screen || m_roomOffer->output != screen) return;
    const QRectF room = m_roomOffer->room;
    // The pictures stand on the desktop itself, with no box behind them.
    QRegion dirtied;
    for (const auto &dirty : deviceRegion.rects())
        dirtied += QRectF(viewport.mapFromDeviceCoordinates(KWin::RectF(dirty))).toAlignedRect();
    // The picture under the pointer takes the selected edge.
    const int hovered = m_roomOffer->hovered;
    if (hovered >= 0 && hovered < m_roomOffer->tiles.size() && hovered < m_roomOffer->cards.size()
        && m_roomOffer->cards.at(hovered)) {
        const QRectF picture = offerPicture(m_roomOffer->tiles.at(hovered),
            QRectF(m_roomOffer->cards.at(hovered)->frameGeometry()).size());
        QList<QRectF> edge;
        for (const QRect &part : dirtied.intersected(picture.toAlignedRect())) edge.append(QRectF(part));
        fillDestinationParts(renderTarget, viewport, edge, picture, 0.f, 0.24f);
    }
    double bottom = room.top();
    for (int i = 0; i < m_roomOffer->tiles.size() && i < m_roomOffer->cards.size(); ++i) {
        KWin::EffectWindow *card = m_roomOffer->cards.at(i);
        if (!card) continue;
        const QRectF tile = m_roomOffer->tiles.at(i);
        m_cardLabelRenderer.render(renderTarget, viewport, tile,
            {spreadCardName(card, m_cardStage->liveCardIndex(card) + 1), QString()});
        overlaid.append(CardLabelRenderer::labelRect(tile));
        bottom = std::max(bottom, tile.bottom() + RoomOfferNameRow);
    }
    // How to choose, and how not to, under the pictures.
    // A label stands under the rect it names, so the rect is one pixel tall
    // and ends where the pictures' names do.
    // A display whose Context Panel speaks for it says it there instead.
    if (contextLineClaimed(screen)) return;
    const QRectF line(room.x() + RoomOfferGap, bottom - 1.0, room.width() - 2 * RoomOfferGap, 1.0);
    if (CardLabelRenderer::labelRect(line).bottom() <= room.bottom())
        m_cardLabelRenderer.render(renderTarget, viewport, line, {roomOfferHint(), QString()});
}

} // namespace Kadunce
