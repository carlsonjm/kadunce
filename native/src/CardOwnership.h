/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QHash>
#include <QSet>
#include <QString>

#include <optional>
#include <vector>

namespace Kadunce
{

// CARD-LIFECYCLE.md §1 defines exactly three owners for an eligible window.
// No type held that value, so §14's first invariant - one window has one owner -
// could not be evaluated anywhere: individual cards live in the card stage and
// Bento panes in the desktop stage's per-output sessions, and Native is only
// absence from both. This value observes all three at once.
enum class CardOwner {
    Native,
    IndividualCard,
    BentoPane,
};

// One output's Bento session, reduced to the identities that decide ownership.
// It is the visible pane combination and nothing else: CARD-LIFECYCLE.md §5
// gives a window the layout cannot show to card ownership, so there is no
// second list here for one to be retained in.
struct BentoOwnershipView {
    QString output;
    std::vector<quintptr> panes;
};

struct OwnershipViolation {
    enum class Rule {
        // "One window has one owner."
        TwoOwners,
        // "One display has at most one Bento layout."
        DuplicateBentoLayout,
    };
    quintptr window = 0;
    Rule rule = Rule::TwoOwners;
    QString output;

    bool operator==(const OwnershipViolation &other) const = default;
};

// The six directed transitions §1 permits between its three owners. There is no
// seventh: an owner never changes except by one of these, and a transition that
// begins and ends at the same owner is not a transition at all.
enum class OwnershipTransition {
    AdmitToCard,   // Native -> individual card
    AdmitToBento,  // Native -> Bento pane
    ReleaseCard,   // individual card -> Native
    ReleaseBento,  // Bento pane -> Native
    CardToBento,   // individual card -> Bento pane
    BentoToCard,   // Bento pane -> individual card
};

[[nodiscard]] constexpr CardOwner transitionFrom(OwnershipTransition transition)
{
    switch (transition) {
    case OwnershipTransition::AdmitToCard:
    case OwnershipTransition::AdmitToBento:
        return CardOwner::Native;
    case OwnershipTransition::ReleaseCard:
    case OwnershipTransition::CardToBento:
        return CardOwner::IndividualCard;
    case OwnershipTransition::ReleaseBento:
    case OwnershipTransition::BentoToCard:
        return CardOwner::BentoPane;
    }
    return CardOwner::Native;
}

[[nodiscard]] constexpr CardOwner transitionTo(OwnershipTransition transition)
{
    switch (transition) {
    case OwnershipTransition::AdmitToCard:
    case OwnershipTransition::BentoToCard:
        return CardOwner::IndividualCard;
    case OwnershipTransition::AdmitToBento:
    case OwnershipTransition::CardToBento:
        return CardOwner::BentoPane;
    case OwnershipTransition::ReleaseCard:
    case OwnershipTransition::ReleaseBento:
        return CardOwner::Native;
    }
    return CardOwner::Native;
}

// Nothing outside the six is expressible, so a caller cannot invent a seventh.
[[nodiscard]] constexpr std::optional<OwnershipTransition> directedTransition(
    CardOwner from, CardOwner to)
{
    if (from == to) return std::nullopt;
    switch (from) {
    case CardOwner::Native:
        return to == CardOwner::IndividualCard ? OwnershipTransition::AdmitToCard
                                               : OwnershipTransition::AdmitToBento;
    case CardOwner::IndividualCard:
        return to == CardOwner::Native ? OwnershipTransition::ReleaseCard
                                       : OwnershipTransition::CardToBento;
    case CardOwner::BentoPane:
        return to == CardOwner::Native ? OwnershipTransition::ReleaseBento
                                       : OwnershipTransition::BentoToCard;
    }
    return std::nullopt;
}

// Assertion-only: this reports, it never repairs. Every violation it returns is
// a pre-existing defect in the caller, not a reason to refuse the caller's work.
[[nodiscard]] inline std::vector<OwnershipViolation> auditCardOwnership(
    const std::vector<quintptr> &individualCards,
    const std::vector<BentoOwnershipView> &sessions)
{
    std::vector<OwnershipViolation> violations;
    const QSet<quintptr> cards(individualCards.cbegin(), individualCards.cend());

    QSet<QString> outputs;
    QSet<quintptr> claimed;
    for (const auto &session : sessions) {
        if (outputs.contains(session.output)) {
            violations.push_back({0, OwnershipViolation::Rule::DuplicateBentoLayout,
                                  session.output});
        }
        outputs.insert(session.output);

        for (const auto pane : session.panes) {
            if (pane == 0) continue;
            // A pane that is also a card, or a pane on a second display, has
            // two owners either way.
            if (cards.contains(pane) || claimed.contains(pane)) {
                violations.push_back({pane, OwnershipViolation::Rule::TwoOwners,
                                      session.output});
            }
            claimed.insert(pane);
        }
    }
    return violations;
}

[[nodiscard]] inline CardOwner ownerOf(quintptr window,
    const std::vector<quintptr> &individualCards,
    const std::vector<BentoOwnershipView> &sessions)
{
    if (window == 0) return CardOwner::Native;
    for (const auto &session : sessions) {
        for (const auto pane : session.panes) {
            if (pane == window) return CardOwner::BentoPane;
        }
    }
    for (const auto card : individualCards) {
        if (card == window) return CardOwner::IndividualCard;
    }
    return CardOwner::Native;
}

// The authority for who owns a window. Before this existed, every transition
// kept the card stage's membership and the desktop stage's sessions consistent
// by hand, and §14's first invariant could not be checked anywhere. A stage
// asks the ledger to move a window and the ledger refuses anything that is not
// one of the six directed transitions, so an owner cannot change by accident.
//
// The ledger records ownership. It does not hold windows, geometry or
// presentation, and it never repairs a caller's containers: `reconcile` reports
// where they disagree with it so a pre-existing defect stays visible instead of
// being silently absorbed.
class CardOwnershipLedger
{
public:
    [[nodiscard]] CardOwner ownerOf(quintptr window) const
    {
        const auto entry = m_owners.constFind(window);
        return entry == m_owners.constEnd() ? CardOwner::Native : entry->owner;
    }

    [[nodiscard]] QString outputOf(quintptr window) const
    {
        const auto entry = m_owners.constFind(window);
        return entry == m_owners.constEnd() ? QString() : entry->output;
    }

    // Returns the transition it performed, or nothing if the move is not one of
    // the six. A move to the owner a window already has is not a transition and
    // is refused rather than silently accepted, so a caller cannot use this to
    // paper over a container it failed to update.
    std::optional<OwnershipTransition> transfer(quintptr window, CardOwner to,
                                                const QString &output = {})
    {
        if (window == 0) return std::nullopt;
        const auto transition = directedTransition(ownerOf(window), to);
        if (!transition) return std::nullopt;
        if (to == CardOwner::Native) m_owners.remove(window);
        else m_owners.insert(window, Entry{to, output});
        return transition;
    }

    // A window KWin destroyed leaves ownership entirely; that is not one of the
    // six, because the window no longer exists to own.
    void forget(quintptr window) { m_owners.remove(window); }
    void clear() { m_owners.clear(); }
    [[nodiscard]] int size() const { return m_owners.size(); }

    [[nodiscard]] std::vector<quintptr> windowsOwnedAs(CardOwner owner) const
    {
        std::vector<quintptr> windows;
        for (auto it = m_owners.cbegin(); it != m_owners.cend(); ++it) {
            if (it->owner == owner) windows.push_back(it.key());
        }
        return windows;
    }

    // Where the stages' own containers disagree with the authority. A caller
    // that maintained both by hand can drift; this is how that drift surfaces
    // instead of becoming a second, contradictory answer to who owns a window.
    [[nodiscard]] std::vector<OwnershipViolation> reconcile(
        const std::vector<quintptr> &individualCards,
        const std::vector<BentoOwnershipView> &sessions) const
    {
        auto violations = auditCardOwnership(individualCards, sessions);
        // The member overload shadows the free one inside this class.
        const auto observed = [&](quintptr window) {
            return ::Kadunce::ownerOf(window, individualCards, sessions);
        };
        for (auto it = m_owners.cbegin(); it != m_owners.cend(); ++it) {
            if (observed(it.key()) != it->owner) {
                violations.push_back({it.key(),
                                      OwnershipViolation::Rule::TwoOwners,
                                      it->output});
            }
        }
        for (const auto card : individualCards) {
            if (card != 0 && ownerOf(card) != CardOwner::IndividualCard) {
                violations.push_back({card, OwnershipViolation::Rule::TwoOwners,
                                      QString()});
            }
        }
        for (const auto &session : sessions) {
            for (const auto pane : session.panes) {
                if (pane != 0 && ownerOf(pane) != CardOwner::BentoPane) {
                    violations.push_back({pane, OwnershipViolation::Rule::TwoOwners,
                                          session.output});
                }
            }
        }
        return violations;
    }

private:
    struct Entry {
        CardOwner owner = CardOwner::Native;
        QString output;
    };
    QHash<quintptr, Entry> m_owners;
};

[[nodiscard]] inline QString describeOwnershipViolation(const OwnershipViolation &violation)
{
    switch (violation.rule) {
    case OwnershipViolation::Rule::TwoOwners:
        return QStringLiteral("window %1 has two owners on %2")
            .arg(violation.window).arg(violation.output);
    case OwnershipViolation::Rule::DuplicateBentoLayout:
        return QStringLiteral("display %1 has more than one Bento layout")
            .arg(violation.output);
    }
    return QStringLiteral("unknown ownership violation");
}

} // namespace Kadunce
