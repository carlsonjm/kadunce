// SPDX-License-Identifier: GPL-2.0-or-later
#include "CarryRow.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
using namespace Kadunce;
void check(bool ok, const char *message) {
    if (!ok) { std::cerr << message << '\n'; std::exit(1); }
}
std::vector<double> positions(const std::vector<double> &shifts, double pitch) {
    std::vector<double> result;
    for (int k = 0; k < int(shifts.size()); ++k) result.push_back(carryEntryPosition(k, shifts[std::size_t(k)], pitch));
    return result;
}
int main() {
    // The tablet: 872 between card centres, cards 790 wide, 1463 across.
    const double pitch = 872, card = 790, screen = 1463;

    // Carried, the row stays three across. Pulled down past a short way it
    // takes one set zoom, whatever the pull, no further out than shows all of
    // it; pushed back up past a nearer line it returns.
    check(carryZoomLevel(8, pitch, card, screen) == CarryZoomScale
              && carryZoomLevel(30, pitch, card, screen) == CarryZoomScale,
          "a long row did not zoom to the set view");
    const double two = carryZoomLevel(2, pitch, card, screen);
    check(two > CarryZoomScale && carryRowFits(2, pitch, card, screen, two)
              && !carryRowFits(2, pitch, card, screen, two * 1.01),
          "a short row zoomed out past showing all of it");
    check(carryZoomLevel(0, pitch, card, screen) == 1.0, "a lone card zoomed out");
    check(!carryZoomed(false, CarryZoomEngage - 1) && carryZoomed(false, CarryZoomEngage + 1),
          "a pull down did not zoom out at its line");
    check(carryZoomed(true, CarryZoomEngage - 1) && !carryZoomed(true, CarryZoomRelease - 1),
          "the zoom flickered between its lines, or did not come back");
    check(!carryZoomed(false, -400.0), "a push up zoomed out");
    // It glides by the same ratio each moment, from end to end.
    check(carryZoomAt(1.0, CarryZoomScale, 0.0) == 1.0
              && std::abs(carryZoomAt(1.0, CarryZoomScale, 1.0) - CarryZoomScale) < 1e-12,
          "the zoom glide did not start and end where it should");
    check(std::abs(carryZoomAt(1.0, CarryZoomScale, 0.5) - std::sqrt(CarryZoomScale)) < 1e-12,
          "the zoom glide was not steady");
    // Let go, the row's cards travel so that its scale changes the same way.
    check(carryLandProgress(1.0, 0.3) == 0.3, "a landing from three across changed pace");
    const double half = carryLandProgress(CarryZoomScale, 0.5);
    check(std::abs(CarryZoomScale + (1.0 - CarryZoomScale) * half - std::sqrt(CarryZoomScale)) < 1e-12
              && carryLandProgress(CarryZoomScale, 0.0) == 0.0 && carryLandProgress(CarryZoomScale, 1.0) == 1.0,
          "a landing from the zoom did not grow the row steadily");

    // Entries before the gap stand where they are; the gap parts the rest,
    // a whole place for a card picked up and less once it is carried.
    check(carryEntryPosition(2, 0.0, pitch) == 2 * pitch, "an entry before the gap moved");
    check(carryEntryPosition(2, 1.0, pitch) == 3 * pitch, "an entry after the gap did not move aside");
    check(carryEntryPosition(2, 0.5, pitch) == 2.5 * pitch, "a moving gap jumped");
    check(carryEntryPosition(2, 1.0, pitch, CarryGapWidth) == (2 + CarryGapWidth) * pitch
              && CarryGapWidth < 1.0,
          "a carried card's place stayed a whole card wide");

    // A little either side of the middle holds the row; off it, the row slides
    // the way the card was carried, faster further out, full speed a third of
    // the way out with no need of the edge, and faster over a zoomed row. A
    // small lean only creeps.
    const CarryLean middle = carryLeanStart(screen / 2, 0, screen);
    check(carrySlideSpeed(screen / 2 + 0.9 * CarrySlideBand * screen, middle, 0, screen, 1.0) == 0.0
              && carrySlideSpeed(screen / 2 - 0.9 * CarrySlideBand * screen, middle, 0, screen, 1.0) == 0.0,
          "the middle did not hold the row");
    check(carrySlideSpeed(1000, middle, 0, screen, 1.0) > 0.0 && carrySlideSpeed(400, middle, 0, screen, 1.0) < 0.0,
          "the row did not slide the way the card was carried");
    check(carrySlideSpeed(1100, middle, 0, screen, 1.0) > carrySlideSpeed(950, middle, 0, screen, 1.0),
          "further out was not faster");
    check(carrySlideSpeed(screen / 2 + CarrySlideFull * screen + 1, middle, 0, screen, 1.0) == CarrySlideSpeed
              && CarrySlideFull < 0.35,
          "a third of the way out did not reach full speed");
    check(carrySlideSpeed(1000, middle, 0, screen, 0.5) == 2 * carrySlideSpeed(1000, middle, 0, screen, 1.0),
          "a zoomed row did not cover more");
    check(carrySlideSpeed(screen / 2 + CarrySlideBand * screen + 20, middle, 0, screen, 1.0) < CarrySlideShown,
          "a small lean did more than creep");
    // A card picked up near a side slides nothing where it was picked up, nor
    // when carried inward; carried in and out again, it slides.
    CarryLean side = carryLeanStart(1440, 0, screen);
    check(carrySlideSpeed(1440, side, 0, screen, 1.0) == 0.0, "picking up near a side slid the row");
    side = carryLeanFollow(side, 700, 0, screen);
    check(carrySlideSpeed(700, side, 0, screen, 1.0) == 0.0, "carrying a side card to the middle slid the row");
    check(carrySlideSpeed(1000, side, 0, screen, 1.0) > 0.0, "carried back out, the card did not lean");

    // Picked up or carried, a card stands a little larger than the row it is
    // over, at whatever zoom.
    for (double scale : {CarryZoomScale, two, 1.0})
        check(carryHeldFraction(scale) > scale, "a held card was smaller than the row");

    // Three items besides the held card, with the gap before the second.
    const auto parted = positions({0.0, 1.0, 1.0}, pitch);
    const std::vector<bool> joinable{true, true, false};
    // In the gap: before the second item.
    check(carryAim(1 * pitch, parted, card, joinable, 0, 3) == CarryAim{CarryAim::Kind::Gap, 1},
          "the gap under the card was not the aim");
    // Past the last item: the gap at the end.
    check(carryAim(4 * pitch, parted, card, joinable, 0, 3) == CarryAim{CarryAim::Kind::Gap, 3},
          "the end of the row was not a place");
    // Over the first item's centre: that card.
    check(carryAim(10, parted, card, joinable, 0, 3) == CarryAim{CarryAim::Kind::Card, 0},
          "a card under the held one was not the aim");
    // A Bento group cannot be joined, so over it is still a gap.
    check(carryAim(3 * pitch, parted, card, joinable, 0, 3).kind == CarryAim::Kind::Gap,
          "a card that cannot be joined was joined");
    // Halfway between two cards is never over either.
    check(carryAim(2.5 * pitch, parted, card, joinable, 0, 3).kind == CarryAim::Kind::Gap,
          "the space between two cards joined one");
    // Sliding, the gap follows the card at once, over a card or not.
    const auto closed = positions({0.0, 0.0, 0.0}, pitch);
    check(carryGapAt(10, closed, 0, 3) == 1 && carryGapAt(2.4 * pitch, closed, 0, 3) == 3
              && carryGapAt(-pitch, closed, 0, 3) == 0,
          "the gap did not follow the card");

    // The row slides no further than a little past either end.
    check(carryClampPosition(-5000, 0, 3, pitch) == -0.6 * pitch, "the row slid off its start");
    check(carryClampPosition(9000, 0, 3, pitch) == 3.6 * pitch, "the row slid off its end");

    // Held in its own Stack of three from its second place, a card moves a
    // place for each slow-stroke step: a finger moving left toward the later
    // cards. It holds at either end, and the way back counts from where the
    // finger turns.
    CarryStackPaging paging{1, 500};
    check(carryPageStack(paging, 500 - ScrubStep * 0.9, 3).place == 1, "less than a step moved the card");
    paging = carryPageStack(paging, 500 - ScrubStep, 3);
    check(paging.place == 2, "a step left did not take the next place");
    paging = carryPageStack(paging, 500 - 5 * ScrubStep, 3);
    check(paging.place == 2, "the card went past the Stack's last place");
    paging = carryPageStack(paging, 500 - 4 * ScrubStep, 3);
    check(paging.place == 1, "turning back at the end did not count from the turn");
    paging = carryPageStack(paging, 500 + 3 * ScrubStep, 3);
    check(paging.place == 0, "the card went before the Stack's first place");
    // Its Stack stands in the order it would have, the held card at its place.
    check(carryStackOrder({4, 7, 9}, 4, 2) == std::vector<int>({7, 9, 4}), "the held card was not put last");
    check(carryStackOrder({4, 7, 9}, 9, 0) == std::vector<int>({9, 4, 7}), "the held card was not put first");
    check(carryStackOrder({4, 7, 9}, 7, 1) == std::vector<int>({4, 7, 9}), "its own place changed the order");
    // Held, a Stack is counted front to back from the card in front, each card
    // behind the one before it round the fan.
    check(carryStackDeck({4, 7, 9}, 0) == std::vector<int>({4, 9, 7})
              && carryStackDeck({4, 7, 9}, 1) == std::vector<int>({7, 4, 9}),
          "a Stack was not counted front to back as its fan shows it");
    // Let go at any place, only the held card moves, and the Stack then shows
    // whichever card is in front, the held one only when it is.
    for (int count = 2; count <= 5; ++count) {
        std::vector<int> members;
        for (int k = 0; k < count; ++k) members.push_back(10 + k);
        for (int face = 0; face < count; ++face) {
            const auto deck = carryStackDeck(members, face);
            for (int held : members) for (int place = 0; place < count; ++place) {
                std::vector<int> others = members;
                others.erase(std::find(others.begin(), others.end(), held));
                const auto landing = carryStackLanding(deck, held, place, others);
                std::vector<int> after = others;
                after.insert(after.begin() + landing.insertion, held);
                const int shown = int(std::find(after.begin(), after.end(), landing.face) - after.begin());
                check(carryStackDeck(after, shown) == carryStackOrder(deck, held, place),
                      "a Stack let go did not stand in the order it was shown");
                check((landing.face == held) == (place == 0), "the held card stayed in front from behind");
            }
        }
    }
    check(carryStackLanding({1, 2}, 1, 1, {2}).face == 2, "a card sent behind its one partner stayed in front");
    // It follows the finger only a little way, never out of its Stack.
    check(std::abs(carryStackLeash(5000)) <= CarryStackLeash && carryStackLeash(-5000) < 0
              && carryStackLeash(4) > 3.5,
          "a card held in its Stack roamed, or did not answer the finger at all");
    std::cout << "A held card's row parts, slides and zooms as the hand asks, and a Stack's card reorders front to back\n";
}
