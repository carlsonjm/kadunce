#pragma once
#include "NativeMoveObserver.h"
#include "CarryInputRoute.h"
#include <QJsonDocument>
#include <QJsonObject>

// Private instrumentation around the shared passive runtime observer.
// Experimental early cancel filter remains private until ownership is integrated.
struct ContactProbe final : QObject, KWin::InputEventFilter {
    Kadunce::NativeMoveObserver observer;
    const Kadunce::CarryContacts &contacts = observer.contacts();
    int starts = 0, requests = 0, matches = 0, cancels = 0, decorationMatches = 0, xwaylandMatches = 0;
    QString matchedKind;
    qint64 matchedContact = -1;
    bool startedBeforeRequest = false;
    QPointer<KWin::Window> client;
    QPointer<KDecoration3::Decoration> watchedDecoration;
    std::function<void()> onNativeStart;
    std::function<void(Kadunce::CarryOwner, QPointF)> onIdentified;
    Kadunce::CarryInputRoute route;
    int moves = 0, releases = 0, routeCancels = 0;
    std::function<void(Kadunce::CarryInputRoute::Action, Kadunce::CarryOwner, QPointF)> onRoute;
    bool dispatch(Kadunce::CarryInputRoute::Action action, Kadunce::CarryOwner owner, QPointF pos = {}) {
        using A = Kadunce::CarryInputRoute::Action;
        if (action == A::Move) ++moves;
        if (action == A::Release) ++releases;
        if (action == A::Cancel) ++routeCancels;
        if (onRoute && (action == A::Move || action == A::Release || action == A::Cancel))
            onRoute(action, owner, pos);
        return action != A::Pass;
    }
    bool pointerButton(KWin::PointerButtonEvent *event) override {
        const Kadunce::CarryOwner owner{Kadunce::CarryDevice::Pointer,
            observer.deviceToken(event->device), event->nativeButton};
        return dispatch(event->state == KWin::PointerButtonState::Pressed
            ? route.down(owner) : route.up(owner), owner, event->position);
    }
    bool pointerMotion(KWin::PointerMotionEvent *event) override {
        auto owner = route.owner();
        owner.kind = Kadunce::CarryDevice::Pointer;
        owner.device = observer.deviceToken(event->device);
        return dispatch(route.motion(owner), owner, event->position);
    }
    bool touchDown(KWin::TouchDownEvent *event) override {
        const Kadunce::CarryOwner owner{Kadunce::CarryDevice::Touch,1,event->id};
        return dispatch(route.down(owner), owner, event->pos);
    }
    bool touchMotion(KWin::TouchMotionEvent *event) override {
        const Kadunce::CarryOwner owner{Kadunce::CarryDevice::Touch,1,event->id};
        return dispatch(route.motion(owner), owner, event->pos);
    }
    bool touchUp(KWin::TouchUpEvent *event) override {
        const Kadunce::CarryOwner owner{Kadunce::CarryDevice::Touch,1,event->id};
        return dispatch(route.up(owner), owner);
    }

    ContactProbe() : InputEventFilter(static_cast<KWin::InputFilterOrder::Order>(-1))
    {
        KWin::input()->installInputEventFilter(this);
        observer.started = [this](KWin::Window *window) {
            if (window != client) return;
            ++starts;
            if (onNativeStart) onNativeStart();
        };
        observer.requested = [this](KWin::Window *window) {
            if (window != client) return;
            ++requests;
            startedBeforeRequest = window->isInteractiveMove();
        };
        observer.identified = [this](KWin::Window *window, auto ticket, QPointF position,
                                     Kadunce::NativeMoveObserver::Proof proof) {
            if (window != client) return;
            if (proof == Kadunce::NativeMoveObserver::Proof::ApplicationRequest) ++matches;
            else if (proof == Kadunce::NativeMoveObserver::Proof::XwaylandRequest) ++xwaylandMatches;
            else ++decorationMatches;
            const auto owner = ticket.owner();
            matchedKind = owner.kind == Kadunce::CarryDevice::Touch ? "touch" : "pointer";
            matchedContact = owner.contact;
            if (onIdentified) onIdentified(owner, position);
        };
    }
    bool touchCancel() override {
        ++cancels; observer.cancelTouch();
        dispatch(route.streamGone(Kadunce::CarryDevice::Touch,1), route.owner());
        return false;
    }
    bool watch(KWin::Window *window)
    {
        observer.clearWindows(); observer.clearContacts();
        starts = requests = matches = cancels = decorationMatches = xwaylandMatches = 0;
        matchedKind.clear(); matchedContact = -1; startedBeforeRequest = false;
        client = window;
        return observer.watch(window);
    }
    bool prepareDecoration()
    {
        if (!client) return false;
        client->maximize(KWin::MaximizeRestore);
        client->setDecorationPolicy(KWin::DecorationPolicy::Server);
        client->moveResize({200,150,700,500});
        watchedDecoration = client->decoration();
        return bool(watchedDecoration); // observer follows decorationChanged automatically
    }
    QString state() const {
        return QString::fromUtf8(QJsonDocument(QJsonObject{
            {"starts",starts},{"requests",requests},{"matches",matches},{"kind",matchedKind},
            {"protocolAlive",bool(observer.protocolFor(client))}, {"clientAlive",bool(client)},
            {"decorationMatches",decorationMatches},
            {"xwaylandMatches",xwaylandMatches},
            {"contact",double(matchedContact)},{"cancel",cancels},
            {"candidate",bool(contacts.soleCandidate())},
            {"startedBeforeRequest",startedBeforeRequest},
            {"moving",client && client->isInteractiveMove()}
            ,{"frameY",client ? client->frameGeometry().y() : -1}
            ,{"clientY",client ? client->clientGeometry().y() : -1}
            ,{"frameX",client ? client->frameGeometry().x() : -1}
            ,{"frameWidth",client ? client->frameGeometry().width() : -1}
            ,{"decorated",bool(watchedDecoration)}
            ,{"titleX",watchedDecoration && client ? client->x() + watchedDecoration->titleBar().center().x() : -1}
            ,{"titleY",watchedDecoration && client ? client->y() + watchedDecoration->titleBar().center().y() : -1}
            ,{"titleHeight",watchedDecoration ? watchedDecoration->titleBar().height() : -1}
        }).toJson(QJsonDocument::Compact));
    }
};
