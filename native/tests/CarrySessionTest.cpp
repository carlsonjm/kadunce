/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "CarrySession.h"
#include "MonitorDropIntent.h"
#include <cstdlib>
#include <iostream>
#include <limits>
using namespace Kadunce;
void require(bool value, const char *message)
{
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}
int main()
{
    const QRectF output(1280, 0, 1280, 800);
    require(monitorCarryEdge(output, {1281, 400}) == CarryEdge::Left, "Left edge missing");
    require(monitorCarryEdge(output, {2559, 400}) == CarryEdge::Right, "Right edge missing");
    require(monitorCarryEdge(output, {1900, 1}) == CarryEdge::Top, "Top edge missing");
    require(!monitorCarryEdge(output, {1900, 400}), "Interior became edge");
    require(!monitorCarryEdge(output, {1281, 799}), "Bottom corner stolen");
    require(!monitorCarryEdge(output, {2559, 760}), "Dock band stolen");
    require(!monitorCarryEdge(output, {1279, 400}), "Foreign output edge admitted");
    const CarryOrigin origin{QStringLiteral("card-a"), QStringLiteral("tablet"), 42, 3};
    const CarryDestination stack{CarryDestinationKind::StackGap, QStringLiteral("tablet"), QStringLiteral("stack-b"), 7, 2};
    const CarryDestination layout{CarryDestinationKind::LayoutSlot, QStringLiteral("monitor"), QStringLiteral("layout-c"), 9, 0};
    for (auto device : {CarryDevice::Pointer, CarryDevice::Touch}) {
        CarryOwner owner{device, 11, 1};
        CarryOwner foreign{device, 12, 1};
        CarrySession s;
        auto invalidOrigin = origin; invalidOrigin.restoreToken = 0;
        require(!s.begin(owner, invalidOrigin, {}, {}), "Missing restore token admitted");
        auto start = [&] { require(s.begin(owner, origin, {120,150}, {100,100}), "Begin failed"); };
        auto outcome = [&](CarryResolution expected) {
            auto result = s.takeOutcome();
            require(result && result->resolution == expected && result->origin == origin, "Wrong outcome or lost rollback token");
            require(!s.takeOutcome(), "Duplicate outcome delivered");
            require(!s.release(owner), "Late release resurrected session");
            return *result;
        };
        start();
        require(!s.begin(foreign, origin, {}, {}), "Another contact replaced owner");
        require(!s.move(foreign, {300,300}) && !s.release(foreign), "Foreign device changed carry");
        const CarryOwner otherContact{device, 11, 2};
        require(!s.move(otherContact, {300,300}) && !s.release(otherContact), "Foreign contact changed carry");
        require(s.move(owner, {-200,1200}) && s.position() == QPointF(-220,1150), "Carry is clamped or lost anchor");
        require(!s.move(owner, {std::numeric_limits<double>::quiet_NaN(),0}), "Nonfinite pose admitted");
        auto oldPreview = s.preview(stack);
        require(oldPreview.has_value() && s.origin() == origin, "Preview mutated source");
        auto newest = s.preview(layout);
        require(newest && *newest != *oldPreview, "Preview replacement reused ticket");
        auto request = s.release(owner);
        require(request && request->destination == layout && request->origin == origin, "Wrong released destination");
        require(!s.release(owner) && !s.move(owner,{0,0}) && !s.preview(stack), "Released transaction still editable");
        require(!s.resolve(*oldPreview,true,3,7), "Stale destination accepted");
        require(s.resolve(request->ticket,true,3,9), "Valid destination rejected");
        require(!s.resolve(request->ticket,true,3,9), "Duplicate acceptance succeeded");
        require(!s.begin(owner,origin,{},{}), "Unconsumed outcome overwritten");
        require(outcome(CarryResolution::ReadyToCommit).destination == layout, "Acceptance lost destination");

        start(); s.preview(stack); s.move(owner,{125,155});
        require(!s.release(owner), "Movement retained stale target");
        outcome(CarryResolution::ReturnToOrigin);
        start(); s.preview(stack); auto rejected = s.release(owner);
        require(s.resolve(rejected->ticket,false,3,7), "Rejection not handled");
        require(!outcome(CarryResolution::ReturnToOrigin).destination, "Rejection retained destination");
        start(); s.preview(stack); auto changed = s.release(owner);
        s.resolve(changed->ticket,true,3,8);
        outcome(CarryResolution::ReturnToOrigin);
        start(); s.preview(stack); auto sourceChanged = s.release(owner);
        s.resolve(sourceChanged->ticket,true,4,7);
        outcome(CarryResolution::NeedsRecovery);

        start(); s.preview(stack); auto canceled = s.release(owner); s.cancel(); s.cancel();
        require(!s.resolve(canceled->ticket,true,3,7), "Canceled acceptance succeeded");
        outcome(CarryResolution::ReturnToOrigin);
        start(); s.preview(stack); auto next = s.release(owner);
        require(!s.resolve(canceled->ticket,true,3,7), "Prior session callback accepted");
        s.resolve(next->ticket,true,3,7); outcome(CarryResolution::ReadyToCommit);

        for (bool released : {false,true}) {
            start(); s.preview(stack); if (released) s.release(owner);
            s.targetChanged(stack.target);
            if (!released) { require(!s.destination(), "Changed stack kept preview"); s.release(owner); }
            outcome(CarryResolution::ReturnToOrigin);
            start(); s.preview(layout); if (released) s.release(owner);
            s.outputRemoved(layout.output);
            if (!released) s.release(owner);
            outcome(CarryResolution::ReturnToOrigin);
            start(); s.preview(layout); if (released) s.release(owner);
            s.outputRemoved(origin.output); outcome(CarryResolution::NeedsRecovery);
            start(); s.preview(stack); if (released) s.release(owner);
            s.cardClosed(origin.card); outcome(CarryResolution::SourceGone);
        }
        start(); s.preview(stack); s.targetChanged(QStringLiteral("unrelated"));
        require(s.destination().has_value(), "Unrelated target invalidated preview");
        s.sourceChanged(); outcome(CarryResolution::NeedsRecovery);
        start(); s.preview(stack);
        auto invalid = stack; invalid.position = -1;
        require(!s.preview(invalid) && !s.destination(), "Invalid destination retained old preview");
        s.cancel(); outcome(CarryResolution::ReturnToOrigin);
        start();
        auto line = stack; line.kind = CarryDestinationKind::LineGap;
        s.preview(line); auto lineRequest = s.release(owner);
        require(s.resolve(lineRequest->ticket, true, 3, 7), "Line gap rejected");
        require(outcome(CarryResolution::ReadyToCommit).destination == line, "Line gap lost");

        const QString monitor = QStringLiteral("monitor");
        const auto native = monitorDropIntent(monitor, 10, std::nullopt);
        require(native && native->kind == CarryDestinationKind::NativeDesktop && !native->edge,
            "Open desktop inferred a snap");
        for (auto edge : {CarryEdge::Left, CarryEdge::Right, CarryEdge::Top, CarryEdge::Bottom}) {
            const auto snap = monitorDropIntent(monitor, 10, std::nullopt, edge);
            require(snap && snap->kind == CarryDestinationKind::NewLayoutEdge && snap->edge == edge,
                "Confirmed edge lost its meaning");
            const auto existing = monitorDropIntent(monitor, 10,
                MonitorLayoutTarget{layout.target, 9, 0}, edge);
            require(existing && *existing == layout, "Edge bypassed existing Bento");
            start(); require(s.preview(*snap).has_value(), "Edge preview rejected");
            auto drop = s.release(owner);
            require(drop && s.resolve(drop->ticket, true, 3, 10), "Edge acceptance failed");
            require(outcome(CarryResolution::ReadyToCommit).destination == snap, "Edge lost on commit");
            start(); s.preview(*snap); s.move(owner, {900,900});
            require(!s.release(owner), "Movement reused confirmed snap");
            outcome(CarryResolution::ReturnToOrigin);
        }
        for (auto destination : {*native, *monitorDropIntent(monitor, 10, std::nullopt, CarryEdge::Left)}) {
            start(); s.preview(destination); auto drop = s.release(owner);
            require(s.resolve(drop->ticket, true, 3, 11), "Topology revision not handled");
            outcome(CarryResolution::ReturnToOrigin);
            start(); s.preview(destination); drop = s.release(owner);
            s.outputRemoved(monitor);
            require(!s.resolve(drop->ticket, true, 3, 10), "Removed monitor accepted drop");
            outcome(CarryResolution::ReturnToOrigin);
            start(); s.preview(destination); drop = s.release(owner); s.cancel();
            require(!s.resolve(drop->ticket, true, 3, 10), "Canceled monitor drop accepted");
            outcome(CarryResolution::ReturnToOrigin);
        }
        start(); s.preview(*native); auto nativeDrop = s.release(owner);
        s.resolve(nativeDrop->ticket, true, 3, 10);
        require(outcome(CarryResolution::ReadyToCommit).destination == native, "Native destination lost");
        require(!monitorDropIntent({}, 10, std::nullopt)
            && !monitorDropIntent(monitor, 10, MonitorLayoutTarget{}, CarryEdge::Left)
            && !monitorDropIntent(monitor, 10, std::nullopt, static_cast<CarryEdge>(99)),
            "Malformed destination fell back to a different action");
        start();
        auto malformed = *native; malformed.edge = CarryEdge::Left;
        require(!s.preview(malformed), "Native destination accepted edge payload");
        malformed = *native; malformed.kind = CarryDestinationKind::NewLayoutEdge;
        require(!s.preview(malformed), "New layout accepted without explicit edge");
        s.cancel(); outcome(CarryResolution::ReturnToOrigin);
    }
}
