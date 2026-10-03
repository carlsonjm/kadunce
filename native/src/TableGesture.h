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

enum class TableLevel { Tabs, Cards };

struct TableAction {
    enum class Kind {
        None,
        // The tabs stay open as a menu bar; nothing else changes.
        StayOpen,
        // Table closes and nothing changes.
        Close,
        // Enter `workspace` as it was left.
        Enter,
        // Make card `card` of `workspace` Active there.
        Activate,
        // Move card `card` of `workspace` to workspace `destination`.
        Move,
        // Make a workspace for card `card` of `workspace` and move it there.
        Create,
        // Make an empty workspace and enter it.
        New,
        // Rename `workspace`: its tab held in a menu bar, right-clicked, or
        // F2.
        Rename,
    };
    Kind kind = Kind::None;
    int workspace = -1;
    int card = -1;
    int destination = -1;
};

// What a menu bar's tap landed on: a tab, the + tab, a card of the tray's
// workspace, or nothing of Table's.
struct TableHit {
    int tab = -1;
    bool plus = false;
    int card = -1;
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
    void setThresholds(double depth, double cancel, double lift)
    {
        m_config.depth = depth;
        m_config.cancel = cancel;
        m_config.lift = lift;
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
    void setLayout(QList<double> tabs, double plus, QList<QList<double>> cards)
    {
        m_tabs = std::move(tabs);
        m_plus = plus;
        m_cards = std::move(cards);
    }

    // The finger is down at position. On a sticky Table's card, the effect
    // found under it, the press previews that card, and dragged it lifts, as
    // anything is dragged on a desktop. On a sticky Table's tab, held still
    // it renames the tab (hold).
    void press(QPointF position, int card, int tab = -1, qint64 now = 0)
    {
        m_down = true;
        m_pressAt = position;
        m_last = position;
        m_held = false;
        m_pressTab = m_sticky && !m_carrying ? tab : -1;
        m_pressedAt = now;
        const int tray = trayWorkspace();
        if (m_sticky && card >= 0 && tray >= 0) {
            m_onPlus = false;
            m_level = TableLevel::Cards;
            m_locked = tray;
            m_hovered = tray;
            m_card = card;
            m_pressCard = card;
        }
    }

    void move(QPointF position, double y)
    {
        m_last = position;
        m_lastY = y;
        if (!m_open) return;
        if (m_pressTab >= 0 && distance(position, m_pressAt) > m_config.tapSlop) m_pressTab = -1;
        if (m_held) return;
        if (m_sticky && !m_carrying) {
            if (m_pressCard < 0 || distance(position, m_pressAt) <= m_config.tapSlop) return;
            liftCard(m_pressCard);
            m_pressCard = -1;
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
            if (m_level == TableLevel::Cards && m_card >= 0)
                return closeWith({TableAction::Kind::Activate, m_locked, m_card});
            if (m_level == TableLevel::Cards) return closeWith({TableAction::Kind::Close});
            // Lifting on + makes a workspace, as lifting on a tab enters one.
            if (m_onPlus) return closeWith({TableAction::Kind::New});
            return closeWith({TableAction::Kind::Enter, m_hovered});
        }
        // A menu bar: a tap previews, a second tap on the same tab enters.
        // A stroke pushed back up to the edge closes it.
        if (distance(position, m_pressAt) > m_config.tapSlop) {
            if (m_lastY < m_config.cancel) return closeWith({TableAction::Kind::Close});
            return {TableAction::Kind::StayOpen};
        }
        if (hit.card >= 0 && trayWorkspace() >= 0)
            return closeWith({TableAction::Kind::Activate, trayWorkspace(), hit.card});
        // + has nothing to preview, so one tap makes the workspace.
        if (hit.plus) return closeWith({TableAction::Kind::New});
        if (hit.tab >= 0) {
            if (hit.tab == m_hovered && m_level == TableLevel::Tabs && !m_onPlus)
                return closeWith({TableAction::Kind::Enter, hit.tab});
            m_onPlus = false;
            m_hovered = hit.tab;
            m_level = TableLevel::Tabs;
            m_card = -1;
            return {TableAction::Kind::StayOpen};
        }
        if (hit.insideRows) return {TableAction::Kind::StayOpen};
        // A tap on the preview goes to what it shows.
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
    void hover(int tab, int card, bool plus = false)
    {
        if (!m_open || !m_sticky || m_down || m_carrying) return;
        if (plus && hasPlus()) {
            m_onPlus = true;
            m_level = TableLevel::Tabs;
            m_card = -1;
        } else if (tab >= 0) {
            m_onPlus = false;
            m_hovered = tab;
            m_level = TableLevel::Tabs;
            m_card = -1;
        } else if (card >= 0 && trayWorkspace() >= 0) {
            m_locked = trayWorkspace();
            m_hovered = m_locked;
            m_level = TableLevel::Cards;
            m_card = card;
        }
    }

    // Steps across the level in view, one tab or card at a time, by the
    // wheel or an arrow key, + coming after the last tab; Table becomes a
    // menu bar.
    void step(int steps)
    {
        if (!m_open || m_carrying || steps == 0) return;
        m_sticky = true;
        if (m_level == TableLevel::Cards) {
            const int count = m_locked >= 0 && m_locked < m_cards.size() ? int(m_cards[m_locked].size()) : 0;
            if (count > 0) m_card = std::clamp(m_card < 0 ? (steps > 0 ? 0 : count - 1) : m_card + steps, 0, count - 1);
        } else if (!m_tabs.isEmpty()) {
            const int last = int(m_tabs.size()) - (hasPlus() ? 0 : 1);
            const int at = std::clamp((m_onPlus ? int(m_tabs.size()) : m_hovered) + steps, 0, last);
            m_onPlus = at == int(m_tabs.size());
            if (!m_onPlus) m_hovered = at;
        }
    }

    enum class Key { Left, Right, Up, Down, Enter, Escape, Rename };

    // The keyboard: left and right choose, down and up change level, Enter
    // goes where the displays show, F2 renames the workspace they show,
    // Escape cancels.
    TableAction key(Key key)
    {
        if (!m_open) return {};
        if (m_carrying) {
            if (key == Key::Escape) refuseCarry();
            return {TableAction::Kind::StayOpen};
        }
        m_sticky = true;
        switch (key) {
        case Key::Left:
            step(-1);
            break;
        case Key::Right:
            step(1);
            break;
        case Key::Down:
            if (m_level == TableLevel::Tabs && !m_onPlus && m_hovered >= 0 && m_hovered < m_cards.size()
                && !m_cards[m_hovered].isEmpty()) {
                m_level = TableLevel::Cards;
                m_locked = m_hovered;
                m_card = 0;
            }
            break;
        case Key::Up:
            if (m_level == TableLevel::Cards) {
                m_level = TableLevel::Tabs;
                m_hovered = m_locked;
                m_card = -1;
            }
            break;
        case Key::Enter:
            if (m_level == TableLevel::Cards && m_card >= 0)
                return closeWith({TableAction::Kind::Activate, m_locked, m_card});
            if (m_onPlus) return closeWith({TableAction::Kind::New});
            return closeWith({TableAction::Kind::Enter, m_hovered});
        case Key::Escape:
            return closeWith({TableAction::Kind::Close});
        case Key::Rename:
            if (m_level == TableLevel::Cards && m_locked >= 0) return renameTab(m_locked);
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
        m_carrying = false;
        m_carryCard = -1;
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
        m_carrying = false;
        m_carryCard = -1;
        m_carryFrom = -1;
        m_over = Over::Nothing;
        m_overWorkspace = -1;
        m_pressCard = -1;
        m_cancelling = false;
    }

    // After a card moved or a workspace was made, the rows describe the new
    // arrangement; Table stays open showing where the card went.
    void showAfterMove(int destination, int card)
    {
        m_sticky = true;
        m_onPlus = false;
        m_level = TableLevel::Cards;
        m_locked = destination;
        m_hovered = destination;
        m_card = card;
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
    [[nodiscard]] int carryCard() const { return m_carryCard; }
    [[nodiscard]] int carryFrom() const { return m_carryFrom; }
    // The finger, pointer or keys are on + rather than a tab.
    [[nodiscard]] bool onPlus() const { return m_open && m_onPlus && !m_carrying; }
    [[nodiscard]] QPointF finger() const { return m_last; }

    enum class Over { Nothing, Workspace, Plus };
    [[nodiscard]] Over over() const { return m_over; }
    [[nodiscard]] int overWorkspace() const { return m_overWorkspace; }

    // The workspace the displays show: the one the finger is on, or where the
    // person already is while a lift would cancel.
    [[nodiscard]] int shownWorkspace() const
    {
        if (!m_open || m_cancelling || (m_onPlus && !m_carrying)) return m_current;
        return m_level == TableLevel::Cards ? m_locked : m_hovered;
    }

    // The card the displays show in front, when the finger is on one.
    [[nodiscard]] int shownCard() const
    {
        if (!m_open || m_cancelling || m_level != TableLevel::Cards) return -1;
        return m_card;
    }

    // Whose cards hang below the tabs: the locked workspace's on the cards,
    // and otherwise the one under the finger or pointer, so a scrub shows the
    // cards before it reaches them, as a menu bar does.
    [[nodiscard]] int trayWorkspace() const
    {
        if (!m_open || (m_onPlus && !m_carrying && m_level == TableLevel::Tabs)) return -1;
        return m_level == TableLevel::Cards ? m_locked : m_hovered;
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
        m_carryFrom = m_locked;
        m_over = Over::Workspace;
        m_overWorkspace = m_locked;
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
                m_level = TableLevel::Cards;
                m_over = Over::Workspace;
                m_overWorkspace = m_locked;
            }
            return;
        }
        if (m_cancelling) return;
        if (y < m_config.depth) {
            if (m_level == TableLevel::Cards) {
                m_level = TableLevel::Tabs;
                m_card = -1;
                m_refused = -1;
            }
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
            // Nothing hangs under +; the finger stays on it.
        } else {
            if (m_level == TableLevel::Tabs) {
                m_level = TableLevel::Cards;
                m_locked = m_hovered;
                m_card = -1;
                m_scrubbed = true;
            }
            const QList<double> cards = m_locked >= 0 && m_locked < m_cards.size()
                ? m_cards[m_locked] : QList<double>{};
            const int card = nearest(cards, position.x());
            if (card != m_card) {
                m_card = card;
                m_refused = -1;
            }
            // Past the line under the cards the finger means to move one.
            if (y > m_config.lift && m_card >= 0 && m_card != m_refused) liftCard(m_card);
        }
        if (y > m_config.cancel + 0.03) m_scrubbed = true;
    }

    TableAction dropCarry()
    {
        const int card = m_carryCard;
        const int from = m_carryFrom;
        const Over over = m_over;
        const int destination = m_overWorkspace;
        m_carrying = false;
        m_carryCard = -1;
        m_carryFrom = -1;
        m_over = Over::Nothing;
        m_overWorkspace = -1;
        if (m_cancelling || over == Over::Nothing) {
            m_cancelling = false;
            m_sticky = true;
            return {TableAction::Kind::StayOpen};
        }
        if (over == Over::Plus) return {TableAction::Kind::Create, from, card};
        // Released on its own place: it opens.
        if (destination == from && m_level == TableLevel::Cards)
            return closeWith({TableAction::Kind::Activate, from, card});
        if (destination == from) {
            m_sticky = true;
            return {TableAction::Kind::StayOpen};
        }
        return {TableAction::Kind::Move, from, card, destination};
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
    // The tab a menu bar's press landed on, while it may still become a hold.
    int m_pressTab = -1;
    qint64 m_pressedAt = 0;
    bool m_held = false;
    // The card that lifted and could not be carried.
    int m_refused = -1;
    bool m_onPlus = false;
    bool m_carrying = false;
    int m_carryCard = -1;
    int m_carryFrom = -1;
    Over m_over = Over::Nothing;
    int m_overWorkspace = -1;
    QList<double> m_tabs;
    double m_plus = std::numeric_limits<double>::quiet_NaN();
    QList<QList<double>> m_cards;
};

} // namespace Kadunce
