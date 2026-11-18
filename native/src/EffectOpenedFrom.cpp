/* SPDX-License-Identifier: GPL-2.0-or-later */
// Where a new window opened from (OpenedFrom.h). Kadunce reads it and writes
// it in the journal; no window is placed by it.
#include "EffectInternal.h"
#include "OpenedFrom.h"
#include <input_event.h>
#include <input_event_spy.h>
#include <wayland/xdgactivation_v1.h>

namespace Kadunce
{

// Hears every press, tap and key before anything acts on it, and acts on
// none. It only reads which window the press landed on.
struct Effect::OpenedFromSpy final : KWin::InputEventSpy {
    Effect *effect;
    explicit OpenedFromSpy(Effect *e) : effect(e) {}
    void pointerButton(KWin::PointerButtonEvent *event) override
    {
        if (event->state != KWin::PointerButtonState::Pressed || !KWin::input()) return;
        effect->notePressed(KWin::input()->findToplevel(event->position), "a click");
    }
    void touchDown(KWin::TouchDownEvent *event) override
    {
        if (KWin::input()) effect->notePressed(KWin::input()->findToplevel(event->pos), "a touch");
    }
    void tabletToolTipEvent(KWin::TabletToolTipEvent *event) override
    {
        if (event->type != KWin::TabletToolTipEvent::Press || !KWin::input()) return;
        effect->notePressed(KWin::input()->findToplevel(event->position), "a pen");
    }
    void keyboardKey(KWin::KeyboardKeyEvent *event) override
    {
        // A key lands on the window that holds the keys.
        if (event->state != KWin::KeyboardKeyState::Pressed || !KWin::workspace()) return;
        effect->notePressed(KWin::workspace()->activeWindow(), "a key");
    }
};

void Effect::watchOpenedFrom()
{
    if (!KWin::input() || m_openedFromSpy) return;
    m_openedFromSpy = std::make_shared<OpenedFromSpy>(this);
    KWin::input()->installInputEventSpy(m_openedFromSpy.get());
    // A ticket presented: by a window not yet shown, which keeps it until it
    // is, or by one already open that an application calls forward.
    if (auto *server = KWin::waylandServer())
        if (auto *activation = server->findChild<KWin::XdgActivationV1Interface *>())
            connect(activation, &KWin::XdgActivationV1Interface::activateRequested, this,
                    &Effect::ticketPresented);
    // A ticket given for an application the compositor announces as starting.
    connect(KWin::effects, &KWin::EffectsHandler::startupAdded, this, [this](const QString &ticket) {
        m_newestTicket = std::max(m_newestTicket, OpenedFrom::ticketNumber(ticket.toStdString()));
        qInfo().noquote() << "Kadunce" << Revision << "window origin: ticket" << ticket
                          << "given as an application starts;" << lastPressedWords(nullptr);
    });
}

void Effect::dropOpenedFromSpy()
{
    if (!m_openedFromSpy) return;
    if (KWin::input()) KWin::input()->uninstallInputEventSpy(m_openedFromSpy.get());
    m_openedFromSpy.reset();
}

void Effect::notePressed(KWin::Window *window, const char *how)
{
    m_lastPressed = window;
    m_lastPressedHow = how;
    m_lastPressedAt = steadyMs();
}

OpenedFrom::Pressed Effect::lastPressedFor(const KWin::EffectWindow *opening) const
{
    if (!m_lastPressedHow) return OpenedFrom::Pressed::Nothing;
    KWin::EffectWindow *pressed = m_lastPressed ? m_lastPressed->effectWindow() : nullptr;
    if (!pressed || pressed->isDeleted()) return OpenedFrom::Pressed::Elsewhere;
    // A press on a dialog is a press on the card it belongs to.
    if (KWin::EffectWindow *lead = dependentLead(pressed)) pressed = lead;
    if (opening && pressed == opening) return OpenedFrom::Pressed::Itself;
    return isApplicationWindow(pressed) ? OpenedFrom::Pressed::Card : OpenedFrom::Pressed::Elsewhere;
}

QString Effect::lastPressedWords(const KWin::EffectWindow *opening) const
{
    if (!m_lastPressedHow) return QStringLiteral("nothing pressed yet");
    return QStringLiteral("last pressed: %1 on %2 (%3), %4 ms before")
        .arg(QLatin1String(m_lastPressedHow),
             m_lastPressed ? logNameOf(m_lastPressed.data()) : QStringLiteral("no window"),
             QLatin1String(OpenedFrom::words(lastPressedFor(opening))))
        .arg(steadyMs() - m_lastPressedAt);
}

void Effect::ticketPresented(KWin::SurfaceInterface *surface, const QString &ticket)
{
    m_newestTicket = std::max(m_newestTicket, OpenedFrom::ticketNumber(ticket.toStdString()));
    KWin::Window *window = KWin::waylandServer() && surface ? KWin::waylandServer()->findWindow(surface) : nullptr;
    KWin::EffectWindow *presenting = window ? window->effectWindow() : nullptr;
    // A window not yet shown keeps its ticket and is written once, as it
    // opens (logOpenedFrom). One that presents it just after opening is the
    // same arrival, so the line says how long it has been open.
    if (!window || !window->readyForPainting()) return;
    const QString since = presenting && presenting == m_lastOpened
        ? QStringLiteral("opened %1 ms before,").arg(steadyMs() - m_lastOpenedAt) : QStringLiteral("already open,");
    qInfo().noquote() << "Kadunce" << Revision << "window origin:" << logNameOf(window) << since
                      << "called forward with ticket" << ticket << ';' << lastPressedWords(presenting);
}

void Effect::logOpenedFrom(KWin::EffectWindow *window)
{
    if (!window || !window->window() || !window->isNormalWindow()) return;
    m_lastOpened = window;
    m_lastOpenedAt = steadyMs();
    const QString ticket = window->window()->activationToken();
    QStringList askers;
    if (m_placementLaunch && m_placementLaunch->matches(applicationIdentity(window)))
        askers.append(QStringLiteral("a placement request"));
    if (m_launcherGuestLaunchPending) askers.append(QStringLiteral("Search"));
    if (m_layoutRun) askers.append(QStringLiteral("a saved layout"));
    if (m_bringBack) askers.append(QStringLiteral("a bring-back"));
    const OpenedFrom::Evidence evidence{!ticket.isEmpty(), lastPressedFor(window), !askers.isEmpty()};
    const OpenedFrom::Finding finding = OpenedFrom::find(evidence);
    qInfo().noquote() << "Kadunce" << Revision << "window origin:" << logNameOf(window) << "opened,"
                      << (window->isX11Client() ? "X11," : "Wayland,")
                      << (ticket.isEmpty() ? QStringLiteral("no ticket") : QStringLiteral("ticket ") + ticket)
                      << QStringLiteral("(newest seen %1);").arg(m_newestTicket)
                      << lastPressedWords(window) << ';'
                      << (askers.isEmpty() ? QStringLiteral("asked for by nothing")
                                           : QStringLiteral("asked for by ") + askers.join(QStringLiteral(", ")))
                      << "; finding:" << OpenedFrom::words(finding)
                      << (finding == OpenedFrom::Finding::Card && m_lastPressed
                              ? QStringLiteral("(%1)").arg(logNameOf(m_lastPressed.data())) : QString());
}

} // namespace Kadunce
