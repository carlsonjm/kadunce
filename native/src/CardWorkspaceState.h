#pragma once
#include "CardOwnership.h"
#include "SpreadModel.h"
#include <QList>
#include <optional>
#include <memory>

namespace Kadunce {
// Owns membership, order, stacks and selection as one unit. Handle is an
// identity-bearing adapter value (QPointer in KWin, strings in isolated tests).
// No rendering, timers, native-window operations or output policy lives here.
//
// A prepared transaction carries the membership, order and grouping delta it
// intends and never a copy of the model. Preparation proves the delta against a
// throwaway model and keeps only the intent; commit re-derives the result from
// the live model. A ticket therefore cannot carry selection, page offset, stack
// face or neighbour side back into the state that issued it.
template<class Handle>
class CardWorkspaceState {
public:
    CardWorkspaceState() = default;
    CardWorkspaceState(const CardWorkspaceState &) = delete;
    CardWorkspaceState &operator=(const CardWorkspaceState &) = delete;
    // Two revisions, because a ticket and a preview do not fear the same change.
    // `revision()` counts every state command and keeps its existing meaning for
    // preview and carry-provenance owners. `ownershipRevision()` counts only
    // membership, order and grouping, so a transaction bound to it survives
    // ordinary paging and selection while a membership change still voids it.
    quint64 revision() const { return m_revision; }
    quint64 ownershipRevision() const { return m_ownershipRevision; }
    // Not an ownership transition: a card joining a stack stays an individual
    // card, so this ticket names no transition.
    class PreparedStackInsertion {
        friend class CardWorkspaceState;
        std::weak_ptr<const int> owner;
        quint64 revision = 0;
        Handle destination{};
        int slot = 0;
        SpreadModel::InsertionSelection selection =
            SpreadModel::InsertionSelection::DestinationCard;
    };
    std::optional<PreparedStackInsertion> prepareStackInsertion(
        const Handle &destination, int slot) const {
        return prepareInsertion(destination, slot,
            SpreadModel::InsertionSelection::DestinationCard);
    }
    // Visual depth is front-first (0 = front), unlike the cyclic storage vector.
    // Choosing the front explicitly selects the newcomer; deeper slots retain
    // the old face. Preview and commit must use this same conversion.
    std::optional<PreparedStackInsertion> prepareStackInsertionAtDepth(
        const Handle &destination, int depth) const {
        const int target = indexOf(destination) + 1;
        const int count = stackSizeForId(target);
        if (target <= 0 || count <= 0 || depth < 0 || depth > count)
            return std::nullopt;
        const int active = m_model.stackActivePositionForId(target);
        const int slot = depth == 0 ? active + 1
            : (active - depth + 1 + count) % count;
        return prepareInsertion(destination, slot,
            depth == 0 ? SpreadModel::InsertionSelection::InsertedCard
                       : SpreadModel::InsertionSelection::DestinationCard);
    }
    bool commitStackInsertion(const PreparedStackInsertion &prepared) {
        if (prepared.owner.lock() != m_identity || prepared.revision != m_revision)
            return false;
        SpreadModel model = m_model;
        if (!applyInsertion(model, prepared.destination, prepared.slot,
                prepared.selection)) return false;
        m_model = std::move(model);
        ++m_revision;
        ++m_ownershipRevision;
        return true;
    }
    // A prepared removal is not membership ownership. Dropping this value
    // cancels without touching the source. Only this originating state may
    // commit it, and any intervening state command invalidates it.
    class PreparedRemoval {
        friend class CardWorkspaceState;
        std::weak_ptr<const int> source;
        quint64 ownershipRevision = 0;
        Handle window{};
        OwnershipTransition step = OwnershipTransition::ReleaseCard;
    public:
        // Which of the six this ticket performs, so the owner a commit produces
        // is stated by the ticket rather than inferred afterwards.
        [[nodiscard]] OwnershipTransition transition() const { return step; }
    };
    std::optional<PreparedRemoval> prepareRemoval(const Handle &window,
        OwnershipTransition step = OwnershipTransition::ReleaseCard) const {
        if (indexOf(window) < 0 || !invariantHolds()) return std::nullopt;
        SpreadModel model = m_model;
        QList<Handle> windows = m_windows;
        if (!applyRemoval(model, windows, window)) return std::nullopt;
        PreparedRemoval result;
        result.source = m_identity;
        result.ownershipRevision = m_ownershipRevision;
        result.window = window;
        result.step = step;
        return result;
    }
    bool commitRemoval(const PreparedRemoval &prepared) {
        if (prepared.source.lock() != m_identity
            || prepared.ownershipRevision != m_ownershipRevision) return false;
        SpreadModel model = m_model;
        QList<Handle> windows = m_windows;
        if (!applyRemoval(model, windows, prepared.window)) return false;
        m_model = std::move(model);
        m_windows = std::move(windows);
        ++m_revision;
        ++m_ownershipRevision; // Also rejects duplicate commits/copies of the ticket.
        return true;
    }
    // Admission is a value plan, not ownership. It preserves the current
    // grouping and uses the same append operation as ordinary admissions.
    class PreparedAdmission {
        friend class CardWorkspaceState;
        std::weak_ptr<const int> destination;
        quint64 ownershipRevision = 0;
        QList<Handle> incoming;
        bool centered = false;
        bool stack = false;
        OwnershipTransition step = OwnershipTransition::AdmitToCard;
    public:
        [[nodiscard]] OwnershipTransition transition() const { return step; }
    };
    std::optional<PreparedAdmission> prepareAdmission(const Handle &window, bool centered,
        OwnershipTransition step = OwnershipTransition::AdmitToCard) const {
        if (indexOf(window) >= 0 || !invariantHolds() || hasDetachedMember())
            return std::nullopt;
        PreparedAdmission result;
        result.destination = m_identity;
        result.ownershipRevision = m_ownershipRevision;
        result.incoming = {window};
        result.centered = centered;
        result.step = step;
        SpreadModel model = m_model;
        QList<Handle> windows = m_windows;
        if (!applyAdmission(result, model, windows)) return std::nullopt;
        return result;
    }
    // Import an output-local composition as one stack. The first identity is
    // its selected face; the remaining identities keep deterministic order.
    std::optional<PreparedAdmission> prepareStackAdmission(const QList<Handle> &windows,
        OwnershipTransition step = OwnershipTransition::BentoToCard) const {
        if (!m_windows.isEmpty() || windows.isEmpty()) return std::nullopt;
        PreparedAdmission result;
        result.destination = m_identity;
        result.ownershipRevision = m_ownershipRevision;
        result.incoming = windows;
        result.stack = true;
        result.step = step;
        SpreadModel model = m_model;
        QList<Handle> planned = m_windows;
        if (!applyAdmission(result, model, planned)) return std::nullopt;
        return result;
    }
    // Synchronous source-model callback only: no native operations, signals,
    // or destination mutations. Validate destination before touching source,
    // then publish immediately, with no externally observable calls between.
    template<class CommitSource>
    bool commitAdmission(const PreparedAdmission &prepared, CommitSource commitSource) {
        if (prepared.destination.lock() != m_identity
            || prepared.ownershipRevision != m_ownershipRevision) return false;
        SpreadModel model = m_model;
        QList<Handle> windows = m_windows;
        if (!applyAdmission(prepared, model, windows)) return false;
        if (!commitSource()) return false;
        m_model = std::move(model);
        m_windows = std::move(windows);
        ++m_revision;
        ++m_ownershipRevision;
        return true;
    }
    const SpreadModel &model() const { return m_model; }
    const QList<Handle> &windows() const { return m_windows; }
    int indexOf(const Handle &window) const { return m_windows.indexOf(window); }
    Handle selectedWindow() const { return m_windows.value(m_model.selectedId() - 1); }
    void clear() {
        ++m_revision;
        ++m_ownershipRevision;
        m_windows.clear();
        // Clearing membership leaves the empty-stage model inert until a fresh
        // admission session explicitly resets it.
    }
    void reset(const QList<Handle> &windows, int selectedIndex) {
        ++m_revision;
        ++m_ownershipRevision;
        m_windows = windows;
        if (!m_windows.isEmpty()) {
            m_model = SpreadModel(m_windows.size());
            m_model.selectIndex(selectedIndex);
        }
    }
    int append(const Handle &window, bool centered) {
        if (indexOf(window) >= 0) return 0;
        ++m_revision;
        ++m_ownershipRevision;
        return appendTo(m_model, m_windows, window, centered);
    }
    bool removeAt(int index) {
        if (index < 0 || index >= m_windows.size()) return false;
        if (m_windows.size() > 1 && !m_model.removeCard(index + 1)) return false;
        ++m_revision;
        ++m_ownershipRevision;
        m_windows.removeAt(index);
        return true;
    }
    bool invariantHolds() const {
        return m_windows.isEmpty()
            || (m_model.invariantHolds() && m_model.cardCount() == m_windows.size());
    }
    int count() const { return m_model.count(); }
    int cardCount() const { return m_model.cardCount(); }
    int selectedIndex() const { return m_model.selectedIndex(); }
    int selectedId() const { return m_model.selectedId(); }
    int pairNeighborSide() const { return m_model.pairNeighborSide(); }
    int idAtOffset(int offset) const { return m_model.idAtOffset(offset); }
    std::array<int, 3> visibleNeighborhood() const { return m_model.visibleNeighborhood(); }
    std::array<int, 3> detachedNeighborhood(int pageOffset) const { return m_model.detachedNeighborhood(pageOffset); }
    bool sameStack(int firstId, int secondId) const { return m_model.sameStack(firstId, secondId); }
    int stackSizeForId(int cardId) const { return m_model.stackSizeForId(cardId); }
    int stackPositionForId(int cardId) const { return m_model.stackPositionForId(cardId); }
    int stackActivePositionForId(int cardId) const { return m_model.stackActivePositionForId(cardId); }
    std::vector<int> stackMembersForId(int cardId) const { return m_model.stackMembersForId(cardId); }
    std::vector<int> stackPaintOrderForId(int cardId) const { return m_model.stackPaintOrderForId(cardId); }
    bool selectedIsStandalone() const { return m_model.selectedIsStandalone(); }
    bool hasDetachedMember() const { return m_model.hasDetachedMember(); }
    void setPairNeighborSide(int side) { ++m_revision; return m_model.setPairNeighborSide(side); }
    void page(int delta) { ++m_revision; return m_model.page(delta); }
    void pageStack(int delta) { ++m_revision; return m_model.pageStack(delta); }
    void selectIndex(int index) { ++m_revision; return m_model.selectIndex(index); }
    void moveSelected(int delta) { ++m_revision; ++m_ownershipRevision; return m_model.moveSelected(delta); }
    bool stackSelectedWith(int destinationId, int insertionIndex = -1) { ++m_revision; ++m_ownershipRevision; return m_model.stackSelectedWith(destinationId, insertionIndex); }
    bool detachSelectedMember() { ++m_revision; ++m_ownershipRevision; return m_model.detachSelectedMember(); }
    bool restoreDetachedMember() { ++m_revision; ++m_ownershipRevision; return m_model.restoreDetachedMember(); }
    void commitDetachedMember() { ++m_revision; ++m_ownershipRevision; return m_model.commitDetachedMember(); }

private:
    static int appendTo(SpreadModel &model, QList<Handle> &windows,
                        const Handle &window, bool centered) {
        windows.append(window);
        if (windows.size() == 1) {
            model = SpreadModel(1);
            return 1;
        }
        return centered ? model.appendCenteredCard() : model.appendCard();
    }
    // The three deltas below are the only places a prepared transaction is
    // realized. Preparation runs them against a throwaway model to prove the
    // intent; commit runs them again against the live one. Both callers share
    // this code so a ticket cannot mean one thing when prepared and another
    // when committed.
    std::optional<PreparedStackInsertion> prepareInsertion(
        const Handle &destination, int slot,
        SpreadModel::InsertionSelection selection) const {
        const int target = indexOf(destination) + 1;
        if (!invariantHolds() || m_windows.isEmpty() || target <= 0
            || !selectedIsStandalone() || target == selectedId()
            || slot < 0 || slot > stackSizeForId(target)) return std::nullopt;
        SpreadModel model = m_model;
        if (!applyInsertion(model, destination, slot, selection))
            return std::nullopt;
        PreparedStackInsertion result;
        result.owner = m_identity;
        result.revision = m_revision;
        result.destination = destination;
        result.slot = slot;
        result.selection = selection;
        return result;
    }
    bool applyInsertion(SpreadModel &model, const Handle &destination, int slot,
                        SpreadModel::InsertionSelection selection) const {
        const int target = indexOf(destination) + 1;
        return target > 0 && model.stackSelectedWith(target, slot, selection);
    }
    bool applyRemoval(SpreadModel &model, QList<Handle> &windows,
                      const Handle &window) const {
        const int index = windows.indexOf(window);
        if (index < 0) return false;
        model.commitDetachedMember();
        if (windows.size() > 1 && !model.removeCard(index + 1)) return false;
        windows.removeAt(index);
        return true;
    }
    bool applyAdmission(const PreparedAdmission &prepared, SpreadModel &model,
                        QList<Handle> &windows) const {
        if (prepared.incoming.isEmpty()) return false;
        if (!prepared.stack) {
            const Handle &window = prepared.incoming.first();
            if (windows.contains(window)) return false;
            appendTo(model, windows, window, prepared.centered);
            return true;
        }
        if (!windows.isEmpty()) return false;
        for (const auto &window : prepared.incoming) {
            if (windows.contains(window)) return false;
            const int id = appendTo(model, windows, window, false);
            if (id > 1) {
                model.selectIndex(model.count() - 1);
                if (!model.stackSelectedWith(1, -1,
                        SpreadModel::InsertionSelection::DestinationCard)) return false;
            }
        }
        return true;
    }
    const std::shared_ptr<const int> m_identity = std::make_shared<const int>(0);
    quint64 m_revision = 0;
    quint64 m_ownershipRevision = 0;
    SpreadModel m_model{1};
    QList<Handle> m_windows;
};
} // namespace Kadunce
