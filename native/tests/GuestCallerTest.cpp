/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "GuestCaller.h"

#include <cstdio>
#include <cstdlib>

using namespace Kadunce;

namespace {
void require(bool condition, const char *what)
{
    if (condition) return;
    std::fprintf(stderr, "guest caller: %s\n", what);
    std::exit(1);
}
}

int main()
{
    const QString guest = QStringLiteral(":1.42");
    const QString stranger = QStringLiteral(":1.77");

    require(guestMayBegin(true, guest, guest), "a guest names its own connection");
    require(!guestMayBegin(true, stranger, guest), "a program cannot name another as the guest");
    require(!guestMayBegin(true, guest, QString()), "a guest must name a connection");
    require(guestMayBegin(false, QString(), guest), "Kadunce itself may begin a guest");

    require(guestMayAct(true, guest, guest), "the guest steers itself");
    require(!guestMayAct(true, stranger, guest), "another program cannot steer or end the guest");
    require(!guestMayAct(true, guest, QString()), "with no guest, no caller on the bus may act");
    require(guestMayAct(false, QString(), guest), "Kadunce ends its guest itself");
    require(guestMayAct(false, QString(), QString()), "Kadunce ends a guest that has gone");
    return 0;
}
