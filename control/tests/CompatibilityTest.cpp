#include "Compatibility.h"
#include <cstdlib>

int main()
{
    using namespace Compatibility;
    using namespace Qt::StringLiterals;
    if (pluginVersion(u"org.kde.kwin.EffectPluginFactory6.7.4"_s) != u"6.7.4"_s
        || !pluginVersion(u"unrelated6.7.4"_s).isEmpty()
        || kwinVersion(u"kwin 6.7.5\n"_s) != u"6.7.5"_s
        || !kwinVersion(u"could not start"_s).isEmpty()
        || !mismatch(u"6.7.4"_s, u"6.7.5"_s)
        || mismatch(u"6.7.5"_s, u"6.7.5"_s)
        || mismatch(u""_s, u"6.7.5"_s) || mismatch(u"6.7.5"_s, u""_s)) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
