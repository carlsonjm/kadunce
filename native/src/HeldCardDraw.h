#pragma once

#include <map>
#include <string>
#include <utility>

namespace Kadunce {

// What became of a card held in Spread, or of a card of the Stack gathered
// behind it, on a display that painted a frame while it was held.
enum class HeldDraw {
    NotOffered, // the frame ended without the card reaching its paint
    Stopped,    // its paint left before the card was routed
    AsItStands, // drawn as an ordinary window, where its window stands
    Hidden,     // the stage asked does not draw it on this display
    Unseen,     // its pose in the Stack is not shown
    OffDisplay, // drawn as a card, no part of it on this display
    Failed,     // KWin did not draw it
    Drawn       // drawn as a card on this display
};

constexpr const char *heldDrawName(HeldDraw outcome)
{
    switch (outcome) {
    case HeldDraw::NotOffered: return "was not offered to the display's frame";
    case HeldDraw::Stopped: return "left its paint before it was routed";
    case HeldDraw::AsItStands: return "is drawn as a window, where its window stands";
    case HeldDraw::Hidden: return "is hidden by the stage asked";
    case HeldDraw::Unseen: return "is unseen in its Stack";
    case HeldDraw::OffDisplay: return "is drawn as a card, no part of it here";
    case HeldDraw::Failed: return "could not be drawn";
    case HeldDraw::Drawn: return "is drawn as a card here";
    }
    return "";
}

// What became of each held card on each display is said in the journal once,
// and again only when it changes, so a carry reads as a few lines however
// many frames it lasts.
class HeldCardDraw
{
public:
    // A display begins a frame: no held card has reached its paint there yet.
    void beginFrame(const std::string &display) { m_offered[display] = false; }

    // A held card reached its paint on the display. True when what became of
    // it there differs from what was last said.
    bool note(const std::string &card, const std::string &display, HeldDraw outcome)
    {
        m_offered[display] = true;
        m_unoffered.erase(display);
        const auto key = std::make_pair(card, display);
        const auto said = m_said.find(key);
        if (said != m_said.end() && said->second == outcome) return false;
        m_said[key] = outcome;
        return true;
    }

    // The display's frame ended while a card is held. True when none reached
    // its paint there and that has not been said since one last did.
    bool endFrame(const std::string &display)
    {
        const auto offered = m_offered.find(display);
        if (offered == m_offered.end() || offered->second) return false;
        return m_unoffered.insert({display, true}).second;
    }

    // The display under the held card, and what it would do there. True when
    // either differs from what was last said.
    bool over(const std::string &where) { return std::exchange(m_over, where) != where; }

    // A card is picked up: everything is said afresh.
    void clear()
    {
        m_said.clear();
        m_offered.clear();
        m_unoffered.clear();
        m_over.clear();
    }

private:
    std::map<std::pair<std::string, std::string>, HeldDraw> m_said;
    std::map<std::string, bool> m_offered;
    std::map<std::string, bool> m_unoffered;
    std::string m_over;
};

} // namespace Kadunce
