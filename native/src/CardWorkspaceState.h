#pragma once
#include "CardLineModel.h"
#include <QList>
#include <optional>
#include <memory>

namespace Kadunce {
// Owns membership, order, stacks and selection as one unit. Handle is an
// identity-bearing adapter value (QPointer in KWin, strings in isolated tests).
// No rendering, timers, native-window operations or output policy lives here.
template<class Handle>
class CardWorkspaceState {
public:
    CardWorkspaceState() = default;
    CardWorkspaceState(const CardWorkspaceState &) = delete;
    CardWorkspaceState &operator=(const CardWorkspaceState &) = delete;
    quint64 revision() const { return m_revision; }
    class PreparedStackInsertion {
        friend class CardWorkspaceState;
        std::weak_ptr<const int> owner;
        quint64 revision = 0;
        CardLineModel model{1};
    };
    std::optional<PreparedStackInsertion> prepareStackInsertion(
        const Handle &destination, int slot) const {
        const int target = indexOf(destination) + 1;
        if (!invariantHolds() || m_windows.isEmpty() || target <= 0
            || !selectedIsStandalone() || target == selectedId()
            || slot < 0 || slot > stackSizeForId(target)) return std::nullopt;
        PreparedStackInsertion result;
        result.owner = m_identity;
        result.revision = m_revision;
        result.model = m_model;
        if (!result.model.stackSelectedWith(target, slot,
                CardLineModel::InsertionSelection::DestinationCard)) return std::nullopt;
        return result;
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
        auto prepared = prepareStackInsertion(destination, slot);
        if (prepared && depth == 0) {
            prepared->model = m_model;
            if (!prepared->model.stackSelectedWith(target, slot,
                    CardLineModel::InsertionSelection::InsertedCard)) return std::nullopt;
        }
        return prepared;
    }
    bool commitStackInsertion(const PreparedStackInsertion &prepared) {
        if (prepared.owner.lock() != m_identity || prepared.revision != m_revision)
            return false;
        m_model = prepared.model;
        ++m_revision;
        return true;
    }
    // A prepared removal is not membership ownership. Dropping this value
    // cancels without touching the source. Only this originating state may
    // commit it, and any intervening state command invalidates it.
    class PreparedRemoval {
        friend class CardWorkspaceState;
        std::weak_ptr<const int> source;
        quint64 revision = 0;
        CardLineModel model{1};
        QList<Handle> windows;
    };
    std::optional<PreparedRemoval> prepareRemoval(const Handle &window) const {
        const int index = indexOf(window);
        if (index < 0 || !invariantHolds()) return std::nullopt;
        PreparedRemoval result;
        result.source = m_identity;
        result.revision = m_revision;
        result.model = m_model;
        result.windows = m_windows;
        result.model.commitDetachedMember();
        if (result.windows.size() > 1 && !result.model.removeCard(index + 1))
            return std::nullopt;
        result.windows.removeAt(index);
        return result;
    }
    bool commitRemoval(const PreparedRemoval &prepared) {
        if (prepared.source.lock() != m_identity || prepared.revision != m_revision)
            return false;
        m_model = prepared.model;
        m_windows = prepared.windows;
        ++m_revision; // Also rejects duplicate commits/copies of the ticket.
        return true;
    }
    // Admission is a value plan, not ownership. It preserves the current
    // grouping and uses the same append operation as ordinary admissions.
    class PreparedAdmission {
        friend class CardWorkspaceState;
        std::weak_ptr<const int> destination;
        quint64 revision = 0;
        CardLineModel model{1};
        QList<Handle> windows;
    };
    std::optional<PreparedAdmission> prepareAdmission(const Handle &window, bool centered) const {
        if (indexOf(window) >= 0 || !invariantHolds() || hasDetachedMember())
            return std::nullopt;
        PreparedAdmission result;
        result.destination = m_identity;
        result.revision = m_revision;
        result.model = m_model;
        result.windows = m_windows;
        appendTo(result.model, result.windows, window, centered);
        return result;
    }
    // Import an output-local composition as one stack. The first identity is
    // its selected face; the remaining identities keep deterministic order.
    std::optional<PreparedAdmission> prepareStackAdmission(const QList<Handle> &windows) const {
        if (!m_windows.isEmpty() || windows.isEmpty()) return std::nullopt;
        PreparedAdmission result;
        result.destination = m_identity;
        result.revision = m_revision;
        for (const auto &window : windows) {
            if (result.windows.contains(window)) return std::nullopt;
            const int id = appendTo(result.model, result.windows, window, false);
            if (id > 1) {
                result.model.selectIndex(result.model.count() - 1);
                if (!result.model.stackSelectedWith(1, -1,
                        CardLineModel::InsertionSelection::DestinationCard)) return std::nullopt;
            }
        }
        return result;
    }
    // Synchronous source-model callback only: no native operations, signals,
    // or destination mutations. Validate destination before touching source,
    // then publish immediately, with no externally observable calls between.
    template<class CommitSource>
    bool commitAdmission(const PreparedAdmission &prepared, CommitSource commitSource) {
        if (prepared.destination.lock() != m_identity || prepared.revision != m_revision)
            return false;
        if (!commitSource()) return false;
        m_model = prepared.model;
        m_windows = prepared.windows;
        ++m_revision;
        return true;
    }
    const CardLineModel &model() const { return m_model; }
    const QList<Handle> &windows() const { return m_windows; }
    int indexOf(const Handle &window) const { return m_windows.indexOf(window); }
    Handle selectedWindow() const { return m_windows.value(m_model.selectedId() - 1); }
    void clear() {
        ++m_revision;
        m_windows.clear();
        // Clearing membership leaves the empty-stage model inert until a fresh
        // admission session explicitly resets it.
    }
    void reset(const QList<Handle> &windows, int selectedIndex) {
        ++m_revision;
        m_windows = windows;
        if (!m_windows.isEmpty()) {
            m_model = CardLineModel(m_windows.size());
            m_model.selectIndex(selectedIndex);
        }
    }
    int append(const Handle &window, bool centered) {
        if (indexOf(window) >= 0) return 0;
        ++m_revision;
        return appendTo(m_model, m_windows, window, centered);
    }
    bool removeAt(int index) {
        if (index < 0 || index >= m_windows.size()) return false;
        if (m_windows.size() > 1 && !m_model.removeCard(index + 1)) return false;
        ++m_revision;
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
    void moveSelected(int delta) { ++m_revision; return m_model.moveSelected(delta); }
    bool stackSelectedWith(int destinationId, int insertionIndex = -1) { ++m_revision; return m_model.stackSelectedWith(destinationId, insertionIndex); }
    bool detachSelectedMember() { ++m_revision; return m_model.detachSelectedMember(); }
    bool restoreDetachedMember() { ++m_revision; return m_model.restoreDetachedMember(); }
    void commitDetachedMember() { ++m_revision; return m_model.commitDetachedMember(); }

private:
    static int appendTo(CardLineModel &model, QList<Handle> &windows,
                        const Handle &window, bool centered) {
        windows.append(window);
        if (windows.size() == 1) {
            model = CardLineModel(1);
            return 1;
        }
        return centered ? model.appendCenteredCard() : model.appendCard();
    }
    const std::shared_ptr<const int> m_identity = std::make_shared<const int>(0);
    quint64 m_revision = 0;
    CardLineModel m_model{1};
    QList<Handle> m_windows;
};
} // namespace Kadunce
