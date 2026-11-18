/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "EffectInternal.h"
#include "OpenedFrom.h"
#include <input_event.h>
#include <input_event_spy.h>
#include <wayland/xdgactivation_v1.h>
#include <functional>

namespace Kadunce
{
// Reads where each new window opened from (OpenedFrom.h), writes it in the
// journal and keeps the card it names for the effect that owns this, which
// alone places windows. On the compositor's own thread; it consumes no event.
class OpenedFromWatch final : public QObject
{
public:
    // The card a window pressed is, or belongs to as a dialog does, none when
    // it is no card; and which programs asked for an opening window by name.
    // Both are answered by the effect that owns this.
    std::function<KWin::EffectWindow *(KWin::EffectWindow *)> cardOf;
    std::function<QStringList(KWin::EffectWindow *)> askers;
    // Told when a ticket that follows its window names a card for it.
    std::function<void(KWin::EffectWindow *)> lateCard;
    // Whether a window opened from a card joins that card's Stack: kept in
    // kaduncerc's [Stacks] group, off unless switched on.
    bool joins = KSharedConfig::openConfig(QStringLiteral("kaduncerc"))->group(QStringLiteral("Stacks"))
        .readEntry(QStringLiteral("JoinOpener"), false);

    // The card a window was found to be opened from, until it is forgotten.
    [[nodiscard]] KWin::EffectWindow *openerOf(const KWin::EffectWindow *opened) const
    {
        for (const auto &[window, card] : m_openers)
            if (window.data() == opened && card && !card->isDeleted()) return card.data();
        return nullptr;
    }
    void forget(const KWin::EffectWindow *opened)
    {
        m_openers.removeIf([opened](const auto &entry) {
            return !entry.first || !entry.second || entry.first.data() == opened;
        });
    }

    explicit OpenedFromWatch(QObject *parent) : QObject(parent), m_spy(this), m_received(this)
    {
        if (KWin::input()) KWin::input()->installInputEventSpy(&m_spy);
        connect(KWin::effects, &KWin::EffectsHandler::windowAdded, this,
                [this](KWin::EffectWindow *window) { opened(window); });
        // A ticket presented: by a window not yet shown, which keeps it until
        // it is, or by one already open that an application calls forward.
        if (auto *server = KWin::waylandServer())
            if (auto *activation = server->findChild<KWin::XdgActivationV1Interface *>())
                connect(activation, &KWin::XdgActivationV1Interface::activateRequested, this,
                        [this](KWin::SurfaceInterface *surface, const QString &ticket) { presented(surface, ticket); });
        // A ticket given for an application the compositor announces as starting.
        connect(KWin::effects, &KWin::EffectsHandler::startupAdded, this, [this](const QString &ticket) {
            seen(ticket);
            // The press it was given for is the latest one now, whatever is
            // pressed before the application's window opens.
            if (const int number = OpenedFrom::ticketNumber(ticket.toStdString()); number >= 0) {
                if (m_given.size() >= TicketsKept) m_given.removeFirst();
                m_given.append(std::pair<int, Press>(number, pressedNow(nullptr)));
            }
            qInfo().noquote() << "Kadunce" << Revision << "window origin: ticket" << ticket
                              << "given as an application starts;" << pressedWords(pressedNow(nullptr));
        });
    }
    ~OpenedFromWatch() override
    {
        if (KWin::input()) KWin::input()->uninstallInputEventSpy(&m_spy);
    }

private:
    // Hears every press, tap and key before anything acts on it, and acts on
    // none. It only reads which window the press landed on.
    struct Spy final : KWin::InputEventSpy {
        OpenedFromWatch *watch;
        explicit Spy(OpenedFromWatch *w) : watch(w) {}
        void pointerButton(KWin::PointerButtonEvent *event) override
        {
            if (event->state != KWin::PointerButtonState::Pressed || !KWin::input()) return;
            watch->pressed(KWin::input()->findToplevel(event->position), "a click");
        }
        void touchDown(KWin::TouchDownEvent *event) override
        {
            if (KWin::input()) watch->pressed(KWin::input()->findToplevel(event->pos), "a touch");
        }
        void tabletToolTipEvent(KWin::TabletToolTipEvent *event) override
        {
            if (event->type != KWin::TabletToolTipEvent::Press || !KWin::input()) return;
            watch->pressed(KWin::input()->findToplevel(event->position), "a pen");
        }
        void keyboardKey(KWin::KeyboardKeyEvent *event) override
        {
            // A key is a shortcut's until a window is seen to receive it
            // (Received), which follows in the same turn when one does.
            if (event->state != KWin::KeyboardKeyState::Pressed) return;
            watch->pressed(nullptr, "a key");
            watch->m_shortcut = true;
        }
    };
    // After the desktop's shortcuts and every effect in KWin's filter order,
    // so a key that reaches it is on its way to the window that holds the
    // keys. It takes nothing.
    struct Received final : KWin::InputEventFilter {
        OpenedFromWatch *watch;
        explicit Received(OpenedFromWatch *w) : InputEventFilter(KWin::InputFilterOrder::Popup), watch(w)
        {
            if (KWin::input()) KWin::input()->installInputEventFilter(this);
        }
        bool keyboardKey(KWin::KeyboardKeyEvent *event) override
        {
            if (event->state == KWin::KeyboardKeyState::Pressed && KWin::workspace())
                watch->pressed(KWin::workspace()->activeWindow(), "a key");
            return false;
        }
    };

    void pressed(KWin::Window *window, const char *how)
    {
        m_pressed = window;
        m_pressedHow = how;
        m_pressedAt = steadyMs();
        m_shortcut = false;
    }
    // A press as it was read: what it landed on, the card that was, how it
    // was made and when.
    struct Press {
        OpenedFrom::Pressed on = OpenedFrom::Pressed::Nothing;
        QPointer<KWin::EffectWindow> card;
        QString name;
        const char *how = nullptr;
        qint64 at = 0;
    };
    void seen(const QString &ticket)
    {
        const int number = OpenedFrom::ticketNumber(ticket.toStdString());
        m_newestTicket = std::max(m_newestTicket, number);
        if (number < 0 || std::any_of(m_seen.cbegin(), m_seen.cend(),
                [number](const OpenedFrom::TicketSeen &other) { return other.number == number; })) return;
        if (m_seen.size() >= TicketsKept) m_seen.removeFirst();
        m_seen.append(OpenedFrom::TicketSeen{number, steadyMs()});
    }
    // The latest press, for a window opening or for none.
    [[nodiscard]] Press pressedNow(const KWin::EffectWindow *opening) const
    {
        Press press;
        if (!m_pressedHow) return press;
        press.how = m_pressedHow;
        press.at = m_pressedAt;
        press.on = OpenedFrom::Pressed::Shortcut;
        press.name = QStringLiteral("no window, a shortcut took it");
        if (m_shortcut) return press;
        KWin::EffectWindow *window = m_pressed ? m_pressed->effectWindow() : nullptr;
        press.on = OpenedFrom::Pressed::Elsewhere;
        press.name = m_pressed ? logNameOf(m_pressed.data()) : QStringLiteral("no window");
        if (!window || window->isDeleted()) return press;
        KWin::EffectWindow *card = cardOf ? cardOf(window) : nullptr;
        if (opening && (window == opening || card == opening)) press.on = OpenedFrom::Pressed::Itself;
        else if (card) {
            press.on = OpenedFrom::Pressed::Card;
            press.card = card;
        }
        return press;
    }
    // The press a window's ticket is for: the one read when the ticket was
    // given, where that was announced, else the latest, unless a ticket given
    // after this one was seen before it.
    [[nodiscard]] Press pressFor(const QString &ticket, const KWin::EffectWindow *opening) const
    {
        const int number = OpenedFrom::ticketNumber(ticket.toStdString());
        for (const auto &[given, press] : m_given) {
            if (number < 0 || given != number) continue;
            Press then = press;
            if (then.on != OpenedFrom::Pressed::Card) return then;
            if (!then.card || then.card->isDeleted()) then.on = OpenedFrom::Pressed::Elsewhere;
            else if (then.card.data() == opening) then.on = OpenedFrom::Pressed::Itself;
            return then;
        }
        Press now = pressedNow(opening);
        if (number >= 0 && now.on == OpenedFrom::Pressed::Card
            && OpenedFrom::givenBeforePress(number, now.at, m_seen)) {
            now.on = OpenedFrom::Pressed::Later;
            now.card.clear();
        }
        return now;
    }
    [[nodiscard]] static QString pressedWords(const Press &press)
    {
        if (!press.how) return QStringLiteral("nothing pressed yet");
        return QStringLiteral("pressed: %1 on %2 (%3), %4 ms before")
            .arg(QLatin1String(press.how), press.name, QLatin1String(OpenedFrom::words(press.on)))
            .arg(steadyMs() - press.at);
    }
    // Reads a window's origin from its ticket, keeps the card it names and
    // says both in words.
    [[nodiscard]] QString found(KWin::EffectWindow *window, const QString &ticket, bool &card)
    {
        const QStringList asked = askers ? askers(window) : QStringList();
        const Press press = pressFor(ticket, window);
        const OpenedFrom::Finding finding = OpenedFrom::find(
            {OpenedFrom::ticketNumber(ticket.toStdString()) >= 0, press.on, !asked.isEmpty()});
        card = finding == OpenedFrom::Finding::Card && press.card;
        forget(window);
        if (card) m_openers.append(std::pair(QPointer<KWin::EffectWindow>(window), press.card));
        return pressedWords(press) + QStringLiteral(" ; ")
            + (asked.isEmpty() ? QStringLiteral("asked for by nothing")
                               : QStringLiteral("asked for by ") + asked.join(QStringLiteral(", ")))
            + QStringLiteral(" ; finding: ") + QLatin1String(OpenedFrom::words(finding))
            + (card ? QStringLiteral(" (%1)").arg(press.name) : QString());
    }

    void presented(KWin::SurfaceInterface *surface, const QString &ticket)
    {
        seen(ticket);
        KWin::Window *window = KWin::waylandServer() && surface ? KWin::waylandServer()->findWindow(surface) : nullptr;
        // A window not yet shown keeps its ticket and is written once, as it
        // opens. One that presents it just after opening is the same arrival,
        // so the line says how long it has been open.
        if (!window || !window->readyForPainting()) return;
        KWin::EffectWindow *presenting = window->effectWindow();
        const bool justOpened = presenting && presenting == m_opened && steadyMs() - m_openedAt <= LateTicketMs;
        const QString since = justOpened
            ? QStringLiteral("opened %1 ms before,").arg(steadyMs() - m_openedAt) : QStringLiteral("already open,");
        // A ticket that follows its window by a moment is that window's own,
        // read as it would have been had it come first.
        if (!justOpened || m_openedWithTicket) {
            qInfo().noquote() << "Kadunce" << Revision << "window origin:" << logNameOf(window) << since
                              << "called forward with" << ticketWords(ticket) << ';'
                              << pressedWords(pressFor(ticket, presenting));
            return;
        }
        m_openedWithTicket = OpenedFrom::ticketNumber(ticket.toStdString()) >= 0;
        bool card = false;
        qInfo().noquote() << "Kadunce" << Revision << "window origin:" << logNameOf(window) << since
                          << "called forward with its late" << ticketWords(ticket) << ';'
                          << found(presenting, ticket, card);
        if (card && lateCard) lateCard(presenting);
    }
    [[nodiscard]] static QString ticketWords(const QString &ticket)
    {
        if (ticket.isEmpty()) return QStringLiteral("no ticket");
        return (OpenedFrom::ticketNumber(ticket.toStdString()) >= 0 ? QStringLiteral("ticket ")
                                                                    : QStringLiteral("a refused ticket ")) + ticket;
    }

    void opened(KWin::EffectWindow *window)
    {
        if (!window || !window->window() || !window->isNormalWindow()) return;
        m_opened = window;
        m_openedAt = steadyMs();
        const QString ticket = window->window()->activationToken();
        seen(ticket);
        m_openedWithTicket = OpenedFrom::ticketNumber(ticket.toStdString()) >= 0;
        bool card = false;
        qInfo().noquote() << "Kadunce" << Revision << "window origin:" << logNameOf(window) << "opened,"
                          << (window->isX11Client() ? "X11," : "Wayland,")
                          << ticketWords(ticket)
                          << QStringLiteral("(newest seen %1);").arg(m_newestTicket)
                          << found(window, ticket, card);
    }

    // How long after a window opens a ticket it presents is still its own.
    static constexpr qint64 LateTicketMs = 1000;
    // How many tickets are remembered, the oldest forgotten first.
    static constexpr qsizetype TicketsKept = 64;
    Spy m_spy;
    Received m_received;
    // The latest press was a key no window has been seen to receive.
    bool m_shortcut = false;
    bool m_openedWithTicket = false;
    // The window the person's latest press, tap or key landed on, how it was
    // made and when; the newest ticket seen; the window that opened last.
    QPointer<KWin::Window> m_pressed;
    const char *m_pressedHow = nullptr;
    qint64 m_pressedAt = 0;
    int m_newestTicket = -1;
    QPointer<KWin::EffectWindow> m_opened;
    qint64 m_openedAt = 0;
    // Each ticket seen and when; the press each was given for, where the
    // compositor announced the giving; and the card each window opened from.
    QList<OpenedFrom::TicketSeen> m_seen;
    QList<std::pair<int, Press>> m_given;
    QList<std::pair<QPointer<KWin::EffectWindow>, QPointer<KWin::EffectWindow>>> m_openers;
};
} // namespace Kadunce
