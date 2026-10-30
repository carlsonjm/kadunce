/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "CallerTrust.h"

#include <cstdio>
#include <cstdlib>

using namespace Kadunce;

namespace {
void require(bool condition, const char *what)
{
    if (condition) return;
    std::fprintf(stderr, "caller trust: %s\n", what);
    std::exit(1);
}
}

int main()
{
    const QString home = QStringLiteral("/home/test");

    require(programMayDrive(QStringLiteral("/usr/bin/plasmashell"), home), "the shell drives the dock's calls");
    require(programMayDrive(QStringLiteral("/usr/lib/kscreenlocker_greet"), home), "the lock screen raises the keys");
    require(programMayDrive(QStringLiteral("/usr/libexec/kscreenlocker_greet"), home), "wherever the system keeps it");
    require(programMayDrive(QStringLiteral("/usr/bin/tettegouche"), home), "Search brings a window forward");
    require(programMayDrive(QStringLiteral("/home/test/.local/bin/shuffle-keyboard"), home),
        "the keys installed for the person");
    require(programMayDrive(QStringLiteral("/usr/bin/split-rock-core"), home),
        "Shuffle Core brings the apps back");
    require(programMayDrive(QStringLiteral("/usr/bin/plasmashell (deleted)"), home),
        "a shell updated on disk while it runs is still the shell");

    require(!programMayDrive(QStringLiteral("/usr/bin/qdbus6"), home), "a bus tool is a stranger");
    require(!programMayDrive(QStringLiteral("/usr/bin/python3"), home), "a script is a stranger");
    require(!programMayDrive(QStringLiteral("/tmp/plasmashell"), home), "a copy elsewhere is a stranger");
    require(!programMayDrive(QStringLiteral("/home/test/plasmashell"), home), "a copy in the home folder is a stranger");
    require(!programMayDrive(QStringLiteral("/home/test/.local/bin/tools/tettegouche"), home),
        "only the programs folder itself");
    require(!programMayDrive(QStringLiteral("/home/user/.local/bin/tettegouche"), home), "only this person's folder");
    require(!programMayDrive(QStringLiteral("/home/test/.local/bin/tettegouche"), QString()),
        "with no home known, no home folder counts");
    require(!programMayDrive(QStringLiteral("/usr/../tmp/plasmashell"), home), "no way out of the system folders");
    require(!programMayDrive(QStringLiteral("plasmashell"), home), "a bare name is no program");
    require(!programMayDrive(QString(), home), "an unknown program is a stranger");
    return 0;
}
