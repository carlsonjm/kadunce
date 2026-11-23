// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QList>
#include <QPointF>

#include <algorithm>
#include <cmath>
#include <limits>

namespace Kadunce
{

// Table (TABLE.md) is one stroke from the top edge. How far
// the finger has pulled picks the level, the workspaces or one workspace's
// cards, and where it is across picks the item on that level. This holds the
// stroke and says what it asks for; the effect lays the rows out, draws them
// and carries the asks out. Workspaces and cards are indexes into what the
// effect handed over when the stroke began.
struct TableConfig {
    // Where the cards level begins, as a fraction of the display's height.
    double depth = 0.30;
    // Past this line under the cards, the card under the finger lifts to be
    // moved: choosing a card and moving it are two depths,
    // never a rest, since a finger pausing to look at a card is resting.
    double lift = 0.45;
    // Back above this after a scrub, lifting cancels.
    double cancel = 0.05;
    // A finger that moves this far before lifting is not a tap, and a menu
    // bar's card pressed and dragged this far lifts.
    double tapSlop = 10.0;
    // A stroke lifted this soon is a flick however far it went, and one
    // lifted before it scrubbed is one too: either leaves the tabs open.
    int flickMs = 260;
    // A menu bar's tab held this long without moving is renamed. Only a menu
    // bar's: a finger scrubbing rests on tabs to look at them, and must never
    // rename one by resting.
    int holdMs = 500;
};

// The workspaces, one workspace's cards, or the cards of one of its Stacks,
// which hang under the Stack once it is rested on.
enum class TableLevel { Tabs, Cards, Members };

struct TableAction {
    enum class Kind {
        None,
        // The tabs stay open as a menu bar; nothing else changes.
        StayOpen,
        // Table closes and nothing changes.
        Close,
        // Enter `workspace` as it was left.
        Enter,
        // Make card `card` of `workspace` Active there; with a `member`,
        // that card of its Stack.
        Activate,
        // Move card `card` of `workspace` to workspace `destination`; with a
        // `member`, that card alone, out of its Stack.
        Move,
        // Make a workspace for card `card` of `workspace` and move it there.
        Create,
        // Stack card `card` of `workspace` (or its `member` alone) onto card
        // `target` of workspace `destination`: the one carried in front, every
        // card of both kept. Table stays open.
        Stack,
        // Card `member` of the Stack `card` of `workspace` leaves the Stack as
        // a card of its own, just after it. Table stays open.
        TakeOut,
        // Make an empty workspace; Table stays open on its tab.
        New,
        // Rename `workspace`: its tab held in a menu bar, right-clicked, or
        // F2.
        Rename,
        // Make a workspace named after saved Layout `card`, enter it and
        // open the Layout there. Table closes.
        OpenLayout,
    };
    Kind kind = Kind::None;
    int workspace = -1;
    int card = -1;
    int destination = -1;
    int member = -1;
    int target = -1;
};

// What a menu bar's tap landed on: a tab, the + tab, a card of the tray's
// workspace, a card hanging under +, or nothing of Table's.
struct TableHit {
    int tab = -1;
    bool plus = false;
    int card = -1;
    int plusCard = -1;
    // A card of the Stack whose cards hang.
    int member = -1;
    bool insideRows = false;
};

class TableGesture
{
public:
    explicit TableGesture(TableConfig config = {})
        : m_config(config)
    {
    }

    [[nodiscard]] const TableConfig &config() const { return m_config; }

    // Where the levels change, as fractions of the display's height: the
    // cards level begins just under the tabs, a card lifts just under the
    // cards, and above the tabs a lift cancels. The effect sets them from
    // where its rows stand.
    void setThresholds(double depth, double cancel, double lift,
                       double membersLift = std::numeric_limits<double>::quiet_NaN())
    {
        m_config.depth = depth;
        m_config.cancel = cancel;
        m_config.lift = lift;
        m_membersLift = membersLift;
    }

    // Opens on the current workspace. A sticky Table is the menu bar a flick
    // leaves; otherwise the finger that opened it is scrubbing.
    void open(int current, qint64 now, bool sticky = false)
    {
        *this = TableGesture(m_config);
        m_open = true;
        m_current = current;
        m_hovered = current;
        m_sticky = sticky;
        // A pull opens Table under a finger that is still down.
        m_down = !sticky;
        m_openedAt = now;
    }

    void close() { *this = TableGesture(m_config); }

    // Where the rows stand across the display, as the effect lays them out:
    // each tab's centre, the + tab's, and each workspace's cards' centres.
    // Each Stack's cards' centres, by workspace and card, empty for a lone
    // card: what hangs under a Stack once it is rested on.
    void setLayout(QList<double> tabs, double plus, QList<QList<double>> cards,
                   QList<QList<QList<double>>> members = {})
    {
        m_tabs = std::move(tabs);
        m_plus = plus;
        m_cards = std::move(cards);
        m_members = std::move(members);
        if (m_membersOf >= 0 && !hasMembers(m_locked, m_membersOf)) fold();
    }

    // The cards that hang under +, their centres across the display: the
    // empty workspace first, then each saved Layout. None, and + makes a
    // workspace at once, as it always has.
    void setPlusCards(QList<double> centres)
    {
        m_plusCards = std::move(centres);
        if (m_plusCard >= m_plusCards.size()) m_plusCard = -1;
    }

    // The finger is down at position. On a sticky Table's card, the effect
    // found under it, the press previews that card, and dragged it lifts, as
    // anything is dragged on a desktop. On a sticky Table's tab, held still
    // it renames the tab (hold).
    void press(QPointF position, int card, int tab = -1, qint64 now = 0, int plusCard = -1, int member = -1)
    {
        m_down = true;
        m_pressAt = position;
        m_last = position;
        m_held = false;
        // A finger takes over from the keys: the card they carry goes back.
        if (m_keyCarry) putBack();
        m_pressTab = m_sticky && !m_carrying ? tab : -1;
        m_pressedAt = now;
        const int tray = trayWorkspace();
        m_pressMember = -1;
        if (m_sticky && member >= 0 && m_membersOf >= 0 && tray >= 0) {
            // A hanging card of a Stack: pressed it shows, dragged it lifts
            // out of its Stack.
            m_onPlus = false;
            m_level = TableLevel::Members;
            m_card = m_membersOf;
            m_member = member;
            m_pressMember = member;
        } else if (m_sticky && card >= 0 && tray >= 0) {
            m_onPlus = false;
            m_level = TableLevel::Cards;
            m_locked = tray;
            m_hovered = tray;
            if (card != m_membersOf) fold();
            m_card = card;
            m_pressCard = card;
        }
        // A card under + is touched to see it; it is never carried.
        if (m_sticky && plusCard >= 0 && plusTray()) m_plusCard = plusCard;
    }

    void move(QPointF position, double y)
    {
        m_last = position;
        m_lastY = y;
        if (!m_open) return;
        if (m_pressTab >= 0 && distance(position, m_pressAt) > m_config.tapSlop) m_pressTab = -1;
        if (m_held) return;
        if (m_sticky && !m_carrying) {
            if ((m_pressCard < 0 && m_pressMember < 0) || distance(position, m_pressAt) <= m_config.tapSlop) return;
            if (m_pressMember >= 0) liftMember(m_pressMember);
            else liftCard(m_pressCard);
            m_pressCard = -1;
            m_pressMember = -1;
        }
        scrub(position, y);
    }

    // The finger lifted at position. For a sticky Table's tap, the effect
    // names what it landed on.
    TableAction release(QPointF position, qint64 now, TableHit hit = {})
    {
        m_down = false;
        m_pressTab = -1;
        if (!m_open) return {};
        m_pressCard = -1;
        m_pressMember = -1;
        // The finger that held a tab to rename it lets go of nothing else.
        if (m_held) {
            m_held = false;
            return {TableAction::Kind::StayOpen};
        }
        // Measured on the tablet on 26 September: a flick carried past the
        // cards line chose a card, so Table closed on one. A quick stroke
        // chooses and moves nothing, however deep it went.
        const bool flick = !m_sticky && !m_cancelling && now - m_openedAt < m_config.flickMs;
        if (m_carrying && !flick) return dropCarry();
        if (!m_sticky) {
            if (m_cancelling) return closeWith({TableAction::Kind::Close});
            if (flick || (m_level == TableLevel::Tabs && !m_scrubbed)) {
                interrupt();
                m_level = TableLevel::Tabs;
                m_card = -1;
                m_hovered = m_current;
                m_onPlus = false;
                return {TableAction::Kind::StayOpen};
            }
            if (m_level == TableLevel::Members && m_member >= 0)
                return closeWith(activateMember());
            if (m_level == TableLevel::Cards && m_card >= 0)
                return closeWith({TableAction::Kind::Activate, m_locked, m_card});
            if (m_level != TableLevel::Tabs) return closeWith({TableAction::Kind::Close});
            if (plusTray() && m_plusCard >= 0) return choosePlusCard(m_plusCard);
            // Lifting on + hangs its cards, or with none makes a workspace,
            // as lifting on a tab enters one.
            if (m_onPlus) return m_plusCards.isEmpty() ? makeNew() : openPlusTray();
            return closeWith({TableAction::Kind::Enter, m_hovered});
        }
        // A menu bar: a tap previews, a second tap on the same tab enters.
        // A stroke pushed back up to the edge closes it.
        if (distance(position, m_pressAt) > m_config.tapSlop) {
            if (m_lastY < m_config.cancel) return closeWith({TableAction::Kind::Close});
            return {TableAction::Kind::StayOpen};
        }
        if (hit.member >= 0 && m_membersOf >= 0 && trayWorkspace() >= 0) {
            m_member = hit.member;
            return closeWith(activateMember());
        }
        if (hit.card >= 0 && trayWorkspace() >= 0)
            return closeWith({TableAction::Kind::Activate, trayWorkspace(), hit.card});
        if (hit.plusCard >= 0 && plusTray()) return choosePlusCard(hit.plusCard);
        // + hangs the empty workspace and the saved Layouts; with no Layouts
        // it has nothing to show, so one tap makes the workspace.
        if (hit.plus) return m_plusCards.isEmpty() ? makeNew() : openPlusTray();
        if (hit.tab >= 0) {
            if (hit.tab == m_hovered && m_level == TableLevel::Tabs && !m_onPlus)
                return closeWith({TableAction::Kind::Enter, hit.tab});
            m_onPlus = false;
            m_plusCard = -1;
            m_hovered = hit.tab;
            m_level = TableLevel::Tabs;
            m_card = -1;
            return {TableAction::Kind::StayOpen};
        }
        if (hit.insideRows) return {TableAction::Kind::StayOpen};
        // A tap on the preview goes to what it shows.
        if (m_level == TableLevel::Members && m_member >= 0 && m_locked >= 0) return closeWith(activateMember());
        if (m_level == TableLevel::Cards && m_card >= 0 && m_locked >= 0)
            return closeWith({TableAction::Kind::Activate, m_locked, m_card});
        return closeWith({TableAction::Kind::Enter, shownWorkspace()});
    }

    // The finger still on the tab it pressed in a menu bar, without having
    // moved, once it has been there holdMs: that tab is renamed. Asked by the
    // effect's timer; a press asks at most once.
    TableAction hold(qint64 now)
    {
        if (!m_open || !m_sticky || !m_down || m_carrying || m_held || m_pressTab < 0
            || now - m_pressedAt < m_config.holdMs) return {};
        m_held = true;
        return renameTab(m_pressTab);
    }

    // How long until a press still waiting to become a hold does, or -1 when
    // no press is waiting. A timer can wake a little early.
    [[nodiscard]] qint64 holdLeft(qint64 now) const
    {
        if (!m_open || !m_sticky || !m_down || m_carrying || m_held || m_pressTab < 0) return -1;
        return std::max<qint64>(1, m_config.holdMs - (now - m_pressedAt));
    }

    // A right-click on a tab renames it, as holding one does.
    TableAction contextOn(int tab)
    {
        if (!m_open || m_carrying || m_down || tab < 0 || tab >= m_tabs.size()) return {};
        m_sticky = true;
        return renameTab(tab);
    }

    // A pointer resting on a menu bar's rows: over a tab it previews that
    // workspace, over a hanging card that card, and over + it shows where the
    // person is.
    void hover(int tab, int card, bool plus = false, int plusCard = -1, int member = -1)
    {
        if (!m_open || !m_sticky || m_down || m_carrying) return;
        if (member >= 0 && m_membersOf >= 0 && trayWorkspace() >= 0) {
            m_level = TableLevel::Members;
            m_card = m_membersOf;
            m_member = member;
        } else if (plusCard >= 0 && plusTray()) {
            m_plusCard = plusCard;
        } else if (plus && hasPlus()) {
            if (!m_onPlus) m_plusCard = -1;
            m_onPlus = true;
            m_level = TableLevel::Tabs;
            m_card = -1;
            fold();
        } else if (tab >= 0) {
            m_onPlus = false;
            m_plusCard = -1;
            m_hovered = tab;
            m_level = TableLevel::Tabs;
            m_card = -1;
            fold();
        } else if (card >= 0 && trayWorkspace() >= 0) {
            m_locked = trayWorkspace();
            m_hovered = m_locked;
            m_level = TableLevel::Cards;
            if (card != m_membersOf) fold();
            m_card = card;
            m_member = -1;
        }
    }

    // Steps across the level in view, one tab or card at a time, by the
    // wheel or an arrow key, + coming after the last tab; Table becomes a
    // menu bar.
    void step(int steps)
    {
        if (!m_open || m_carrying || steps == 0) return;
        m_sticky = true;
        if (plusTray() && m_plusCard >= 0) {
            m_plusCard = std::clamp(m_plusCard + steps, 0, int(m_plusCards.size()) - 1);
        } else if (m_level == TableLevel::Members) {
            const int count = int(memberCentres().size());
            if (count > 0) m_member = std::clamp(m_member < 0 ? 0 : m_member + steps, 0, count - 1);
        } else if (m_level == TableLevel::Cards) {
            const int count = m_locked >= 0 && m_locked < m_cards.size() ? int(m_cards[m_locked].size()) : 0;
            if (count > 0) m_card = std::clamp(m_card < 0 ? (steps > 0 ? 0 : count - 1) : m_card + steps, 0, count - 1);
            if (m_card != m_membersOf) fold();
        } else if (!m_tabs.isEmpty()) {
            const int last = int(m_tabs.size()) - (hasPlus() ? 0 : 1);
            const int at = std::clamp((m_onPlus ? int(m_tabs.size()) : m_hovered) + steps, 0, last);
            m_onPlus = at == int(m_tabs.size());
            m_plusCard = -1;
            if (!m_onPlus) m_hovered = at;
        }
    }

    // CarryLeft and CarryRight are Shift's arrows, and Drop is Shift let go.
    // TakeOut is Meta+Shift+Down, which takes a card out of its Stack everywhere.
    enum class Key { Left, Right, Up, Down, Enter, Escape, Rename, CarryLeft, CarryRight, Drop, TakeOut };

    // The keyboard: left and right choose, down and up change level, Enter
    // goes where the displays show, F2 renames the workspace they show,
    // Escape cancels. Shift's arrows on a chosen card lift it and carry it
    // across the tabs and +, and letting go of Shift sets it down there, as
    // a finger letting go does.
    TableAction key(Key key)
    {
        if (!m_open) return {};
        if (m_carrying && !m_keyCarry) {
            if (key == Key::Escape) refuseCarry();
            return {TableAction::Kind::StayOpen};
        }
        if (m_carrying) {
            switch (key) {
            case Key::CarryLeft:
                carryStep(-1);
                break;
            case Key::CarryRight:
                carryStep(1);
                break;
            case Key::Drop:
                return dropKeyCarry();
            case Key::Escape:
                putBack();
                break;
            default:
                break;
            }
            return {TableAction::Kind::StayOpen};
        }
        m_sticky = true;
        switch (key) {
        case Key::CarryLeft:
        case Key::CarryRight: {
            const int steps = key == Key::CarryLeft ? -1 : 1;
            if (m_level == TableLevel::Members && m_member >= 0 && m_locked >= 0) {
                liftMember(m_member);
                m_keyCarry = true;
                carryStep(steps);
            } else if (m_level == TableLevel::Cards && m_card >= 0 && m_locked >= 0) {
                liftCard(m_card);
                m_keyCarry = true;
                carryStep(steps);
            } else {
                step(steps);
            }
            break;
        }
        case Key::Drop:
            break;
        case Key::Left:
            step(-1);
            break;
        case Key::Right:
            step(1);
            break;
        case Key::Down:
            if (plusTray() && m_plusCard < 0) m_plusCard = 0;
            // Down on a Stack hangs its cards, the front one chosen.
            if (m_level == TableLevel::Cards && m_card >= 0 && hasMembers(m_locked, m_card)) {
                m_membersOf = m_card;
                m_level = TableLevel::Members;
                m_member = 0;
                break;
            }
            if (m_level == TableLevel::Tabs && !m_onPlus && m_hovered >= 0 && m_hovered < m_cards.size()
                && !m_cards[m_hovered].isEmpty()) {
                m_level = TableLevel::Cards;
                m_locked = m_hovered;
                m_card = 0;
            }
            break;
        case Key::Up:
            if (plusTray()) m_plusCard = -1;
            if (m_level == TableLevel::Members) {
                m_level = TableLevel::Cards;
                m_member = -1;
                break;
            }
            if (m_level == TableLevel::Cards) {
                m_level = TableLevel::Tabs;
                m_hovered = m_locked;
                m_card = -1;
            }
            break;
        case Key::TakeOut:
            if (m_level == TableLevel::Members && m_member >= 0 && m_locked >= 0) return takeOut();
            break;
        case Key::Enter:
            if (m_level == TableLevel::Members && m_member >= 0) return closeWith(activateMember());
            if (m_level == TableLevel::Cards && m_card >= 0)
                return closeWith({TableAction::Kind::Activate, m_locked, m_card});
            if (plusTray() && m_plusCard >= 0) return choosePlusCard(m_plusCard);
            // On + with cards hanging, Enter goes down to the first of them.
            if (plusTray()) {
                m_plusCard = 0;
                break;
            }
            if (m_onPlus) return makeNew();
            return closeWith({TableAction::Kind::Enter, m_hovered});
        case Key::Escape:
            return closeWith({TableAction::Kind::Close});
        case Key::Rename:
            if (m_level != TableLevel::Tabs && m_locked >= 0) return renameTab(m_locked);
            if (!m_onPlus && m_hovered >= 0) return renameTab(m_hovered);
            break;
        }
        return {TableAction::Kind::StayOpen};
    }

    // The card that lifted cannot be carried; the finger keeps it chosen,
    // and it does not lift again until the finger chooses another.
    void refuseCarry()
    {
        m_refused = m_carryCard;
        m_keyCarry = false;
        m_carrying = false;
        m_carryCard = -1;
        m_carryMember = -1;
        dropAim();
        m_carryFrom = -1;
        m_over = Over::Nothing;
        m_overWorkspace = -1;
    }

    // The finger's stroke was taken away mid-way. Table stays as the menu bar
    // the next tap works, with nothing lifted and nothing cancelled.
    void interrupt()
    {
        if (!m_open) return;
        m_down = false;
        m_sticky = true;
        if (m_keyCarry) putBack();
        m_carrying = false;
        m_carryCard = -1;
        m_carryMember = -1;
        m_carryFrom = -1;
        m_over = Over::Nothing;
        m_overWorkspace = -1;
        m_pressCard = -1;
        m_pressMember = -1;
        dropAim();
        m_cancelling = false;
    }

    // After a card moved or a workspace was made, the rows describe the new
    // arrangement; Table stays open showing where the card went.
    void showAfterMove(int destination, int card)
    {
        m_sticky = true;
        fold();
        m_onPlus = false;
        m_level = TableLevel::Cards;
        m_locked = destination;
        m_hovered = destination;
        m_card = card;
    }

    // The workspace + just made: its tab is in view, nothing of it chosen.
    void showNewTab(int tab)
    {
        m_sticky = true;
        fold();
        m_onPlus = false;
        m_level = TableLevel::Tabs;
        m_hovered = tab;
        m_card = -1;
    }

    [[nodiscard]] bool isOpen() const { return m_open; }
    [[nodiscard]] bool sticky() const { return m_sticky; }
    [[nodiscard]] bool down() const { return m_down; }
    [[nodiscard]] TableLevel level() const { return m_level; }
    [[nodiscard]] int current() const { return m_current; }
    [[nodiscard]] int hovered() const { return m_hovered; }
    [[nodiscard]] int locked() const { return m_locked; }
    [[nodiscard]] int card() const { return m_card; }
    [[nodiscard]] bool cancelling() const { return m_cancelling; }
    [[nodiscard]] bool scrubbing() const { return m_open && !m_sticky; }
    [[nodiscard]] bool carrying() const { return m_carrying; }
    // The card in hand is carried by the keys rather than a finger.
    [[nodiscard]] bool keyCarrying() const { return m_carrying && m_keyCarry; }
    [[nodiscard]] int carryCard() const { return m_carryCard; }
    [[nodiscard]] int carryFrom() const { return m_carryFrom; }
    // The card of a Stack carried out of it, or -1 when a whole card is.
    [[nodiscard]] int carryMember() const { return m_carrying ? m_carryMember : -1; }

    // The Stack whose cards hang under it, in the workspace whose cards
    // hang, or -1; and the one of its cards the finger, pointer or keys are
    // on.
    [[nodiscard]] int membersOf() const { return m_open ? m_membersOf : -1; }
    [[nodiscard]] int member() const { return m_open && m_level == TableLevel::Members ? m_member : -1; }
    // Whether card `card` of `workspace` is a Stack, whose cards can hang.
    [[nodiscard]] bool hasMembers(int workspace, int card) const
    {
        return workspace >= 0 && workspace < m_members.size() && card >= 0 && card < m_members[workspace].size()
            && m_members[workspace][card].size() > 1;
    }
    // A finger or pointer resting on a Stack whose cards do not hang yet:
    // the effect waits a moment, then calls hangMembers.
    [[nodiscard]] bool wantsRest() const
    {
        return m_open && !m_carrying && m_level == TableLevel::Cards && m_card >= 0 && m_card != m_membersOf
            && hasMembers(m_locked, m_card);
    }
    // The Stack rested on hangs its cards. True when they came to hang.
    bool hangMembers()
    {
        if (!wantsRest()) return false;
        m_membersOf = m_card;
        m_member = -1;
        return true;
    }

    // The card a carried one is over, in the row it hangs over, as the effect
    // measures it; held there long enough, the effect arms it, and let go the
    // carried card stacks onto it. A new aim disarms. True when it changed.
    bool aimAt(int card)
    {
        if (!m_carrying || m_keyCarry || m_level != TableLevel::Cards) card = -1;
        if (card == m_aim) return false;
        m_aim = card;
        m_aimArmed = false;
        return true;
    }
    void armAim() { if (m_carrying && m_aim >= 0) m_aimArmed = true; }
    [[nodiscard]] int aim() const { return m_carrying ? m_aim : -1; }
    [[nodiscard]] bool aimArmed() const { return m_carrying && m_aim >= 0 && m_aimArmed; }
    // The finger, pointer or keys are on + rather than a tab.
    [[nodiscard]] bool onPlus() const { return m_open && m_onPlus && !m_carrying; }
    // The cards under + hang, as a tab's do while it is in view.
    [[nodiscard]] bool plusTray() const { return onPlus() && !m_plusCards.isEmpty(); }
    // The card under + the finger, pointer or keys are on, or -1.
    [[nodiscard]] int plusCard() const { return plusTray() ? m_plusCard : -1; }
    [[nodiscard]] QPointF finger() const { return m_last; }

    enum class Over { Nothing, Workspace, Plus };
    [[nodiscard]] Over over() const { return m_over; }
    [[nodiscard]] int overWorkspace() const { return m_overWorkspace; }

    // The workspace the displays show: the one the finger is on, or where the
    // person already is while a lift would cancel.
    [[nodiscard]] int shownWorkspace() const
    {
        if (!m_open || m_cancelling || (m_onPlus && !m_carrying)) return m_current;
        if (m_carrying) return m_hovered;
        return m_level == TableLevel::Tabs ? m_hovered : m_locked;
    }

    // The card the displays show in front, when the finger is on one.
    [[nodiscard]] int shownCard() const
    {
        if (!m_open || m_cancelling || m_level == TableLevel::Tabs) return -1;
        return m_card;
    }
    // The card of a hanging Stack the displays show in its place, or -1.
    [[nodiscard]] int shownMember() const
    {
        if (!m_open || m_cancelling || m_level != TableLevel::Members) return -1;
        return m_member;
    }

    // Whose cards hang below the tabs: the locked workspace's on the cards,
    // and otherwise the one under the finger or pointer, so a scrub shows the
    // cards before it reaches them, as a menu bar does.
    [[nodiscard]] int trayWorkspace() const
    {
        if (!m_open || (m_onPlus && !m_carrying && m_level == TableLevel::Tabs)) return -1;
        // A card in hand hangs the row it is over: the tab it last passed.
        if (m_carrying) return m_hovered;
        return m_level == TableLevel::Tabs ? m_hovered : m_locked;
    }

private:
    [[nodiscard]] bool hasPlus() const { return !std::isnan(m_plus); }

    // Whether + is nearer x than the nearest tab.
    [[nodiscard]] bool plusNearer(int tab, double x) const
    {
        return hasPlus() && (tab < 0 || std::abs(m_plus - x) < std::abs(m_tabs[tab] - x));
    }

    static double distance(QPointF a, QPointF b)
    {
        return std::hypot(a.x() - b.x(), a.y() - b.y());
    }

    // The nearest of the given centres to x: the whole column under a tab
    // belongs to it, so the finger never has to land on the tab itself.
    static int nearest(const QList<double> &centres, double x)
    {
        int best = -1;
        double bestDistance = std::numeric_limits<double>::infinity();
        for (int i = 0; i < centres.size(); ++i) {
            const double d = std::abs(centres[i] - x);
            if (d < bestDistance) {
                bestDistance = d;
                best = i;
            }
        }
        return best;
    }

    // The tab being renamed is the one in view, its cards hanging under it.
    TableAction renameTab(int tab)
    {
        m_onPlus = false;
        m_hovered = tab;
        m_level = TableLevel::Tabs;
        m_card = -1;
        return {TableAction::Kind::Rename, tab};
    }

    // The card lifts from the workspace whose cards hang, to be carried.
    void liftCard(int card)
    {
        m_carrying = true;
        m_carryCard = card;
        m_carryMember = -1;
        m_carryFrom = m_locked;
        m_hovered = m_locked;
        m_over = Over::Workspace;
        m_overWorkspace = m_locked;
        dropAim();
    }

    // A hanging card of a Stack lifts out of it, to be carried.
    void liftMember(int member)
    {
        liftCard(m_membersOf);
        m_carryMember = member;
    }

    void dropAim()
    {
        m_aim = -1;
        m_aimArmed = false;
    }

    // The Stack's cards no longer hang.
    void fold()
    {
        m_membersOf = -1;
        m_member = -1;
        if (m_level == TableLevel::Members) m_level = TableLevel::Cards;
    }

    [[nodiscard]] QList<double> memberCentres() const
    {
        return hasMembers(m_locked, m_membersOf) ? m_members[m_locked][m_membersOf] : QList<double>{};
    }

    // Lifting on a hanging card opens it, as its Stack's card in front.
    [[nodiscard]] TableAction activateMember() const
    {
        return {TableAction::Kind::Activate, m_locked, m_membersOf, -1, m_member};
    }

    // The chosen hanging card leaves its Stack; Table stays open on the
    // workspace, the Stack's cards folded.
    TableAction takeOut()
    {
        const TableAction action{TableAction::Kind::TakeOut, m_locked, m_membersOf, -1, m_member};
        m_sticky = true;
        fold();
        m_level = TableLevel::Cards;
        m_card = action.card;
        return action;
    }

    void scrub(QPointF position, double y)
    {
        m_cancelling = y < m_config.cancel && m_scrubbed;
        if (m_carrying) {
            if (m_cancelling) {
                m_over = Over::Nothing;
                m_overWorkspace = -1;
            } else if (y < m_config.depth) {
                m_level = TableLevel::Tabs;
                dropAim();
                const int tab = nearest(m_tabs, position.x());
                if (plusNearer(tab, position.x())) {
                    m_over = Over::Plus;
                    m_overWorkspace = -1;
                } else if (tab >= 0) {
                    m_over = Over::Workspace;
                    m_overWorkspace = tab;
                    m_hovered = tab;
                } else {
                    m_over = Over::Nothing;
                    m_overWorkspace = -1;
                }
            } else {
                // Down among the cards of the workspace it last passed over,
                // whose row hangs; over + it goes back to its own.
                if (m_over != Over::Workspace || m_overWorkspace < 0) m_hovered = m_carryFrom;
                m_level = TableLevel::Cards;
                m_over = Over::Workspace;
                m_overWorkspace = m_hovered;
            }
            return;
        }
        if (m_cancelling) return;
        if (y < m_config.depth) {
            if (m_level != TableLevel::Tabs) {
                fold();
                m_level = TableLevel::Tabs;
                m_card = -1;
                m_refused = -1;
            }
            m_plusCard = -1;
            const int tab = nearest(m_tabs, position.x());
            if (plusNearer(tab, position.x())) {
                if (!m_onPlus) m_scrubbed = true;
                m_onPlus = true;
            } else if (tab >= 0 && (tab != m_hovered || m_onPlus)) {
                m_onPlus = false;
                m_hovered = tab;
                m_scrubbed = true;
            }
        } else if (m_onPlus && m_level == TableLevel::Tabs) {
            // Under + the finger chooses among its cards, which are only
            // ever chosen, never carried; with none it stays on +.
            if (!m_plusCards.isEmpty()) {
                m_plusCard = nearest(m_plusCards, position.x());
                m_scrubbed = true;
            }
        } else {
            if (m_level == TableLevel::Tabs) {
                m_level = TableLevel::Cards;
                m_locked = m_hovered;
                m_card = -1;
                m_scrubbed = true;
            }
            if (m_membersOf >= 0 && y > m_config.lift) {
                // Under a Stack whose cards hang the finger chooses among
                // them, and past the line under them it lifts one out.
                m_level = TableLevel::Members;
                m_card = m_membersOf;
                const int member = nearest(memberCentres(), position.x());
                if (member != m_member) {
                    m_member = member;
                    m_refusedMember = -1;
                }
                if (!std::isnan(m_membersLift) && y > m_membersLift && m_member >= 0 && m_member != m_refusedMember)
                    liftMember(m_member);
            } else {
                if (m_level == TableLevel::Members) {
                    m_level = TableLevel::Cards;
                    m_member = -1;
                }
                const QList<double> cards = m_locked >= 0 && m_locked < m_cards.size()
                    ? m_cards[m_locked] : QList<double>{};
                const int card = nearest(cards, position.x());
                if (card != m_card) {
                    m_card = card;
                    m_refused = -1;
                    if (card != m_membersOf) fold();
                }
                // Past the line under the cards the finger means to move one.
                if (y > m_config.lift && m_card >= 0 && m_card != m_refused) liftCard(m_card);
            }
        }
        if (y > m_config.cancel + 0.03) m_scrubbed = true;
    }

    // The keys move the card in hand one place along the tabs, + coming
    // after the last. The displays show the workspace it is over, its cards
    // hanging, as they do under a carrying finger.
    void carryStep(int steps)
    {
        if (m_tabs.isEmpty()) return;
        const int count = int(m_tabs.size());
        const int last = count - (hasPlus() ? 0 : 1);
        const int at = std::clamp((m_over == Over::Plus ? count : m_overWorkspace) + steps, 0, last);
        m_level = TableLevel::Tabs;
        m_card = -1;
        if (at == count) {
            m_over = Over::Plus;
            m_overWorkspace = -1;
        } else {
            m_over = Over::Workspace;
            m_overWorkspace = at;
            m_hovered = at;
        }
    }

    // The card the keys carry goes back to its place, chosen again.
    void putBack()
    {
        const int card = m_carryCard;
        const int from = m_carryFrom;
        const int member = m_carryMember;
        m_keyCarry = false;
        m_carrying = false;
        m_carryCard = -1;
        m_carryMember = -1;
        m_carryFrom = -1;
        m_over = Over::Nothing;
        m_overWorkspace = -1;
        dropAim();
        m_onPlus = false;
        m_level = member >= 0 ? TableLevel::Members : TableLevel::Cards;
        m_locked = from;
        m_hovered = from;
        m_card = card;
        m_membersOf = member >= 0 ? card : -1;
        m_member = member;
    }

    // Shift let go: on another tab the card moves there, on + a workspace is
    // made for it, and on its own tab it goes back, still chosen. Table stays
    // open either way; the keys have chosen nothing to open.
    TableAction dropKeyCarry()
    {
        const int card = m_carryCard;
        const int from = m_carryFrom;
        const int member = m_carryMember;
        const Over over = m_over;
        const int destination = m_overWorkspace;
        putBack();
        if (over != Over::Nothing && member >= 0) fold();
        if (over == Over::Plus) return {TableAction::Kind::Create, from, card, -1, member};
        if (over == Over::Workspace && destination != from)
            return {TableAction::Kind::Move, from, card, destination, member};
        return {TableAction::Kind::StayOpen};
    }

    TableAction dropCarry()
    {
        const int card = m_carryCard;
        const int from = m_carryFrom;
        const int member = m_carryMember;
        const Over over = m_over;
        const int destination = m_overWorkspace;
        const int target = m_aimArmed ? m_aim : -1;
        m_carrying = false;
        m_carryCard = -1;
        m_carryMember = -1;
        m_carryFrom = -1;
        m_over = Over::Nothing;
        m_overWorkspace = -1;
        dropAim();
        if (m_cancelling || over == Over::Nothing) {
            m_cancelling = false;
            m_sticky = true;
            return {TableAction::Kind::StayOpen};
        }
        if (member >= 0) fold();
        if (over == Over::Plus) return {TableAction::Kind::Create, from, card, -1, member};
        // Held over a card long enough: it stacks there, and Table stays open.
        if (target >= 0 && m_level == TableLevel::Cards && destination >= 0) {
            m_sticky = true;
            return {TableAction::Kind::Stack, from, card, destination, member, target};
        }
        // A card of a Stack let go in its own workspace's row stands on its
        // own, just after its Stack.
        if (member >= 0 && destination == from && m_level == TableLevel::Cards) {
            m_sticky = true;
            return {TableAction::Kind::TakeOut, from, card, -1, member};
        }
        // Released on its own place: it opens.
        if (destination == from && m_level == TableLevel::Cards)
            return closeWith({TableAction::Kind::Activate, from, card});
        if (destination == from) {
            m_sticky = true;
            return {TableAction::Kind::StayOpen};
        }
        return {TableAction::Kind::Move, from, card, destination, member};
    }

    // + makes an empty workspace and Table stays open as a menu bar, so a
    // card can be carried to the new tab or the tab entered.
    TableAction makeNew()
    {
        m_sticky = true;
        m_down = false;
        m_onPlus = false;
        m_plusCard = -1;
        m_level = TableLevel::Tabs;
        m_card = -1;
        return {TableAction::Kind::New};
    }

    // + is the tab in view and its cards hang under it, none chosen. Table
    // stays open as a menu bar; nothing is made until a card is chosen.
    TableAction openPlusTray()
    {
        interrupt();
        m_onPlus = true;
        m_level = TableLevel::Tabs;
        m_card = -1;
        m_plusCard = -1;
        return {TableAction::Kind::StayOpen};
    }

    // The first card under + is the empty workspace + has always made; each
    // after it is a saved Layout, opened in a workspace of its own.
    TableAction choosePlusCard(int card)
    {
        if (card <= 0) return makeNew();
        return closeWith({TableAction::Kind::OpenLayout, -1, card - 1});
    }

    TableAction closeWith(TableAction action)
    {
        close();
        return action;
    }

    TableConfig m_config;
    bool m_open = false;
    bool m_sticky = false;
    bool m_down = false;
    bool m_scrubbed = false;
    bool m_cancelling = false;
    TableLevel m_level = TableLevel::Tabs;
    int m_current = -1;
    int m_hovered = -1;
    int m_locked = -1;
    int m_card = -1;
    qint64 m_openedAt = 0;
    QPointF m_pressAt;
    QPointF m_last;
    double m_lastY = 1.0;
    // A menu bar's card under the press, until it is dragged or let go.
    int m_pressCard = -1;
    // A menu bar's hanging card of a Stack under the press, the same way.
    int m_pressMember = -1;
    // The Stack whose cards hang, and the one of them chosen.
    int m_membersOf = -1;
    int m_member = -1;
    int m_refusedMember = -1;
    double m_membersLift = std::numeric_limits<double>::quiet_NaN();
    int m_carryMember = -1;
    int m_aim = -1;
    bool m_aimArmed = false;
    // The tab a menu bar's press landed on, while it may still become a hold.
    int m_pressTab = -1;
    qint64 m_pressedAt = 0;
    bool m_held = false;
    // The card that lifted and could not be carried.
    int m_refused = -1;
    bool m_onPlus = false;
    // The card under + chosen, while + is in view.
    int m_plusCard = -1;
    bool m_carrying = false;
    bool m_keyCarry = false;
    int m_carryCard = -1;
    int m_carryFrom = -1;
    Over m_over = Over::Nothing;
    int m_overWorkspace = -1;
    QList<double> m_tabs;
    double m_plus = std::numeric_limits<double>::quiet_NaN();
    QList<QList<double>> m_cards;
    QList<double> m_plusCards;
    QList<QList<QList<double>>> m_members;
};

} // namespace Kadunce
