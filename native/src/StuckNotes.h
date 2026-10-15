/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "NotePlacement.h"

#include <QList>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QStringList>
#include <QUuid>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>

#include <algorithm>
#include <cmath>
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
    // Where it stands on its window, as shares of the window's width and
    // height; negative when it was never placed there, or when Gooseberry
    // does not say, and it is then in the corner stack.
    QPointF place{-1.0, -1.0};
    // How big it is over its window, in the window's pixels; empty when it
    // was never resized there.
    QSizeF size;

    [[nodiscard]] bool placed() const { return notePlaced(place); }
};

struct StuckNotesEntry {
    QList<QUuid> windowIds;
    QString caption;
    QString app;
    int count = 0;
    // The top note first.
    QList<StuckNote> notes;
    // Its notes are up over the window, which Spread shows by drawing those
    // in place in full rather than faint.
    bool shown = false;
    // Up, but folded into one dot where the top note stands, which Spread
    // draws in their place. A Gooseberry that does not say never folds.
    bool folded = false;
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
            StuckNote stuck;
            stuck.id = id;
            stuck.title = note.value(QStringLiteral("title")).toString();
            stuck.colourHex = note.value(QStringLiteral("colourHex")).toString();
            // A Gooseberry that tells no place, or one that cannot be read,
            // leaves the note in the stack.
            const auto number = [&note](const char *key) {
                bool ok = false;
                const double value = note.value(QLatin1String(key)).toDouble(&ok);
                return ok && std::isfinite(value) ? value : -1.0;
            };
            const QPointF place(number("x"), number("y"));
            if (notePlaced(place) && place.x() <= 1.0 && place.y() <= 1.0) stuck.place = place;
            const QSizeF size(number("width"), number("height"));
            if (noteSized(size)) stuck.size = size;
            entry.notes.append(stuck);
        }
        entry.shown = map.value(QStringLiteral("shown")).toBool();
        entry.folded = entry.shown && map.value(QStringLiteral("folded")).toBool();
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
    // The window's own size, which the card shows scaled; placed notes stand
    // on the card only when it is known.
    QSizeF window;
    // Where the window itself is drawn on the card this frame, which the card
    // may show smaller than its slot and moves with it; empty for the slot.
    QRectF face;

    [[nodiscard]] QRectF drawnFace() const { return face.isEmpty() ? rect : face; }
};

// The stack is the same size on every card, whatever the card's scale, so a
// near card's notes are no easier to hit than a far one's.
struct NoteGeometry {
    // The stack is a mini note: the top note with its first words, the
    // next one's edge peeking above it when there are more.
    static constexpr double MiniWidth = 64.0;
    static constexpr double MiniHeight = 44.0;
    static constexpr double MiniRadius = 6.0;
    static constexpr double Peek = 4.0;
    static constexpr double Inset = 12.0;
    // A finger's reach around the mini note, an 80 by 60 px target.
    static constexpr double Reach = 8.0;
    // A carried note, and the size a note's corners, margin and words are
    // drawn for.
    static constexpr double NoteWidth = 136.0;
    static constexpr double NoteHeight = 88.0;
    static constexpr double Radius = 8.0;
    // A note standing in its place while its notes are put away is drawn
    // faint; brought up over the window, it is drawn in full.
    static constexpr double PutAwayOpacity = 0.6;
};

// The stack at the card's bottom-right corner, the deepest sheet first and
// the top note last: the mini note, and behind it, when there are more, the
// next note's edge peeking above.
[[nodiscard]] inline QList<QRectF> noteStackSquares(const QRectF &card, int count)
{
    using G = NoteGeometry;
    if (card.isEmpty() || count <= 0) return {};
    const QRectF top(card.right() - G::Inset - G::MiniWidth, card.bottom() - G::Inset - G::MiniHeight,
                     G::MiniWidth, G::MiniHeight);
    if (count == 1) return {top};
    return {top.translated(-G::Peek / 2, -G::Peek), top};
}

[[nodiscard]] inline QRectF noteStackReach(const QRectF &card, int count)
{
    QRectF reach;
    for (const QRectF &sheet : noteStackSquares(card, count)) reach |= sheet;
    if (reach.isEmpty()) return {};
    return reach.adjusted(-NoteGeometry::Reach, -NoteGeometry::Reach,
                          NoteGeometry::Reach, NoteGeometry::Reach);
}

// What a card shows of its notes, in the order they are drawn, the lowest
// first: each placed note where it stands on the window, the oldest lowest,
// and over them the notes never placed, as the corner stack. A press is the
// topmost mark's whose reach holds it, so what is touched is what is drawn.
//
// Every rectangle is worked out afresh from the rectangles passed in, with
// nothing kept between calls, so a card drawn mid-motion carries its notes
// exactly where it stands that frame. A placed note stands on `face`, where
// the window is drawn; the stack keeps to the card's corner. A note that has
// a place is never put in the stack: while its place on the card cannot be
// worked out, as before the window's size is known, it is not drawn at all,
// so it never starts from the corner and moves to its place. Folded on the
// window, the notes are their dot alone, a tap on it a tap on the top note.
struct CardNoteMark {
    enum class Kind { Stack, Note, Dot };
    Kind kind = Kind::Note;
    // The note in the entry: the note drawn, or the stack's or the dot's top
    // note.
    int note = 0;
    QRectF rect;
    QRectF reach;
    // For the stack: its sheets, the deepest first, the note each sheet
    // shows, and the count shown.
    QList<QRectF> sheets;
    QList<int> sheetNotes;
    int count = 0;
};

// The dot a window's folded notes are, on a card that shows the window at
// `face`: where it stands on the window, scaled with the card, and the size
// the title bar draws it at on every card, a finger's 44 px around it. Empty
// while the window's size is not known.
[[nodiscard]] inline std::optional<CardNoteMark> foldedDotMark(const QRectF &card, const QRectF &face,
                                                               const QSizeF &window, const StuckNotesEntry &entry)
{
    using P = NotePlacement;
    if (entry.notes.isEmpty() || face.isEmpty() || window.isEmpty()) return std::nullopt;
    const auto &top = entry.notes.first();
    const QPointF at = foldedDotCentre(window, noteRectOnWindow(window, top.place, top.size, 0));
    const QPointF centre(face.x() + at.x() * face.width() / window.width(),
                         face.y() + at.y() * face.height() / window.height());
    CardNoteMark dot;
    dot.kind = CardNoteMark::Kind::Dot;
    dot.note = 0;
    dot.count = std::max<int>(entry.count, int(entry.notes.size()));
    dot.rect = QRectF(centre.x() - P::DotRadius, centre.y() - P::DotRadius, 2 * P::DotRadius, 2 * P::DotRadius);
    dot.reach = QRectF(centre.x() - P::DotTarget / 2, centre.y() - P::DotTarget / 2, P::DotTarget, P::DotTarget)
                    .intersected(card | face);
    return dot;
}

[[nodiscard]] inline QList<CardNoteMark> cardNoteMarks(const QRectF &card, const QRectF &face,
                                                        const QSizeF &window, const StuckNotesEntry &entry)
{
    QList<CardNoteMark> marks;
    if (card.isEmpty()) return marks;
    const QRectF drawn = face.isEmpty() ? card : face;
    // Folded on the window, the notes are their dot on the card too.
    if (entry.folded) {
        if (const auto dot = foldedDotMark(card, drawn, window, entry)) marks.append(*dot);
        return marks;
    }
    QList<int> loose;
    for (int index = int(entry.notes.size()) - 1; index >= 0; --index) {
        const auto &note = entry.notes.at(index);
        if (!note.placed()) {
            loose.prepend(index);
            continue;
        }
        const QRectF rect = placedNoteRect(drawn, window, note.place, note.size);
        if (rect.isEmpty()) continue;
        CardNoteMark mark;
        mark.note = index;
        mark.rect = rect;
        // A finger's reach around it, kept on its own card.
        mark.reach = rect.adjusted(-NoteGeometry::Reach, -NoteGeometry::Reach,
                                   NoteGeometry::Reach, NoteGeometry::Reach).intersected(card | drawn);
        marks.append(mark);
    }
    if (loose.isEmpty()) return marks;
    // The count is of the notes in the stack; Gooseberry may count more notes
    // than it lists.
    const int placed = int(std::count_if(entry.notes.cbegin(), entry.notes.cend(),
                                         [](const StuckNote &note) { return note.placed(); }));
    CardNoteMark stack;
    stack.kind = CardNoteMark::Kind::Stack;
    stack.note = loose.first();
    stack.count = std::max<int>(int(loose.size()), entry.count - placed);
    stack.sheets = noteStackSquares(card, stack.count);
    for (int sheet = 0; sheet < stack.sheets.size(); ++sheet)
        stack.sheetNotes.append(loose.at(std::min<int>(int(stack.sheets.size()) - 1 - sheet, int(loose.size()) - 1)));
    stack.rect = stack.sheets.last();
    stack.reach = noteStackReach(card, stack.count);
    marks.append(stack);
    return marks;
}

// What the window's notes look like over it, in the compositor's pixels, the
// lowest first: each note where Gooseberry draws it, a note never placed at
// its starting place rather than in a stack, or the dot when they are folded.
// Spread draws these over the Active card it has just opened until
// Gooseberry's own notes stand there, so nothing moves as one gives way to
// the other.
[[nodiscard]] inline QList<CardNoteMark> windowNoteMarks(const QRectF &frame, const StuckNotesEntry &entry)
{
    QList<CardNoteMark> marks;
    if (frame.isEmpty()) return marks;
    if (entry.folded) {
        if (const auto dot = foldedDotMark(frame, frame, frame.size(), entry)) marks.append(*dot);
        return marks;
    }
    for (int index = int(entry.notes.size()) - 1; index >= 0; --index) {
        const auto &note = entry.notes.at(index);
        const QRectF rect = noteRectOnWindow(frame.size(), note.place, note.size, index).translated(frame.topLeft());
        if (rect.isEmpty()) continue;
        CardNoteMark mark;
        mark.note = index;
        mark.rect = rect;
        mark.reach = rect;
        marks.append(mark);
    }
    return marks;
}

// Whether Gooseberry's notes surface stands over the window: laid over it
// from its top-left, as big as the window or as much of it as its display
// holds, give or take the pixel either rounds to.
[[nodiscard]] inline bool notesSurfaceStandsOn(const QRectF &surface, const QRectF &window, const QRectF &display)
{
    if (surface.isEmpty() || window.isEmpty()) return false;
    const auto close = [](double a, double b) { return std::abs(a - b) <= 1.0; };
    const QSizeF size = window.size().boundedTo(display.isEmpty() ? window.size() : display.size());
    return close(surface.x(), window.x()) && close(surface.y(), window.y()) && close(surface.width(), size.width())
        && close(surface.height(), size.height());
}

// Gooseberry's notes hold the keys while a note is typed on. Spread opening
// ends the typing, the notes give the keys back, and KWin hands them to the
// window they lay over, the card Spread opened from. That activation is the
// notes stepping aside, not someone asking for the card: answered as a pick,
// it would put the card back in Active and drop the swipe still opening
// Spread. Told every activation in order, it says whether this one is that.
class NotesKeysReturn {
public:
    [[nodiscard]] bool activated(bool notesSurface, bool spreadShown, bool toWindowBefore)
    {
        const bool notesHeld = std::exchange(m_notesHeld, notesSurface);
        return notesHeld && !notesSurface && spreadShown && toWindowBefore;
    }

private:
    bool m_notesHeld = false;
};

// Notes on Spread's cards, and the one contact the notes hold from press to
// release. A stuck note is typed on its window, never in Spread: a tap on a
// note drawn on a card opens that card with the note out on the window,
// ready to type, and a tap on the stack opens it with the window's notes out.
// Opening the card is the effect's, as for any tap on a card; this only says
// what to ask Gooseberry. A hold on a note drawn, or on a stack for its top
// note, carries that note, and letting it go on another card asks Gooseberry
// to stick it there. Any other press is not the notes'.
class StuckNotesSpread
{
public:
    enum class Contact { None, Stack, Note };
    struct Request {
        // Write: bring the window's notes up with this note ready to type.
        // Show: bring the window's notes up. Stick: stick the note there.
        enum class Kind { Write, Show, Stick };
        Kind kind = Kind::Show;
        QUuid window;
        QString noteId;
    };

    void setEntries(const QList<StuckNotesEntry> &entries)
    {
        m_entries = entries;
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

    // Whether the window's notes are up over it, as Gooseberry last said.
    [[nodiscard]] bool shown(const QUuid &window) const
    {
        const auto *entry = entryFor(window);
        return entry && entry->shown;
    }

    [[nodiscard]] Contact contact() const { return m_contact; }

    // A press in Spread. True when it is the notes': on a note standing in
    // its place, or on a stack. Cards run nearest the eye first.
    bool press(const QPointF &position, const QList<NotesCard> &cards)
    {
        cancel();
        for (const auto &card : cards) {
            const auto *entry = entryFor(card.id);
            if (!entry) continue;
            const auto marks = cardNoteMarks(card.rect, card.face, card.window, *entry);
            for (auto mark = marks.crbegin(); mark != marks.crend(); ++mark) {
                if (!mark->reach.contains(position)) continue;
                // A folded dot is its top note: a tap types on it, a hold
                // carries it.
                begin(mark->kind == CardNoteMark::Kind::Stack ? Contact::Stack : Contact::Note, card.id,
                      entry->notes.at(mark->note), position);
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
    // Returns what to ask Gooseberry: a note tapped still is to be written on
    // its window, a stack tapped still brings the window's notes up, and a
    // carried note let go on another card is stuck there. Nothing else asks.
    // A tap asks the same however the notes stand: it never puts them away.
    std::optional<Request> release(const QPointF &position, bool still, const QList<NotesCard> &cards)
    {
        const Contact contact = std::exchange(m_contact, Contact::None);
        const QUuid pressedOn = std::exchange(m_pressedOn, {});
        const auto pressed = std::exchange(m_pressed, std::nullopt);
        if (m_carry) {
            const Carry carry = *std::exchange(m_carry, std::nullopt);
            const QUuid target = cardAt(position, cards);
            if (target.isNull() || target == carry.from) return std::nullopt;
            return Request{Request::Kind::Stick, target, carry.note.id};
        }
        if (!still || contact == Contact::None || !entryFor(pressedOn)) return std::nullopt;
        if (contact == Contact::Note && pressed && noteOn(pressedOn, pressed->id))
            return Request{Request::Kind::Write, pressedOn, pressed->id};
        return Request{Request::Kind::Show, pressedOn, {}};
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

    // Where to tap the card a tap on its notes opens, as if it were tapped
    // there: where the notes were let go, when a tap there reaches that card,
    // or else the first of its centre and the middles of its four quarters
    // that does, since a nearer card or the notes may cover part of it.
    // `reaches` names the card a tap at a point reaches. Empty when no point
    // reaches it.
    template<typename Reaches>
    [[nodiscard]] static std::optional<QPointF> openingPoint(const QUuid &window, const QPointF &position,
                                                             const QList<NotesCard> &cards, Reaches reaches)
    {
        const auto card = std::find_if(cards.cbegin(), cards.cend(),
                                       [&](const NotesCard &each) { return each.id == window; });
        if (card == cards.cend() || window.isNull()) return std::nullopt;
        const QRectF rect = card->rect;
        const QPointF centre = rect.center();
        const double dx = rect.width() / 4.0;
        const double dy = rect.height() / 4.0;
        for (const QPointF &at : {position, centre, centre + QPointF(-dx, -dy), centre + QPointF(dx, -dy),
                                  centre + QPointF(-dx, dy), centre + QPointF(dx, dy)}) {
            if (rect.contains(at) && reaches(at) == window) return at;
        }
        return std::nullopt;
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
    Contact m_contact = Contact::None;
    QUuid m_pressedOn;
    std::optional<StuckNote> m_pressed;
    QPointF m_pressedAt;
    std::optional<Carry> m_carry;
};
} // namespace Kadunce
