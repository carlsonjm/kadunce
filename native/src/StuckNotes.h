/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

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
    static constexpr double Square = 28.0;
    static constexpr double Step = 5.0;
    static constexpr double Inset = 12.0;
    // A finger's reach around the stack, so a 38 px stack is a 54 px target.
    static constexpr double Reach = 8.0;
    static constexpr int Layers = 3;
    static constexpr double NoteWidth = 136.0;
    static constexpr double NoteHeight = 88.0;
    static constexpr double NoteGap = 8.0;
    static constexpr double Radius = 8.0;
};

// The stack's sheets at the card's bottom-right corner, the deepest first and
// the top note last: one sheet for each note, up to three, each further one
// stepped up and to the left behind it.
[[nodiscard]] inline QList<QRectF> noteStackSquares(const QRectF &card, int count)
{
    using G = NoteGeometry;
    QList<QRectF> squares;
    if (card.isEmpty() || count <= 0) return squares;
    const int layers = std::min(count, G::Layers);
    const QPointF front(card.right() - G::Inset - G::Square, card.bottom() - G::Inset - G::Square);
    for (int layer = layers - 1; layer >= 0; --layer)
        squares.append(QRectF(front - QPointF(G::Step * layer, G::Step * layer),
                              QSizeF(G::Square, G::Square)));
    return squares;
}

[[nodiscard]] inline QRectF noteStackReach(const QRectF &card, int count)
{
    QRectF reach;
    for (const QRectF &square : noteStackSquares(card, count)) reach |= square;
    if (reach.isEmpty()) return {};
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
    const double stack = G::Square + G::Step * (G::Layers - 1) + G::Reach + G::NoteGap;
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

// Notes on Spread's cards: which card's notes are fanned out, and the one
// contact the notes hold, from press to release. A tap on a stack fans its
// notes over the card, and a tap on it again or anywhere off a fanned note
// folds them. A hold on a fanned note, or on a stack for its top note,
// carries that note, and letting it go on another card sticks it there.
class StuckNotesSpread
{
public:
    enum class Contact { None, Stack, Note, Fold };
    struct Stick {
        QString noteId;
        QUuid window;
    };

    void setEntries(const QList<StuckNotesEntry> &entries)
    {
        m_entries = entries;
        if (!m_fanned.isNull() && !entryFor(m_fanned)) m_fanned = {};
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

    [[nodiscard]] QUuid fanned() const { return m_fanned; }
    [[nodiscard]] Contact contact() const { return m_contact; }

    // A press in Spread. True when it is the notes': on a stack, on a fanned
    // note, or anywhere while notes are fanned, which folds them at once.
    bool press(const QPointF &position, const QList<NotesCard> &cards)
    {
        cancel();
        if (!m_fanned.isNull()) {
            if (const auto card = cardOf(m_fanned, cards)) {
                const auto *entry = entryFor(m_fanned);
                if (noteStackReach(card->rect, entry->count).contains(position)) {
                    begin(Contact::Stack, m_fanned, entry->notes.first(), position);
                    return true;
                }
                const auto rects = fannedNoteRects(card->rect, entry->notes.size());
                for (int index = 0; index < rects.size(); ++index) {
                    if (!rects.at(index).contains(position)) continue;
                    begin(Contact::Note, m_fanned, entry->notes.at(index), position);
                    return true;
                }
            }
            m_fanned = {};
            m_contact = Contact::Fold;
            return true;
        }
        for (const auto &card : cards) {
            const auto *entry = entryFor(card.id);
            if (!entry || !noteStackReach(card.rect, entry->count).contains(position)) continue;
            begin(Contact::Stack, card.id, entry->notes.first(), position);
            return true;
        }
        return false;
    }

    // The press was held still long enough: its note is carried.
    bool hold()
    {
        if (m_contact != Contact::Stack && m_contact != Contact::Note) return false;
        if (!m_pressed || m_carry) return false;
        m_carry = Carry{m_pressedOn, *m_pressed, m_pressedAt};
        return true;
    }

    void move(const QPointF &position)
    {
        if (m_carry) m_carry->at = position;
    }

    // The contact lifts; `still` when it never moved further than a tap may.
    // Returns the stick to ask for when a carried note was let go on another
    // card; anything else changes no note.
    std::optional<Stick> release(const QPointF &position, bool still, const QList<NotesCard> &cards)
    {
        const Contact contact = std::exchange(m_contact, Contact::None);
        const QUuid pressedOn = std::exchange(m_pressedOn, {});
        m_pressed.reset();
        if (m_carry) {
            const Carry carry = *std::exchange(m_carry, std::nullopt);
            const QUuid target = cardAt(position, cards);
            if (target.isNull() || target == carry.from) return std::nullopt;
            m_fanned = {};
            return Stick{carry.note.id, target};
        }
        if (still && contact == Contact::Stack)
            m_fanned = m_fanned == pressedOn ? QUuid() : pressedOn;
        return std::nullopt;
    }

    void cancel()
    {
        m_contact = Contact::None;
        m_pressedOn = {};
        m_pressed.reset();
        m_carry.reset();
    }

    // Leaving Spread folds whatever was fanned and drops whatever was carried.
    void fold()
    {
        m_fanned = {};
        cancel();
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

    static std::optional<NotesCard> cardOf(const QUuid &id, const QList<NotesCard> &cards)
    {
        for (const auto &card : cards)
            if (card.id == id) return card;
        return std::nullopt;
    }

    static QUuid cardAt(const QPointF &position, const QList<NotesCard> &cards)
    {
        for (const auto &card : cards)
            if (card.rect.contains(position)) return card.id;
        return {};
    }

    QList<StuckNotesEntry> m_entries;
    QUuid m_fanned;
    Contact m_contact = Contact::None;
    QUuid m_pressedOn;
    std::optional<StuckNote> m_pressed;
    QPointF m_pressedAt;
    std::optional<Carry> m_carry;
};
} // namespace Kadunce
