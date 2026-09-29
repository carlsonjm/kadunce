#pragma once
#include <QtGlobal>

namespace Kadunce {
// A queued command is valid only within its original workspace generation.
class DeferredCommandGuard {
public:
    quint64 issue() { return ++m_generation; }
    void invalidate() { ++m_generation; }
    bool accepts(quint64 ticket) const { return ticket == m_generation; }
    quint64 generation() const { return m_generation; }
private:
    quint64 m_generation = 0;
};
}
