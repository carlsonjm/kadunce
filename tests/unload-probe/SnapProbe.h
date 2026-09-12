#pragma once

#include "NativeMoveTakeover.h"
#include <effect/effecthandler.h>
#include <input.h>
#include <input_event.h>
#include <options.h>
#include <outline.h>
#include <window.h>
#include <workspace.h>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>

// Experiment only. Never installed or wired into Effect. Cancelling native move
// is an ownership handoff, NOT a no-snap flag that preserves native dragging.
struct SnapProbe final : KWin::InputEventFilter {
    SnapProbe() : InputEventFilter(KWin::InputFilterOrder::Effects) {}
    QPointer<KWin::Window> client;
    Kadunce::NativeMoveTakeover carry;
    Kadunce::CarryOwner owner{Kadunce::CarryDevice::Pointer, 1, 272};
    QPointF anchorContact;
    KWin::RectF origin;
    bool armed = false;
    bool adopted = false;
    bool released = false;
    int nativeSteps = 0;
    int nativeFinishes = 0;
    int guardedFinishes = 0;
    QString interruptMode;
    bool nestedRejected = false;
    int outlines = 0;
    QMetaObject::Connection steps, finishes, outline;

    ~SnapProbe() override {
        QObject::disconnect(steps);
        QObject::disconnect(finishes);
        QObject::disconnect(outline);
    }
    bool start(bool intercept, bool touch) {
        if (KWin::workspace()->moveResizeWindow()) return false;
        carry.cancel();
        carry.takeOutcome();
        QObject::disconnect(steps);
        QObject::disconnect(finishes);
        QObject::disconnect(outline);
        for (auto *w : KWin::effects->stackingOrder()) {
            if (w->isNormalWindow() && !w->isDeleted() && w->window()) {
                client = w->window(); break;
            }
        }
        if (!client) return false;
        client->setQuickTileMode(KWin::QuickTileMode(), {500,350});
        client->maximize(KWin::MaximizeRestore);
        client->moveResize({200,150,600,400});
        armed = intercept; adopted = released = false;
        owner = {touch ? Kadunce::CarryDevice::Touch : Kadunce::CarryDevice::Pointer,
                 1, touch ? 42 : 272};
        nativeSteps = nativeFinishes = guardedFinishes = outlines = 0;
        interruptMode.clear(); nestedRejected = false;
        steps = QObject::connect(client, &KWin::Window::interactiveMoveResizeStepped,
            client, [this] { ++nativeSteps; });
        finishes = QObject::connect(client, &KWin::Window::interactiveMoveResizeFinished,
            client, [this] {
                ++nativeFinishes;
                if (!carry.ownsNativeFinish(client)) return;
                ++guardedFinishes;
                if (interruptMode == QStringLiteral("cancel")) carry.cancel();
                if (interruptMode == QStringLiteral("source")) carry.invalidateSource();
                if (interruptMode == QStringLiteral("output"))
                    Q_EMIT KWin::effects->screenRemoved(client->output());
                if (interruptMode == QStringLiteral("nested")) {
                    nestedRejected = carry.adopt(client, owner,
                        {client->internalId().toString(), client->output()->name(), 2, 1},
                        anchorContact, origin.topLeft(), [] { return true; })
                        == Kadunce::NativeMoveTakeover::Result::Rejected;
                }
            });
        outline = QObject::connect(KWin::workspace()->outline(), &KWin::Outline::activeChanged,
            client, [this] { if (KWin::workspace()->outline()->isActive()) ++outlines; });
        return true;
    }
    bool beginMove() {
        if (!client || KWin::workspace()->moveResizeWindow()) return false;
        KWin::workspace()->performWindowOperation(client, KWin::Options::MoveOp);
        return true;
    }
    bool capture() {
        if (!client || !client->isInteractiveMove()) return false;
        anchorContact = KWin::input()->globalPointer();
        origin = client->frameGeometry();
        return true;
    }
    bool move(const QPointF &p, Kadunce::CarryDevice device, qint64 contact) {
        if (!armed || released || device != owner.kind || contact != owner.contact) return false;
        if (!adopted) {
            if (!client || KWin::workspace()->moveResizeWindow() != client
                || !client->isInteractiveMove()) return false;
            const auto result = carry.adopt(client, owner, {client->internalId().toString(),
                    client->output()->name(), 1, 1}, anchorContact, origin.topLeft(),
                    [this] { return interruptMode != QStringLiteral("reject"); });
            if (result == Kadunce::NativeMoveTakeover::Result::Rejected) return false;
            adopted = true; // Interrupted takeovers still own/drain their input.
        }
        carry.move(owner, p);
        return true;
    }
    bool pointerMotion(KWin::PointerMotionEvent *e) override {
        return move(e->position, Kadunce::CarryDevice::Pointer, 272);
    }
    bool touchMotion(KWin::TouchMotionEvent *e) override {
        return move(e->pos, Kadunce::CarryDevice::Touch, e->id);
    }
    bool keyboardKey(KWin::KeyboardKeyEvent *e) override {
        if (!armed || released) return false;
        // KWin's keyboard move filter also calls updateInteractiveMoveResize.
        // Acquire ownership before allowing that alternate entry point to run.
        if (!adopted && !move(anchorContact, owner.kind, owner.contact)) return false;
        if (e->key == Qt::Key_Escape) carry.cancel();
        return true;
    }
    bool pointerButton(KWin::PointerButtonEvent *e) override {
        if (!adopted || owner.kind != Kadunce::CarryDevice::Pointer) return false;
        if (e->state == KWin::PointerButtonState::Released && e->button == Qt::LeftButton) {
            carry.release(owner); released = true;
        }
        return true;
    }
    bool touchUp(KWin::TouchUpEvent *e) override {
        if (!adopted || owner.kind != Kadunce::CarryDevice::Touch || e->id != owner.contact) return false;
        carry.release(owner); released = true; return true;
    }
    QString state() const {
        const auto pose = carry.position();
        return QString::fromUtf8(QJsonDocument(QJsonObject{
            {"moving", client && client->isInteractiveMove()},
            {"tiled", client && client->quickTileMode() != KWin::QuickTileMode()},
            {"maximized", client && client->requestedMaximizeMode() != KWin::MaximizeRestore},
            {"outline", KWin::workspace()->outline()->isActive()},
            {"outlineCount", outlines}, {"steps", nativeSteps}, {"finishes", nativeFinishes},
            {"guardedFinishes", guardedFinishes}, {"busy", carry.busy()},
            {"finishGuardCleared", !carry.ownsNativeFinish(client)},
            {"nestedRejected", nestedRejected},
            {"snapshotPreserved", carry.snapshot().window == client
                && carry.snapshot().geometry == origin},
            {"adopted", adopted}, {"released", released},
            {"poseX", pose.x()}, {"poseY", pose.y()},
            {"anchorX", anchorContact.x() - origin.x()},
            {"anchorY", anchorContact.y() - origin.y()},
            {"nativeUnmoved", client && client->frameGeometry() == origin},
            {"nativeX", client ? client->frameGeometry().x() : 0},
            {"nativeY", client ? client->frameGeometry().y() : 0},
            {"electricTiling", KWin::options->electricBorderTiling()},
            {"electricMaximize", KWin::options->electricBorderMaximize()}
        }).toJson(QJsonDocument::Compact));
    }
    bool foreignOwnerRejected() {
        const auto position = carry.position();
        auto foreign = owner; ++foreign.device;
        const bool moved = carry.move(foreign, {900,900});
        const auto dropped = carry.release(foreign);
        return !moved && !dropped && carry.busy() && carry.position() == position;
    }
    QString outcome() {
        const auto outcome = carry.takeOutcome();
        if (!outcome) return QStringLiteral("none");
        return QString::fromUtf8(QJsonDocument(QJsonObject{
            {"resolution", int(outcome->resolution)},
            {"source", outcome->origin.card},
            {"restoreToken", int(outcome->origin.restoreToken)},
            {"revision", int(outcome->origin.revision)}
        }).toJson(QJsonDocument::Compact));
    }
};
