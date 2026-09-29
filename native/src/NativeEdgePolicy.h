/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

namespace Kadunce {
// Runtime-only ownership of automatic edge placement. Never changes kwinrc,
// screen-edge gestures, manual maximize, or window-to-window magnetism.
template<typename Options>
class NativeEdgePolicy {
public:
    explicit NativeEdgePolicy(Options *options) : m_options(options) { refresh(); }
    NativeEdgePolicy(const NativeEdgePolicy &) = delete;
    NativeEdgePolicy &operator=(const NativeEdgePolicy &) = delete;
    ~NativeEdgePolicy() {
        m_options->setElectricBorderTiling(m_tiling);
        m_options->setElectricBorderMaximize(m_maximize);
    }
    // Called after KWin reloads user configuration: preserve the new preference
    // for disable/unload, while keeping the active workspace's single owner.
    void refresh() {
        m_tiling = m_options->electricBorderTiling();
        m_maximize = m_options->electricBorderMaximize();
        m_options->setElectricBorderTiling(false);
        m_options->setElectricBorderMaximize(false);
    }
private:
    Options *m_options;
    bool m_tiling = false;
    bool m_maximize = false;
};
}
