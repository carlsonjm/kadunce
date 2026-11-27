/* SPDX-License-Identifier: GPL-2.0-or-later */

// INPUT.md § Active card: a window an application opens under a finger still
// pulling, as a tab pulled out of its strip, follows that finger as a window
// carried by its title bar does, and settles where the finger lets go.
#include "EffectInternal.h"
#include <options.h>
#include <window.h>

namespace Kadunce
{

namespace
{
// A tap that opens a window travels less than this; a pull travels more.
constexpr double PullDistance = 40.0;
// A window ready this long after its finger last moved was not pulled out.
constexpr qint64 PullFreshMs = 1500;
// Where the finger holds the window: across a quarter of its width, at most
// this far, and this far down, in its own tab row.
constexpr double GrabAcrossMax = 200.0;
constexpr double GrabDown = 20.0;

QString touchUnderName(const QPointF &point)
{
    const KWin::Window *window = KWin::input() ? KWin::input()->findToplevel(point) : nullptr;
    return window && window->effectWindow() ? logNameOf(window->effectWindow()) : QStringLiteral("nothing");
}

QString touchUnderApplication(const QPointF &point)
{
    const KWin::Window *window = KWin::input() ? KWin::input()->findToplevel(point) : nullptr;
    return window && window->effectWindow() ? applicationIdentity(window->effectWindow()) : QString();
}

} // namespace

void Effect::watchTouchCarry()
{
    if (!m_touchWitness) return;
    m_touchWitness->describeUnder = [](const QPointF &at) { return touchUnderName(at); };
    m_touchWitness->applicationAt = [](const QPointF &at) { return touchUnderApplication(at); };
    m_touchWitness->onLift = [this](qint32 id) { touchCarryLifted(id); };
}

bool Effect::carryOpenedUnderFinger(KWin::EffectWindow *window, const std::function<void()> &admit)
{
    if (!m_touchWitness || !window || !window->window() || !window->isNormalWindow()
        || window->isModal() || window->transientFor()) return false;
    const QString application = applicationIdentity(window);
    const auto pulled = [&](const TouchWitness::Contact &contact, qint64 quiet) {
        return contact.application == application && contact.travelled > PullDistance
            && quiet >= 0 && quiet <= PullFreshMs;
    };
    const auto &contacts = m_touchWitness->contacts();
    if (contacts.isEmpty()) {
        // The finger lifted before the window was ready: let go at an edge it
        // goes there, as a dock drop does; anywhere else it is a pane beside
        // the window it was pulled from.
        const TouchWitness::Contact &lifted = m_touchWitness->lifted();
        const qint64 since = m_touchWitness->sinceLift();
        if (!pulled(lifted, TouchWitness::now() - lifted.lastMoved) || since > PullFreshMs) return false;
        qInfo().noquote() << "Kadunce" << Revision << "contact lifted" << since << "ms before"
                          << logNameOf(window) << "was ready; places it at the lift point" << lifted.last;
        const QPointer<KWin::EffectWindow> source = lifted.window ? lifted.window->effectWindow() : nullptr;
        admitWithoutCarry(window, admit);
        const PlacementAim aim = placementAimAt(lifted.last);
        const bool edge = aim.kind == PlacementAimKind::Left || aim.kind == PlacementAimKind::Right
            || aim.kind == PlacementAimKind::Top || aim.kind == PlacementAimKind::Display;
        if (edge) (void)placeWindow(window, aim, lifted.last);
        else (void)pairCarriedWithSource(window, source, lifted.last);
        return true;
    }
    if (contacts.size() != 1) return false;
    const TouchWitness::Contact &contact = contacts.cbegin().value();
    if (!pulled(contact, TouchWitness::now() - contact.lastMoved)) return false;
    KWin::Window *client = window->window();
    const QPointF point = KWin::input()->touch() ? KWin::input()->touch()->position() : contact.last;
    // A window wider than half its display arrives at half the display's
    // width, at its own proportions, so it does not cover everything.
    QSizeF size = client->frameGeometry().size();
    if (KWin::LogicalOutput *display = window->screen()) {
        const double half = display->geometry().width() / 2.0;
        if (size.width() > half && size.width() > 0)
            size = QSizeF(half, size.height() * half / size.width());
    }
    const QPointF grab(std::min(size.width() / 4.0, GrabAcrossMax), GrabDown);
    client->moveResize(KWin::RectF(point - grab, size));
    m_touchCarry.window = window;
    m_touchCarry.source = contact.window ? contact.window->effectWindow() : nullptr;
    m_touchCarry.contact = contact.id;
    m_touchCarry.admit = admit;
    if (!client->performMousePressCommand(KWin::Options::MouseMove, point) && !window->isUserMove()) {
        qInfo().noquote() << "Kadunce" << Revision << "could not carry" << logNameOf(window)
                          << "under contact" << contact.id << "; admits it as opened";
        m_touchCarry.window.clear();
        m_touchCarry.admit = nullptr;
        return false;
    }
    // KWin moves it; Kadunce's carry takes it as it takes a title bar's drag.
    const bool proven = m_carryRuntime && m_carryRuntime->observer.identifyOpenedUnderContact(client, contact.id);
    qInfo().noquote() << "Kadunce" << Revision << "carries" << logNameOf(window) << "under contact" << contact.id
                      << (proven ? "with its edges" : "without its edges")
                      << "at" << point << "size" << size;
    return true;
}

void Effect::admitWithoutCarry(KWin::EffectWindow *window, const std::function<void()> &admit)
{
    m_touchCarry.passed = window;
    admit();
    m_touchCarry.passed.clear();
}

void Effect::touchCarryLifted(qint32 id)
{
    if (!m_touchCarry.window || id != m_touchCarry.contact) return;
    // The carry's own finish places it at an edge, on a card or on another
    // display; let go where nothing takes it, it is a pane beside the window
    // it was pulled from.
    const QPointer<KWin::EffectWindow> window = m_touchCarry.window;
    const QPointer<KWin::EffectWindow> source = std::exchange(m_touchCarry.source, nullptr);
    const QPointF lift = m_touchWitness ? m_touchWitness->lifted().last : QPointF();
    m_touchCarry.window.clear();
    m_touchCarry.contact = -1;
    auto admit = std::exchange(m_touchCarry.admit, nullptr);
    QTimer::singleShot(400, this, [this, window, source, lift, admit] {
        if (!window || window->isDeleted() || window->isUserMove() || !admit) return;
        SessionScope scope(this, m_scopedSession, stageKeyOf(window));
        if (m_desktopStage->managesWindow(window) || m_cardStage->liveCardIndex(window) >= 0) {
            qInfo().noquote() << "Kadunce" << Revision << "carried" << logNameOf(window) << "settled where it was let go";
            return;
        }
        qInfo().noquote() << "Kadunce" << Revision << "carried" << logNameOf(window)
                          << "was let go where nothing took it";
        admitWithoutCarry(window, admit);
        (void)pairCarriedWithSource(window, source, lift);
    });
}

bool Effect::pairCarriedWithSource(KWin::EffectWindow *window, KWin::EffectWindow *source, QPointF lift)
{
    // J: a tab pulled out is pulled out to see more at once, so it stands
    // beside its source, on the side it was let go: with the Active card as a
    // pair, or in the source's layout as a pick from Spread joins it.
    KWin::LogicalOutput *display = window ? window->screen() : nullptr;
    // §3, §5: only with an awake card or pane on the display let go on, in
    // this workspace; else it stays as it was admitted, the Active card.
    if (!window || window->isDeleted() || !source || source->isDeleted() || source == window || !display
        || source->screen() != display || !QRectF(display->geometry()).contains(lift)
        || source->isMinimized() || !source->isOnCurrentDesktop()) return false;
    SessionScope scope(this, m_scopedSession, stageKeyFor(display));
    const bool right = lift.x() >= QRectF(display->geometry()).center().x();
    bool paired = false;
    if (m_desktopStage->managesWindow(source)) {
        paired = playPickedCardIntoLayout(window) && m_desktopStage->managesWindow(window);
    } else if (m_cardStage->liveCardIndex(source) >= 0 && m_cardStage->liveCardIndex(window) >= 0) {
        paired = pairCardsIntoBento(display, window, source,
            BentoSidePlacement{right, true, connectedOutputForDesktopStage(display), false});
        if (paired) KWin::effects->activateWindow(window);
    }
    qInfo().noquote() << "Kadunce" << Revision << (paired ? "pairs" : "could not pair") << "carried"
                      << logNameOf(window) << "with its source" << logNameOf(source) << "on the"
                      << (right ? "right" : "left");
    return paired;
}

} // namespace Kadunce
