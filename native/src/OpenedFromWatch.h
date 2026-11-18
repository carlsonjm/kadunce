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
// Reads where each new window opened from (OpenedFrom.h) and writes it in the
// journal. Passive, on the compositor's own thread: it consumes no event and
// places no window.
class OpenedFromWatch final : public QObject
{
public:
    // Whether a window pressed is a card, and which programs asked for an
    // opening window by name. Both are answered by the effect that owns this.
    std::function<bool(KWin::EffectWindow *)> isCard;
    std::function<QStringList(KWin::EffectWindow *)> askers;

    explicit OpenedFromWatch(QObject *parent) : QObject(parent), m_spy(this)
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
            qInfo().noquote() << "Kadunce" << Revision << "window origin: ticket" << ticket
                              << "given as an application starts;" << pressedWords(nullptr);
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
            // A key lands on the window that holds the keys.
            if (event->state != KWin::KeyboardKeyState::Pressed || !KWin::workspace()) return;
            watch->pressed(KWin::workspace()->activeWindow(), "a key");
        }
    };

    void pressed(KWin::Window *window, const char *how)
    {
        m_pressed = window;
        m_pressedHow = how;
        m_pressedAt = steadyMs();
    }
    void seen(const QString &ticket)
    {
        m_newestTicket = std::max(m_newestTicket, OpenedFrom::ticketNumber(ticket.toStdString()));
    }
    [[nodiscard]] OpenedFrom::Pressed pressedFor(const KWin::EffectWindow *opening) const
    {
        if (!m_pressedHow) return OpenedFrom::Pressed::Nothing;
        KWin::EffectWindow *window = m_pressed ? m_pressed->effectWindow() : nullptr;
        if (!window || window->isDeleted()) return OpenedFrom::Pressed::Elsewhere;
        if (opening && window == opening) return OpenedFrom::Pressed::Itself;
        return isCard && isCard(window) ? OpenedFrom::Pressed::Card : OpenedFrom::Pressed::Elsewhere;
    }
    [[nodiscard]] QString pressedName() const
    {
        return m_pressed ? logNameOf(m_pressed.data()) : QStringLiteral("no window");
    }
    [[nodiscard]] QString pressedWords(const KWin::EffectWindow *opening) const
    {
        if (!m_pressedHow) return QStringLiteral("nothing pressed yet");
        return QStringLiteral("last pressed: %1 on %2 (%3), %4 ms before")
            .arg(QLatin1String(m_pressedHow), pressedName(), QLatin1String(OpenedFrom::words(pressedFor(opening))))
            .arg(steadyMs() - m_pressedAt);
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
        const QString since = presenting && presenting == m_opened
            ? QStringLiteral("opened %1 ms before,").arg(steadyMs() - m_openedAt) : QStringLiteral("already open,");
        qInfo().noquote() << "Kadunce" << Revision << "window origin:" << logNameOf(window) << since
                          << "called forward with ticket" << ticket << ';' << pressedWords(presenting);
    }

    void opened(KWin::EffectWindow *window)
    {
        if (!window || !window->window() || !window->isNormalWindow()) return;
        m_opened = window;
        m_openedAt = steadyMs();
        const QString ticket = window->window()->activationToken();
        const QStringList asked = askers ? askers(window) : QStringList();
        const OpenedFrom::Finding finding = OpenedFrom::find({!ticket.isEmpty(), pressedFor(window), !asked.isEmpty()});
        qInfo().noquote() << "Kadunce" << Revision << "window origin:" << logNameOf(window) << "opened,"
                          << (window->isX11Client() ? "X11," : "Wayland,")
                          << (ticket.isEmpty() ? QStringLiteral("no ticket") : QStringLiteral("ticket ") + ticket)
                          << QStringLiteral("(newest seen %1);").arg(m_newestTicket)
                          << pressedWords(window) << ';'
                          << (asked.isEmpty() ? QStringLiteral("asked for by nothing")
                                              : QStringLiteral("asked for by ") + asked.join(QStringLiteral(", ")))
                          << "; finding:" << OpenedFrom::words(finding)
                          << (finding == OpenedFrom::Finding::Card ? QStringLiteral("(%1)").arg(pressedName()) : QString());
    }

    Spy m_spy;
    // The window the person's latest press, tap or key landed on, how it was
    // made and when; the newest ticket seen; the window that opened last.
    QPointer<KWin::Window> m_pressed;
    const char *m_pressedHow = nullptr;
    qint64 m_pressedAt = 0;
    int m_newestTicket = -1;
    QPointer<KWin::EffectWindow> m_opened;
    qint64 m_openedAt = 0;
};
} // namespace Kadunce
