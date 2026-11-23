#include "DisplayHandoffPolicy.h"
#include "HeldCardDraw.h"

#include <cassert>
#include <cstring>

using namespace Kadunce;

int main()
{
    // Each display paints with its own stage named, and a stage draws only
    // its own cards. A card held in one display's Spread is no card of the
    // display beside it: asked of that display's stage, which is painting its
    // own display, does not own the window, has no slot for it and holds no
    // grab, it is hidden. So it vanished the moment it crossed.
    assert(cardPaintRoute(true, false, false, false, false) == CardPaintRoute::Hidden);
    // Asked of the stage carrying it, it is drawn as a card on either display.
    for (bool paintingItsOwnDisplay : {false, true})
        assert(cardPaintRoute(paintingItsOwnDisplay, true, true, false, true) == CardPaintRoute::Card);

    HeldCardDraw said;
    // What became of a card on a display is said once, then on each change.
    assert(said.note("ghostty", "DP-1", HeldDraw::OffDisplay));
    assert(!said.note("ghostty", "DP-1", HeldDraw::OffDisplay));
    assert(said.note("ghostty", "DP-1", HeldDraw::Drawn));
    assert(!said.note("ghostty", "DP-1", HeldDraw::Drawn));
    // Each card and each display is said on its own.
    assert(said.note("ghostty", "eDP-1", HeldDraw::Drawn));
    assert(said.note("spotify", "DP-1", HeldDraw::Drawn));
    assert(said.note("ghostty", "DP-1", HeldDraw::Hidden));

    // A frame no held card reached is said once, until one reaches it again.
    said.beginFrame("DP-1");
    assert(said.endFrame("DP-1"));
    said.beginFrame("DP-1");
    assert(!said.endFrame("DP-1"));
    said.beginFrame("DP-1");
    assert(!said.note("ghostty", "DP-1", HeldDraw::Hidden));
    assert(!said.endFrame("DP-1"));
    said.beginFrame("DP-1");
    assert(said.endFrame("DP-1"));
    // A display that has begun no frame has nothing to say.
    assert(!said.endFrame("HDMI-1"));

    // The display under the card and its aim there are said on each change.
    assert(said.over("DP-1: a place in its layout"));
    assert(!said.over("DP-1: a place in its layout"));
    assert(said.over("eDP-1: its row"));

    // A new carry says everything afresh.
    said.clear();
    assert(said.note("ghostty", "DP-1", HeldDraw::Hidden));
    assert(said.over("eDP-1: its row"));

    for (HeldDraw outcome : {HeldDraw::NotOffered, HeldDraw::Stopped, HeldDraw::AsItStands, HeldDraw::Hidden,
                             HeldDraw::Unseen, HeldDraw::OffDisplay, HeldDraw::Failed, HeldDraw::Drawn})
        assert(std::strlen(heldDrawName(outcome)) > 0);
    return 0;
}
