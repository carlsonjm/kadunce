/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QHash>
#include <QList>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QUuid>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>

#include <algorithm>
#include <utility>
#include <optional>

namespace Kadunce
{
// Notes Gooseberry has stuck to windows, as its StuckNotes interface last
// described them (TETTEGOUCHE-CONTEXT.md § Stuck notes from Gooseberry).
// Kadunce keeps no notes of its own: it draws what the interface last said,
// and draws nothing while Gooseberry is absent.
struct StuckNote {
    QString id;
    QString title;
    QString colourHex;
};

struct StuckNotesEntry {
    QList<QUuid> windowIds;
    QString caption;
    QString app;
    int count = 0;
    // The top note first.
    QList<StuckNote> notes;
    // Its notes are out over the window, which Spread shows as fanned.
    bool shown = false;
};

// The entries of Windows() or WindowsChanged, each already a map. An entry
// names its window by the compositor's id, a UUID; X11's decimal ids parse to
// no UUID and match no card. An entry with no note is dropped.
[[nodiscard]] inline QList<StuckNotesEntry> stuckNotesEntries(const QVariantList &windows)
{
    QList<StuckNotesEntry> entries;
    for (const QVariant &value : windows) {
        const QVariantMap map = value.toMap();
        StuckNotesEntry entry;
        for (const QString &id : map.value(QStringLiteral("windowIds")).toStringList()) {
            const QUuid uuid(id);
            if (!uuid.isNull()) entry.windowIds.append(uuid);
        }
        entry.caption = map.value(QStringLiteral("caption")).toString();
        entry.app = map.value(QStringLiteral("app")).toString();
        for (const QVariant &noteValue : map.value(QStringLiteral("notes")).toList()) {
            const QVariantMap note = noteValue.toMap();
            const QString id = note.value(QStringLiteral("id")).toString();
            if (id.isEmpty()) continue;
            entry.notes.append({id, note.value(QStringLiteral("title")).toString(),
                                note.value(QStringLiteral("colourHex")).toString()});
        }
        entry.shown = map.value(QStringLiteral("shown")).toBool();
        entry.count = std::max<int>(map.value(QStringLiteral("count")).toInt(), entry.notes.size());
        if (entry.windowIds.isEmpty() || entry.notes.isEmpty()) continue;
        entries.append(entry);
    }
    return entries;
}

// A card in Spread as notes see it: its window's id and where it stands, in
// logical pixels. Lists run nearest the eye first.
struct NotesCard {
    QUuid id;
    QRectF rect;
};

// The stack and the fan are the same size on every card, whatever the
// card's scale, so a near card's notes are no easier to hit than a far one's.
struct NoteGeometry {
    // The top note alone stands for them all (J, 7 October: option A).
    static constexpr double Square = 30.0;
    // The count's badge, on the note's top-right corner.
    static constexpr double Badge = 18.0;
    static constexpr double BadgeInset = 7.0;
    static constexpr double Inset = 12.0;
    // A finger's reach around the note, so a 30 px note is a 46 px target and
    // more with its badge.
    static constexpr double Reach = 8.0;
    static constexpr double NoteWidth = 136.0;
    static constexpr double NoteHeight = 88.0;
    static constexpr double NoteGap = 8.0;
    static constexpr double Radius = 8.0;
};

// The stack at the card's bottom-right corner: one sheet, the top note,
// however many there are. Kept as a list so the drawing and the reach read it
// the same way.
[[nodiscard]] inline QList<QRectF> noteStackSquares(const QRectF &card, int count)
{
    using G = NoteGeometry;
    if (card.isEmpty() || count <= 0) return {};
    return {QRectF(card.right() - G::Inset - G::Square, card.bottom() - G::Inset - G::Square,
                   G::Square, G::Square)};
}

// The count's badge over the top note's top-right corner, shown when there
// is more than one note.
[[nodiscard]] inline QRectF noteStackBadge(const QRectF &top)
{
    using G = NoteGeometry;
    if (top.isEmpty()) return {};
    return QRectF(top.right() - G::Badge / 2 - G::BadgeInset, top.top() - G::Badge / 2 + G::BadgeInset,
                  G::Badge, G::Badge);
}

[[nodiscard]] inline QRectF noteStackReach(const QRectF &card, int count)
{
    const auto squares = noteStackSquares(card, count);
    if (squares.isEmpty()) return {};
    QRectF reach = squares.last();
    if (count > 1) reach |= noteStackBadge(squares.last());
    return reach.adjusted(-NoteGeometry::Reach, -NoteGeometry::Reach,
                          NoteGeometry::Reach, NoteGeometry::Reach);
}

// The notes fanned out over the card, the top note beside the stack, which
// stays to fold them, and the rest in rows leftward and then upward from it,
// as many as the card holds. A card too small for one still shows the top
// note beside its corner.
[[nodiscard]] inline QList<QRectF> fannedNoteRects(const QRectF &card, int count)
{
    using G = NoteGeometry;
    QList<QRectF> rects;
    if (card.isEmpty() || count <= 0) return rects;
    const auto fits = [](double length, double size) {
        return std::max(1, int((length - 2 * G::Inset + G::NoteGap) / (size + G::NoteGap)));
    };
    const double stack = G::Square + G::Reach + G::NoteGap;
    const int columns = fits(card.width() - stack, G::NoteWidth);
    const int rows = fits(card.height(), G::NoteHeight);
    const int shown = std::min(count, columns * rows);
    for (int index = 0; index < shown; ++index) {
        const int column = index % columns;
        const int row = index / columns;
        rects.append(QRectF(card.right() - stack - G::Inset - G::NoteWidth - column * (G::NoteWidth + G::NoteGap),
                            card.bottom() - G::Inset - G::NoteHeight - row * (G::NoteHeight + G::NoteGap),
                            G::NoteWidth, G::NoteHeight));
    }
    return rects;
}

// Notes on Spread's cards, and the one contact the notes hold from press to
// release. Whether a card's notes are fanned is Gooseberry's: they are fanned
// exactly while its entry says they are shown over the window, so notes out
// over a window arrive fanned and stay out after Spread closes. A tap on a
// stack asks Gooseberry to toggle them, and shows the answer it expects until
// Gooseberry says otherwise. A hold on a fanned note, or on a stack for its
// top note, carries that note, and letting it go on another card asks
// Gooseberry to stick it there. Any other press is not the notes'.
// Gooseberry's reply to a toggle confirms the guess or ends it: a reply that
// fails, or that names the other state, drops the guess and asks for a fresh
// read, since no signal follows a toggle that changed nothing.
class StuckNotesSpread
{
public:
    enum class Contact { None, Stack, Note };
    struct Request {
        enum class Kind { Toggle, Stick };
        Kind kind = Kind::Toggle;
        QUuid window;
        QString noteId;
    };

    // Gooseberry's word replaces every guess made since it last spoke.
    void setEntries(const QList<StuckNotesEntry> &entries)
    {
        m_entries = entries;
        m_expected.clear();
        if (m_carry && !noteOn(m_carry->from, m_carry->note.id)) m_carry.reset();
    }

    [[nodiscard]] bool isEmpty() const { return m_entries.isEmpty(); }

    [[nodiscard]] const StuckNotesEntry *entryFor(const QUuid &window) const
    {
        if (window.isNull()) return nullptr;
        for (const auto &entry : m_entries)
            if (entry.windowIds.contains(window)) return &entry;
        return nullptr;
    }

    [[nodiscard]] bool fanned(const QUuid &window) const
    {
        const auto *entry = entryFor(window);
        return entry && m_expected.value(window, entry->shown);
    }

    [[nodiscard]] Contact contact() const { return m_contact; }

    // A press in Spread. True when it is the notes': on a stack, or on a note
    // fanned over its card. Cards run nearest the eye first.
    bool press(const QPointF &position, const QList<NotesCard> &cards)
    {
        cancel();
        for (const auto &card : cards) {
            const auto *entry = entryFor(card.id);
            if (!entry) continue;
            if (noteStackReach(card.rect, entry->count).contains(position)) {
                begin(Contact::Stack, card.id, entry->notes.first(), position);
                return true;
            }
            if (!fanned(card.id)) continue;
            const auto rects = fannedNoteRects(card.rect, entry->notes.size());
            for (int index = 0; index < rects.size(); ++index) {
                if (!rects.at(index).contains(position)) continue;
                begin(Contact::Note, card.id, entry->notes.at(index), position);
                return true;
            }
        }
        return false;
    }

    // The press was held still long enough: its note is carried.
    bool hold()
    {
        if (m_contact == Contact::None || !m_pressed || m_carry) return false;
        m_carry = Carry{m_pressedOn, *m_pressed, m_pressedAt};
        return true;
    }

    void move(const QPointF &position)
    {
        if (m_carry) m_carry->at = position;
    }

    // The contact lifts; `still` when it never moved further than a tap may.
    // Returns what to ask Gooseberry: to toggle a stack tapped still, or to
    // stick a carried note let go on another card. Nothing else asks.
    std::optional<Request> release(const QPointF &position, bool still, const QList<NotesCard> &cards)
    {
        const Contact contact = std::exchange(m_contact, Contact::None);
        const QUuid pressedOn = std::exchange(m_pressedOn, {});
        m_pressed.reset();
        if (m_carry) {
            const Carry carry = *std::exchange(m_carry, std::nullopt);
            const QUuid target = cardAt(position, cards);
            if (target.isNull() || target == carry.from) return std::nullopt;
            return Request{Request::Kind::Stick, target, carry.note.id};
        }
        if (!still || contact != Contact::Stack || !entryFor(pressedOn)) return std::nullopt;
        m_expected.insert(pressedOn, !fanned(pressedOn));
        return Request{Request::Kind::Toggle, pressedOn, {}};
    }

    // Gooseberry answered a toggle of the window's notes: whether they are
    // shown now, or nothing when the call failed. True when the guess made
    // for it was wrong and dropped, so Windows() is to be read again. A reply
    // that comes after Gooseberry has already said what it holds changes
    // nothing, since its word has replaced the guess.
    bool toggleAnswered(const QUuid &window, std::optional<bool> shown)
    {
        const auto guess = m_expected.constFind(window);
        if (guess == m_expected.cend()) return false;
        if (shown && *shown == *guess) return false;
        m_expected.erase(guess);
        return true;
    }

    // The contact was taken away, or Spread closed under it: nothing is asked.
    void cancel()
    {
        m_contact = Contact::None;
        m_pressedOn = {};
        m_pressed.reset();
        m_carry.reset();
    }

    [[nodiscard]] bool carrying() const { return m_carry.has_value(); }
    [[nodiscard]] std::optional<StuckNote> carriedNote() const
    {
        return m_carry ? std::optional<StuckNote>(m_carry->note) : std::nullopt;
    }
    [[nodiscard]] QPointF carriedAt() const { return m_carry ? m_carry->at : QPointF(); }
    [[nodiscard]] QUuid carriedFrom() const { return m_carry ? m_carry->from : QUuid(); }

    // The card a carried note would be stuck to if let go now: one under the
    // contact other than the card it came from.
    [[nodiscard]] QUuid dropTarget(const QList<NotesCard> &cards) const
    {
        if (!m_carry) return {};
        const QUuid target = cardAt(m_carry->at, cards);
        return target == m_carry->from ? QUuid() : target;
    }

    // Where a carried note is drawn: centred on the contact.
    [[nodiscard]] QRectF carriedRect() const
    {
        if (!m_carry) return {};
        return QRectF(m_carry->at.x() - NoteGeometry::NoteWidth / 2,
                      m_carry->at.y() - NoteGeometry::NoteHeight / 2,
                      NoteGeometry::NoteWidth, NoteGeometry::NoteHeight);
    }

private:
    struct Carry {
        QUuid from;
        StuckNote note;
        QPointF at;
    };

    void begin(Contact contact, const QUuid &card, const StuckNote &note, const QPointF &position)
    {
        m_contact = contact;
        m_pressedOn = card;
        m_pressed = note;
        m_pressedAt = position;
    }

    [[nodiscard]] bool noteOn(const QUuid &window, const QString &noteId) const
    {
        const auto *entry = entryFor(window);
        if (!entry) return false;
        return std::any_of(entry->notes.cbegin(), entry->notes.cend(),
                           [&](const StuckNote &note) { return note.id == noteId; });
    }

    static QUuid cardAt(const QPointF &position, const QList<NotesCard> &cards)
    {
        for (const auto &card : cards)
            if (card.rect.contains(position)) return card.id;
        return {};
    }

    QList<StuckNotesEntry> m_entries;
    // Toggles asked of Gooseberry and not yet answered, by window.
    QHash<QUuid, bool> m_expected;
    Contact m_contact = Contact::None;
    QUuid m_pressedOn;
    std::optional<StuckNote> m_pressed;
    QPointF m_pressedAt;
    std::optional<Carry> m_carry;
};
} // namespace Kadunce
