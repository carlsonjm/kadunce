/* SPDX-License-Identifier: GPL-2.0-or-later */
// Where a Stack's count stands in Spread: on its front card's top-right
// corner, turned with the card, and always whole on the display.
#include "StackCountBadgeBox.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

using namespace Kadunce;

namespace {
void require(bool value, const char *message)
{
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}
bool near(double a, double b) { return std::abs(a - b) < 0.01; }
const QRectF Display(0, 0, 2560, 1440);
constexpr double Diameter = 26.0;
} // namespace

int main()
{
    const QRectF face(600, 200, 1200, 800);
    // Unturned, it sits a little inside the top-right corner.
    const QRectF flat = stackCountBadgeBox(face, 0.0, Display, Diameter);
    require(near(flat.width(), Diameter) && near(flat.height(), Diameter),
        "The count was not its own size");
    require(flat.contains(face.topRight() + QPointF(-1, 1)),
        "The count did not cover the front card's top-right corner");
    require(flat.center().x() < face.right() && flat.center().y() > face.top(),
        "The count's centre was not inside the card");
    // Turned clockwise about the lower-right corner, the top-right corner moves
    // right, and the count follows it.
    const QRectF turned = stackCountBadgeBox(face, 0.6, Display, Diameter);
    const double shift = face.height() * std::sin(0.6 * M_PI / 180.0);
    require(near(turned.left() - flat.left(), shift),
        "The count did not follow its card's turn");
    // At the display's edge, it stays whole on the display.
    const QRectF edge(1400, 0, 1160, 700);
    const QRectF kept = stackCountBadgeBox(edge, 0.0, Display, Diameter);
    require(Display.contains(kept), "The count left the display");
    // No card, no count.
    require(stackCountBadgeBox(QRectF(), 0.0, Display, Diameter).isEmpty(),
        "A count stood where no card is drawn");
    return 0;
}
