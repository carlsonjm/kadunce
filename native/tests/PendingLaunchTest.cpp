#include "PendingLaunch.h"
#include <cstdlib>

using namespace Kadunce;

int main()
{
    using namespace Qt::StringLiterals;
    bool ok = true;
    const auto launch = PendingLaunch::make({u"applications:org.kde.dolphin.desktop"_s, u" "_s}, u"t1"_s);
    ok &= launch.has_value();
    ok &= launch && launch->identities == QStringList{u"org.kde.dolphin"_s};
    ok &= launch && launch->matches(u"org.kde.Dolphin"_s);
    ok &= launch && !launch->matches(u"org.kde.konsole"_s);
    ok &= !PendingLaunch::make({u"zen"_s}, QString()).has_value();
    ok &= !PendingLaunch::make({u""_s}, u"t2"_s).has_value();
    QStringList many;
    for (int i = 0; i <= PendingLaunch::MaximumIdentities; ++i) many << u"app%1"_s.arg(i);
    ok &= !PendingLaunch::make(many, u"t3"_s).has_value();
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
