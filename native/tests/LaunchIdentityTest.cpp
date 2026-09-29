#include "LaunchIdentity.h"
#include <cstdlib>
int main()
{
    using namespace Qt::StringLiterals;
    using namespace LaunchIdentity;
    return matches(u"services_org.example.App.desktop"_s, u"org.example.app"_s)
        && matches(u"applications:zen.desktop"_s, u"zen"_s)
        && !matches(u"zen"_s, u"zen-other"_s)
        && !matches(u""_s, u""_s)
        && !matches(u"org.example.App"_s, u"org.example.Unrelated"_s)
        ? EXIT_SUCCESS : EXIT_FAILURE;
}
