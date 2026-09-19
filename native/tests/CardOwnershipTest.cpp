/* SPDX-License-Identifier: GPL-2.0-or-later */
// The ownership observer evaluates CARD-LIFECYCLE.md §14 against all three
// owners at once. It is assertion-only: these cases check what it reports, not
// that the reported shape has been repaired.
#include "CardOwnership.h"

#include <cstdlib>
#include <iostream>

using namespace Kadunce;
using namespace Qt::StringLiterals;

namespace {
void require(bool value, const char *message)
{
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}
bool reports(const std::vector<OwnershipViolation> &found, quintptr window,
             OwnershipViolation::Rule rule)
{
    for (const auto &violation : found) {
        if (violation.window == window && violation.rule == rule) return true;
    }
    return false;
}
} // namespace

int main()
{
    const QString tablet = u"tablet"_s;
    const QString monitor = u"monitor"_s;

    // A contract-clean arrangement: panes owned by Bento, everything else an
    // individual card, overflow retained but independently owned.
    {
        const std::vector<quintptr> cards{10, 11, 40};
        const std::vector<BentoOwnershipView> sessions{
            {tablet, {20, 21}, {40}},
        };
        require(auditCardOwnership(cards, sessions).empty(),
            "A contract-clean arrangement reported a violation");
        require(ownerOf(20, cards, sessions) == CardOwner::BentoPane,
            "A visible pane was not owned by Bento");
        require(ownerOf(10, cards, sessions) == CardOwner::IndividualCard,
            "A card was not owned as an individual card");
        require(ownerOf(99, cards, sessions) == CardOwner::Native,
            "An unmanaged window was not Native");
        require(ownerOf(0, cards, sessions) == CardOwner::Native,
            "A null identity was not Native");
    }

    // §14: one window has one owner. The two containers are maintained by hand,
    // so a window can appear in both.
    {
        const std::vector<quintptr> cards{10, 20};
        const std::vector<BentoOwnershipView> sessions{{tablet, {20, 21}, {}}};
        const auto found = auditCardOwnership(cards, sessions);
        require(found.size() == 1 && reports(found, 20, OwnershipViolation::Rule::TwoOwners),
            "A window owned as both a card and a pane was not reported");
        // Ownership resolution prefers the Bento pane, so a caller reading the
        // owner cannot silently see the stale card.
        require(ownerOf(20, cards, sessions) == CardOwner::BentoPane,
            "A doubly-owned window did not resolve to its pane");
    }

    // §14: one window has one owner, across displays too.
    {
        const std::vector<BentoOwnershipView> sessions{
            {tablet, {20}, {}}, {monitor, {20}, {}}};
        const auto found = auditCardOwnership({}, sessions);
        require(found.size() == 1 && reports(found, 20, OwnershipViolation::Rule::TwoOwners),
            "The same window as a pane on two displays was not reported");
    }

    // §14: one display has at most one Bento layout.
    {
        const std::vector<BentoOwnershipView> sessions{
            {tablet, {20}, {}}, {tablet, {21}, {}}};
        const auto found = auditCardOwnership({}, sessions);
        require(found.size() == 1
            && found.front().rule == OwnershipViolation::Rule::DuplicateBentoLayout
            && found.front().output == tablet,
            "Two Bento layouts on one display were not reported");
    }

    // §14: a window outside the visible combination is an individual card.
    // This is the pre-existing defect the audit recorded: the implementation
    // retains overflow beside the session without giving it card ownership.
    {
        const std::vector<quintptr> cards{10};
        const std::vector<BentoOwnershipView> sessions{{tablet, {20}, {41}}};
        const auto found = auditCardOwnership(cards, sessions);
        require(found.size() == 1
            && reports(found, 41, OwnershipViolation::Rule::OverflowWithoutOwner),
            "Retained overflow with no owner was not reported");
        require(ownerOf(41, cards, sessions) == CardOwner::Native,
            "Unowned overflow did not resolve to Native");
    }

    // Overflow that is also a visible pane of the same session is two owners,
    // not merely unowned overflow.
    {
        const std::vector<BentoOwnershipView> sessions{{tablet, {20}, {20}}};
        const auto found = auditCardOwnership({}, sessions);
        require(found.size() == 1 && reports(found, 20, OwnershipViolation::Rule::TwoOwners),
            "A window both visible and overflowed was not reported as two owners");
    }

    // Several independent defects are reported independently rather than
    // collapsing into the first one found.
    {
        const std::vector<quintptr> cards{20, 30};
        const std::vector<BentoOwnershipView> sessions{
            {tablet, {20}, {41}}, {monitor, {30}, {}}};
        const auto found = auditCardOwnership(cards, sessions);
        require(found.size() == 3
            && reports(found, 20, OwnershipViolation::Rule::TwoOwners)
            && reports(found, 41, OwnershipViolation::Rule::OverflowWithoutOwner)
            && reports(found, 30, OwnershipViolation::Rule::TwoOwners),
            "Independent violations were not reported independently");
    }

    // Empty and Native-only arrangements are silent.
    require(auditCardOwnership({}, {}).empty(), "An empty workspace reported a violation");
    require(auditCardOwnership({1, 2, 3}, {}).empty(),
        "Cards without any Bento session reported a violation");
    require(auditCardOwnership({}, {{tablet, {}, {}}}).empty(),
        "An empty session reported a violation");

    // The transition grammar: exactly six, and nothing else is expressible.
    {
        const CardOwner owners[] = {CardOwner::Native, CardOwner::IndividualCard,
                                    CardOwner::BentoPane};
        int directed = 0;
        for (const auto from : owners) {
            for (const auto to : owners) {
                const auto transition = directedTransition(from, to);
                if (from == to) {
                    require(!transition.has_value(),
                        "An owner moving to itself was accepted as a transition");
                    continue;
                }
                require(transition.has_value(), "A directed move had no transition");
                require(transitionFrom(*transition) == from
                    && transitionTo(*transition) == to,
                    "A transition did not round-trip to the pair that named it");
                ++directed;
            }
        }
        require(directed == 6, "The contract defines six directed transitions");
    }

    // The ledger is the authority: a stage asks it to move a window, and it
    // refuses anything that is not one of the six.
    {
        CardOwnershipLedger ledger;
        require(ledger.ownerOf(20) == CardOwner::Native,
            "An unknown window was not Native");
        require(!ledger.transfer(20, CardOwner::Native),
            "Moving a Native window to Native was accepted");
        require(!ledger.transfer(0, CardOwner::IndividualCard),
            "A null identity was admitted");

        auto step = ledger.transfer(20, CardOwner::IndividualCard);
        require(step && *step == OwnershipTransition::AdmitToCard,
            "Admission to a card was not the AdmitToCard transition");
        require(ledger.ownerOf(20) == CardOwner::IndividualCard,
            "The ledger did not record the new owner");
        require(!ledger.transfer(20, CardOwner::IndividualCard),
            "Re-admitting an owned window was accepted as a transition");

        step = ledger.transfer(20, CardOwner::BentoPane, tablet);
        require(step && *step == OwnershipTransition::CardToBento,
            "A card entering Bento was not the CardToBento transition");
        require(ledger.outputOf(20) == tablet, "The ledger lost the pane's display");

        step = ledger.transfer(20, CardOwner::IndividualCard);
        require(step && *step == OwnershipTransition::BentoToCard,
            "A pane leaving Bento was not the BentoToCard transition");
        step = ledger.transfer(20, CardOwner::Native);
        require(step && *step == OwnershipTransition::ReleaseCard,
            "Releasing a card was not the ReleaseCard transition");
        require(ledger.ownerOf(20) == CardOwner::Native && ledger.size() == 0,
            "A released window was still owned");

        require(ledger.transfer(21, CardOwner::BentoPane, tablet).has_value(),
            "Admission straight to Bento was refused");
        step = ledger.transfer(21, CardOwner::Native);
        require(step && *step == OwnershipTransition::ReleaseBento,
            "Releasing a pane was not the ReleaseBento transition");
    }

    // A destroyed window leaves ownership without a transition, because it no
    // longer exists to own.
    {
        CardOwnershipLedger ledger;
        require(ledger.transfer(30, CardOwner::IndividualCard).has_value(), "Admission failed");
        ledger.forget(30);
        require(ledger.ownerOf(30) == CardOwner::Native && ledger.size() == 0,
            "A forgotten window was still owned");
    }

    // Reconciliation reports where a stage's own container disagrees with the
    // authority, rather than silently absorbing the difference.
    {
        CardOwnershipLedger ledger;
        require(ledger.transfer(10, CardOwner::IndividualCard).has_value(), "Admission failed");
        require(ledger.transfer(20, CardOwner::BentoPane, tablet).has_value(), "Admission failed");
        const std::vector<quintptr> cards{10};
        const std::vector<BentoOwnershipView> sessions{{tablet, {20}, {}}};
        require(ledger.reconcile(cards, sessions).empty(),
            "Agreeing containers reported a disagreement");

        // The card stage dropped a window the ledger still owns.
        require(!ledger.reconcile({}, sessions).empty(),
            "A card missing from its stage was not reported");
        // A stage holds a card the ledger never gave it.
        require(!ledger.reconcile({10, 11}, sessions).empty(),
            "An unrecorded card was not reported");
        // A pane the ledger believes is an ordinary card.
        CardOwnershipLedger stale;
        require(stale.transfer(20, CardOwner::IndividualCard).has_value(), "Admission failed");
        require(!stale.reconcile({}, sessions).empty(),
            "A pane the ledger recorded as a card was not reported");
    }

    // The measured live defect: a five-window tablet Bento leaves two panes and
    // three overflow windows that no owner claims. The ledger records what is
    // true rather than pretending the overflow is owned, and reconciliation
    // keeps reporting it until Block 3 removes the retention.
    {
        CardOwnershipLedger ledger;
        const std::vector<quintptr> panes{20, 21};
        const std::vector<quintptr> overflow{41, 42, 43};
        for (const auto pane : panes) {
            require(ledger.transfer(pane, CardOwner::BentoPane, tablet).has_value(),
                "Pane admission failed");
        }
        const std::vector<BentoOwnershipView> sessions{{tablet, panes, overflow}};
        const auto found = ledger.reconcile({}, sessions);
        require(found.size() == 3, "The measured overflow shape changed shape");
        for (const auto window : overflow) {
            require(reports(found, window, OwnershipViolation::Rule::OverflowWithoutOwner),
                "A retained overflow window was not reported as unowned");
            require(ledger.ownerOf(window) == CardOwner::Native,
                "Unowned overflow was recorded as owned");
        }
    }

    // A description exists for every rule, so a report is readable in a log.
    for (const auto rule : {OwnershipViolation::Rule::TwoOwners,
                            OwnershipViolation::Rule::DuplicateBentoLayout,
                            OwnershipViolation::Rule::OverflowWithoutOwner}) {
        require(!describeOwnershipViolation({7, rule, tablet}).isEmpty(),
            "An ownership violation had no description");
    }
}
