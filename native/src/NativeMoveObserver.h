/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "CarryContacts.h"
#include "NativeMoveProtocol.h"
#include <core/inputdevice.h>
#include <input.h>
#include <input_event.h>
#include <input_event_spy.h>
#include <touch_input.h>
#include <wayland/seat.h>
#include <wayland/surface.h>
#include <wayland_server.h>
#include <window.h>
#include <KDecoration3/Decoration>
#include <QEvent>
#include <QHash>
#include <QTimer>
#include <config-kwin.h>
#if KWIN_BUILD_X11
#include <main.h>
#include <workspace.h>
#include <x11window.h>
#include <x11eventfilter.h>
#include <utils/xcbutils.h>
#include <linux/input-event-codes.h>
#endif

namespace Kadunce {
// Passive, main-compositor-thread observation. Never consumes an event or changes
// native geometry. A separate owner routes/drains the stream after adoption.
class NativeMoveObserver final : public QObject {
public:
    enum class Proof { ApplicationRequest, DecorationMotion, XwaylandRequest };
    using Candidate = CarryContacts::Candidate;
    std::function<void(KWin::Window *)> started;
    std::function<void(KWin::Window *, Candidate, QPointF, Proof)> identified;
    std::function<void(KWin::Window *)> requested;
    std::function<void()> invalidated;
    std::function<void(CarryDevice, quint64)> streamRemoved;

    NativeMoveObserver() : m_spy(this)
    {
        KWin::input()->installInputEventSpy(&m_spy);
        connect(KWin::input(), &KWin::InputRedirection::deviceRemoved, this,
                [this](KWin::InputDevice *device) { removeDevice(device); });
        connect(KWin::input(), &KWin::InputRedirection::deviceAdded, this,
                [this](KWin::InputDevice *device) { token(device); });
        for (auto *device : KWin::input()->devices()) token(device);
#if KWIN_BUILD_X11
        connect(KWin::kwinApp(), &KWin::Application::x11ConnectionChanged, this, [this] { refreshX11(); });
        connect(KWin::kwinApp(), &KWin::Application::x11ConnectionAboutToBeDestroyed, this,
            [this] { m_x11Filter.reset(); m_x11Candidate.reset(); });
        refreshX11();
#endif
    }
    ~NativeMoveObserver() override { clearWindows(); }
    const CarryContacts &contacts() const { return m_contacts; }
    quint64 deviceToken(KWin::InputDevice *device) const { return m_devices.value(device).token; }
    void clearContacts()
    {
        m_contacts.clear(); m_pointerSurface.clear(); m_decorationCandidate.reset();
        if (invalidated) invalidated();
    }
    // InputEventSpy has no touchCancel. The caller must forward cancellation
    // before any consumer can swallow it; this class chooses no filter priority.
    void cancelTouch()
    {
        m_contacts.cancel(CarryDevice::Touch); m_decorationCandidate.reset();
        if (invalidated) invalidated();
    }
    bool watch(KWin::Window *window)
    {
        if (!window || window->isDeleted()) return false;
        if (m_windows.contains(window)) return bool(m_windows.value(window)->protocol);
        auto record = std::make_shared<Watch>();
        record->window = window;
        m_windows.insert(window, record);
        record->connections.push_back(connect(window, &QObject::destroyed, this,
            [this, window] { forget(window); }));
        record->connections.push_back(connect(window, &KWin::Window::surfaceChanged, this,
            [this, record] { refreshProtocol(record); }));
        record->connections.push_back(connect(window, &KWin::Window::decorationChanged, this,
            [this, record] { refreshDecoration(record); }));
        record->connections.push_back(connect(window, &KWin::Window::interactiveMoveResizeStarted, this,
            [this, record] {
                if (!record->window) return;
                if (started) started(record->window);
#if KWIN_BUILD_X11
                if (current(record) && qobject_cast<KWin::X11Window *>(record->window.data())
                    && record->window->isInteractiveMove()) {
                    m_x11Window = record->window;
                    m_x11Candidate = m_contacts.soleCandidate();
                    const auto generation = ++m_x11Event;
                    QTimer::singleShot(0, this, [this, generation] {
                        if (generation == m_x11Event) m_x11Candidate.reset();
                    });
                }
#endif
                // A consumer may unwatch/cancel during the native-start callback.
                // Server-side decoration proof also applies to Xwayland. Only
                // application-request proof requires an xdg-toplevel protocol.
                if (!current(record) || !record->window->isInteractiveMove()
                    || m_decorationWindow != record->window || !m_decorationCandidate) return;
                const auto ticket = *m_decorationCandidate;
                const auto contact = m_contacts.resolve(ticket);
                if (!contact) return;
                if (ticket.owner().kind == CarryDevice::Touch
                    && KWin::input()->touch()->decorationPressId() != ticket.owner().contact) return;
                if (identified) identified(record->window, ticket, contact->position, Proof::DecorationMotion);
            }));
        refreshProtocol(record);
        refreshDecoration(record);
        return bool(record->protocol);
    }
    void forget(KWin::Window *window)
    {
        const auto record = m_windows.take(window);
        if (!record) return;
        for (const auto &connection : record->connections) disconnect(connection);
        disconnect(record->requestConnection);
        disconnect(record->decorationDestroyed);
        if (record->decoration) {
            record->decoration->removeEventFilter(this);
            m_decorations.remove(record->decoration);
        }
        if (m_decorationWindow == window) m_decorationCandidate.reset();
    }
    void clearWindows()
    {
        const auto windows = m_windows.keys();
        for (auto *window : windows) forget(window);
    }
    QPointer<KWin::XdgToplevelInterface> protocolFor(KWin::Window *window) const
    {
        const auto record = m_windows.value(window);
        return record ? record->protocol : nullptr;
    }
private:
#if KWIN_BUILD_X11
    // EWMH has no Wayland serial. Require the sole live contact's actual input
    // surface and exact request window. KWin's RootInfo filter runs first, so
    // correlate its accepted move start with the request later in the SAME event
    // dispatch. This observer never consumes or replays the X11 request.
    struct X11Requests final : KWin::X11EventFilter {
        NativeMoveObserver *observer;
        KWin::Xcb::Atom moveAtom;
        explicit X11Requests(NativeMoveObserver *o)
            : X11EventFilter(XCB_CLIENT_MESSAGE), observer(o), moveAtom("_NET_WM_MOVERESIZE") {}
        bool event(xcb_generic_event_t *event) override {
            const auto *message = reinterpret_cast<xcb_client_message_event_t *>(event);
            if (message->type == moveAtom) observer->x11Requested(*message);
            return false;
        }
    };
    void refreshX11() {
        m_x11Filter.reset(); m_x11Candidate.reset();
        if (KWin::kwinApp()->x11Connection()) m_x11Filter = std::make_unique<X11Requests>(this);
    }
    void x11Requested(const xcb_client_message_event_t &message) {
        const auto ticket = m_x11Candidate;
        m_x11Candidate.reset();
        if (message.format != 32 || message.data.data32[2] != NET::Move) return;
        auto *window = KWin::workspace()->findClient(message.window);
        if (!window || window != m_x11Window || !window->isInteractiveMove()
            || !m_windows.contains(window) || !window->surface() || !ticket) return;
        const auto owner = ticket->owner();
        // Titlebar primary-button moves only. Other buttons and keyboard/resize
        // requests keep their native path; button 0 means unspecified in EWMH.
        const auto button = message.data.data32[3];
        if (button != 0 && button != 1) return;
        const bool matches = owner.kind == CarryDevice::Pointer
            ? (owner.contact == BTN_LEFT && m_pointerOwner == owner && m_pointerSurface == window->surface())
            : (m_touchOwner == owner && m_touchWindow == window);
        if (!matches) return;
        const auto contact = m_contacts.resolve(*ticket);
        if (contact && identified) identified(window, *ticket, contact->position, Proof::XwaylandRequest);
    }
    std::unique_ptr<X11Requests> m_x11Filter;
    QPointer<KWin::Window> m_x11Window;
    std::optional<Candidate> m_x11Candidate;
    quint64 m_x11Event = 0;
#endif
    struct Watch {
        QPointer<KWin::Window> window;
        QPointer<KWin::XdgToplevelInterface> protocol;
        QPointer<KDecoration3::Decoration> decoration;
        QList<QMetaObject::Connection> connections;
        QMetaObject::Connection requestConnection;
        QMetaObject::Connection decorationDestroyed;
    };
    bool current(const std::shared_ptr<Watch> &record) const
    {
        return record->window && !record->window->isDeleted()
            && m_windows.value(record->window) == record;
    }
    void refreshProtocol(const std::shared_ptr<Watch> &record)
    {
        disconnect(record->requestConnection);
        record->protocol = nativeMoveProtocol(record->window ? record->window->surface() : nullptr);
        if (!record->protocol) return;
        record->requestConnection = connect(record->protocol, &KWin::XdgToplevelInterface::moveRequested,
            this, [this, record](KWin::SeatInterface *seat, quint32 serial) {
                if (!current(record)) return;
                if (requested) requested(record->window);
                const auto ticket = m_contacts.soleCandidate();
                if (!current(record) || !ticket || !record->window->isInteractiveMove()
                    || !seat || seat != KWin::waylandServer()->seat()) return;
                const auto owner = ticket->owner();
                bool match = false;
                if (owner.kind == CarryDevice::Touch) {
                    const auto *point = seat->touchPointByImplicitGrabSerial(serial);
                    match = point && point->id == owner.contact && point->surface == record->window->surface();
                } else {
                    match = seat->hasImplicitPointerGrab(serial)
                        && seat->pointerButtonSerial(quint32(owner.contact)) == serial
                        && m_pointerOwner == owner && m_pointerSurface == record->window->surface();
                }
                const auto contact = m_contacts.resolve(*ticket);
                if (match && contact && identified)
                    identified(record->window, *ticket, contact->position, Proof::ApplicationRequest);
            });
    }
    void refreshDecoration(const std::shared_ptr<Watch> &record)
    {
        disconnect(record->decorationDestroyed);
        if (record->decoration) {
            record->decoration->removeEventFilter(this);
            m_decorations.remove(record->decoration);
        }
        record->decoration = record->window ? record->window->decoration() : nullptr;
        if (record->decoration) {
            m_decorations.insert(record->decoration, record->window);
            record->decoration->installEventFilter(this);
            auto *decoration = record->decoration.data();
            record->decorationDestroyed = connect(decoration, &QObject::destroyed, this,
                [this, decoration] { m_decorations.remove(decoration); });
        }
        if (m_decorationWindow == record->window) m_decorationCandidate.reset();
    }
    bool eventFilter(QObject *object, QEvent *event) override
    {
        if (m_decorations.contains(object)
            && (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseMove
                || event->type() == QEvent::HoverMove)) {
            m_decorationWindow = m_decorations.value(object);
            m_decorationCandidate = m_contacts.soleCandidate();
            const auto generation = ++m_decorationEvent;
            QTimer::singleShot(0, this, [this, generation] {
                if (generation == m_decorationEvent) m_decorationCandidate.reset();
            });
        }
        return false;
    }
    quint64 token(KWin::InputDevice *device)
    {
        if (!device) return 0;
        if (!m_devices.contains(device)) {
            m_devices.insert(device, {++m_nextDevice, device->isTouch()});
            connect(device, &QObject::destroyed, this, [this, device] { removeDevice(device); });
        }
        return m_devices.value(device).token;
    }
    void removeDevice(KWin::InputDevice *device)
    {
        const auto removed = m_devices.take(device);
        if (!removed.token) return;
        m_contacts.removeDevice(removed.token);
        // Touch is seat-scoped: losing any touchscreen invalidates the whole
        // reservation. Cache capability while alive; never call virtual methods
        // from QObject::destroyed. Duplicate removal is harmless.
        if (removed.touch) m_contacts.cancel(CarryDevice::Touch);
        if (m_pointerOwner.device == removed.token) m_pointerSurface.clear();
        m_decorationCandidate.reset();
        if (invalidated) invalidated();
        if (streamRemoved) {
            streamRemoved(CarryDevice::Pointer, removed.token);
            if (removed.touch) streamRemoved(CarryDevice::Touch, 1);
        }
    }
    struct Spy final : KWin::InputEventSpy {
        NativeMoveObserver *observer;
        explicit Spy(NativeMoveObserver *o) : observer(o) {}
        void pointerButton(KWin::PointerButtonEvent *event) override
        {
            const CarryOwner owner{CarryDevice::Pointer, observer->token(event->device), event->nativeButton};
            if (event->state == KWin::PointerButtonState::Pressed) {
                observer->m_pointerOwner = owner;
                observer->m_pointerSurface = KWin::waylandServer()->seat()->focusedPointerSurface();
                observer->m_contacts.press(owner, event->position);
            } else {
                observer->m_contacts.release(owner);
                if (observer->m_pointerOwner == owner) observer->m_pointerSurface.clear();
            }
        }
        void pointerMotion(KWin::PointerMotionEvent *event) override
        {
            const auto ticket = observer->m_contacts.soleCandidate();
            if (!ticket || ticket->owner().kind != CarryDevice::Pointer
                || ticket->owner().device != observer->token(event->device)) return;
            observer->m_contacts.motion(ticket->owner(), event->position);
        }
        void touchDown(KWin::TouchDownEvent *event) override
        {
            observer->m_touchOwner = {CarryDevice::Touch, 1, event->id};
            observer->m_touchWindow = KWin::input()->touch()->focus();
            observer->m_contacts.press(observer->m_touchOwner, event->pos);
        }
        void touchMotion(KWin::TouchMotionEvent *event) override
        {
            const CarryOwner owner{CarryDevice::Touch, 1, event->id};
            observer->m_contacts.motion(owner, event->pos);
        }
        void touchUp(KWin::TouchUpEvent *event) override
        {
            const CarryOwner owner{CarryDevice::Touch, 1, event->id};
            observer->m_contacts.release(owner);
        }
        void keyboardKey(KWin::KeyboardKeyEvent *) override { observer->m_decorationCandidate.reset(); }
    };
    CarryContacts m_contacts;
    struct Device { quint64 token = 0; bool touch = false; };
    QHash<KWin::InputDevice *, Device> m_devices;
    quint64 m_nextDevice = 1; // 1 is the explicit seat-scoped touch stream.
    QHash<KWin::Window *, std::shared_ptr<Watch>> m_windows;
    QHash<QObject *, QPointer<KWin::Window>> m_decorations;
    CarryOwner m_pointerOwner{CarryDevice::Pointer, 0, 0};
    QPointer<KWin::SurfaceInterface> m_pointerSurface;
    CarryOwner m_touchOwner{CarryDevice::Touch, 1, -1};
    QPointer<KWin::Window> m_touchWindow;
    QPointer<KWin::Window> m_decorationWindow;
    std::optional<Candidate> m_decorationCandidate;
    quint64 m_decorationEvent = 0;
    Spy m_spy; // uninstalls before contact storage is destroyed
};
} // namespace Kadunce
