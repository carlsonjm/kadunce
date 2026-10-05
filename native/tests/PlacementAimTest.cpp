#include "PlacementAim.h"
#include <cstdlib>

using namespace Kadunce;

int main()
{
    using namespace Qt::StringLiterals;
    const QList<PlacementOutput> outputs{
        {u"tablet"_s, QRectF(0, 0, 1280, 800), true},
        {u"monitor"_s, QRectF(1280, 0, 1920, 1080), false},
    };
    const auto at = [&](double x, double y) { return placementAim(outputs, QPointF(x, y)); };
    bool ok = true;
    ok &= at(640, 400) == PlacementAim{PlacementAimKind::Card, u"tablet"_s};
    ok &= at(5, 400) == PlacementAim{PlacementAimKind::Left, u"tablet"_s};
    ok &= at(1275, 400) == PlacementAim{PlacementAimKind::Right, u"tablet"_s};
    // The top edge of the card display makes the application Active.
    ok &= at(640, 5) == PlacementAim{PlacementAimKind::Card, u"tablet"_s};
    // The dock's band, its corners included, answers nothing.
    ok &= at(640, 780) == PlacementAim{};
    ok &= at(5, 790) == PlacementAim{};
    ok &= at(2200, 500) == PlacementAim{PlacementAimKind::Display, u"monitor"_s};
    ok &= at(1285, 500) == PlacementAim{PlacementAimKind::Left, u"monitor"_s};
    ok &= at(3195, 500) == PlacementAim{PlacementAimKind::Right, u"monitor"_s};
    ok &= at(2200, 5) == PlacementAim{PlacementAimKind::Top, u"monitor"_s};
    ok &= at(2200, 1070) == PlacementAim{};
    ok &= at(-10, 400) == PlacementAim{};
    ok &= placementAimName(PlacementAimKind::Display) == u"display"_s;
    ok &= placementAimName(PlacementAimKind::None) == u"none"_s;
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
