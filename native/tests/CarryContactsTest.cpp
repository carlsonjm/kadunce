/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "CarryContacts.h"
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
    const CarryOwner mouse{CarryDevice::Pointer, 1, 272};
    const CarryOwner secondMouse{CarryDevice::Pointer, 2, 272};
    const CarryOwner finger{CarryDevice::Touch, 3, 0};
    const CarryOwner secondFinger{CarryDevice::Touch, 3, 1};
    for (const auto owner : {mouse, finger}) {
        CarryContacts contacts;
        require(!contacts.soleCandidate(), "Empty stream invented candidate");
        require(!contacts.motion(owner, {10,20}), "Motion invented missing down");
        require(contacts.press(owner, {10,20}), "Valid down rejected");
        const auto ticket = *contacts.soleCandidate();
        require(contacts.motion(owner, {-500,2000}), "Unbounded motion rejected");
        require(contacts.resolve(ticket)->position == QPointF(-500,2000), "Position stale or clamped");
        CarryContacts foreign;
        foreign.press(owner, {});
        require(!foreign.resolve(ticket), "Other ledger accepted ticket");
        contacts.release(owner);
        contacts.press(owner, {});
        require(!contacts.resolve(ticket), "Reused contact resurrected ticket");
        const auto next = *contacts.soleCandidate();
        require(!contacts.press(owner, {}), "Duplicate down accepted");
        require(!contacts.resolve(next) && !contacts.soleCandidate(), "Duplicate down kept candidate");
        contacts.release(owner);
        contacts.press(owner, {});
        const auto removed = *contacts.soleCandidate();
        contacts.removeDevice(owner.device);
        require(!contacts.resolve(removed) && !contacts.soleCandidate(), "Removed device survived");
        contacts.press(owner, {});
        const auto cleared = *contacts.soleCandidate();
        contacts.clear();
        require(!contacts.resolve(cleared), "Reset kept reservation");
        contacts.press(owner, {});
        require(!contacts.motion(owner, {std::numeric_limits<double>::quiet_NaN(),0})
                && !contacts.soleCandidate(), "Invalid position preserved candidate");
    }
    CarryContacts contacts;
    contacts.press(mouse, {});
    const auto before = *contacts.soleCandidate();
    contacts.press(secondMouse, {});
    require(!contacts.soleCandidate(), "Two mice assigned one owner");
    contacts.release(secondMouse);
    require(!contacts.resolve(before), "Ambiguity removal resurrected old ticket");
    contacts.press(finger, {});
    require(!contacts.soleCandidate(), "Mouse plus touch assigned one owner");
    contacts.cancel(CarryDevice::Touch);
    require(contacts.soleCandidate()->owner() == mouse, "Touch cancel lost held mouse");
    contacts.clear();
    contacts.press(finger, {});
    contacts.press(secondFinger, {});
    require(!contacts.soleCandidate(), "Multitouch assigned one owner");
    contacts.cancel(CarryDevice::Pointer);
    require(!contacts.soleCandidate(), "Pointer cancel lost touch ambiguity");
    contacts.release(secondFinger);
    require(contacts.soleCandidate()->owner() == finger, "Touch ID zero rejected");
    contacts.clear();
    require(!contacts.press({CarryDevice::Pointer,0,272}, {}), "Unknown device accepted");
    require(!contacts.press({CarryDevice::Pointer,1,0}, {}), "No-button accepted");
    require(!contacts.press({CarryDevice::Touch,3,-1}, {}), "Invalid touch ID accepted");
    std::cout << "Carry contact tests passed\n";
}
