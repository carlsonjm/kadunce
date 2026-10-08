/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "CarryInputRoute.h"
#include <cstdlib>
#include <iostream>
using namespace Kadunce;
using A = CarryInputRoute::Action;
void check(bool ok) { if (!ok) { std::cerr << "Carry input route failure\n"; std::abort(); } }
int main()
{
    for (auto kind : {CarryDevice::Pointer, CarryDevice::Touch}) {
        const CarryOwner owner{kind, 2, 42}, other{kind, 3, 42}, extra{kind, 2, 43};
        const CarryOwner foreign{kind == CarryDevice::Touch ? CarryDevice::Pointer : CarryDevice::Touch, 1, 42};
        CarryInputRoute route;
        check(!route.acquire({kind,0,42}));
        check(route.acquire(owner));
        check(!route.acquire(other));
        check(route.motion(owner) == A::Move);
        check(route.down(foreign) == A::Pass && route.up(foreign) == A::Pass);
        check(route.motion(other) == A::Pass && route.up(other) == A::Pass);
        check(route.up(owner) == A::Release && !route.busy());
        check(route.up(owner) == A::Pass); // one drop, never two
        check(route.acquire(owner));
        check(route.cancel() == A::Cancel && route.draining());
        check(route.cancel() == A::Consume);
        check(route.motion(owner) == A::Consume);
        check(!route.acquire(owner));
        check(route.up(owner) == A::Consume && !route.busy());
        check(route.acquire(owner));
        check(route.down(extra) == A::Cancel && route.draining());
        check(route.up(owner) == A::Consume && route.busy());
        check(route.up(extra) == A::Consume && !route.busy());
        check(route.acquire(owner));
        check(route.down(owner) == A::Cancel); // duplicate ID is not a new gesture
        check(route.up(owner) == A::Consume && !route.busy());
        check(route.acquire(owner, false)); // interrupted native cancellation
        check(route.motion(owner) == A::Consume && route.up(owner) == A::Consume);
        check(route.acquire(owner));
        check(route.streamGone(kind,3) == A::Pass && route.active());
        check(route.streamGone(kind,2) == A::Cancel && !route.busy());
        check(route.acquire(owner)); // seat ID reuse after real stream cancellation
        check(route.up(owner) == A::Release);
    }
    {
        // A touch whose release this route never saw is forgotten when a new
        // down arrives under its id or it is no longer down, so the next
        // contact passes instead of draining as a second finger.
        const CarryOwner touch{CarryDevice::Touch, 1, 0}, pointer{CarryDevice::Pointer, 2, 1};
        const auto down = [](qint64) { return true; };
        const auto lifted = [](qint64) { return false; };
        CarryInputRoute route;
        check(route.forgetLiftedTouches(0, lifted) == A::Pass);
        check(route.acquire(touch) && route.cancel() == A::Cancel && route.draining());
        check(route.forgetLiftedTouches(5, down) == A::Pass && route.busy());
        check(route.forgetLiftedTouches(0, down) == A::Consume && !route.busy());
        check(route.down(touch) == A::Pass);
        check(route.acquire(touch));
        check(route.forgetLiftedTouches(5, lifted) == A::Cancel && !route.busy() && !route.active());
        check(route.acquire(pointer));
        check(route.forgetLiftedTouches(1, lifted) == A::Pass && route.active());
    }
    std::cout << "Carry input route checks passed\n";
}
