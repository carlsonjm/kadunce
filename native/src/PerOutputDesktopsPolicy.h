/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

namespace Kadunce {
// Runtime-only ownership of KWin's per-display desktop switching. With it on,
// each display has its own current desktop and a window moved between
// displays changes desktop, against a workspace that spans every display.
// Held off while this effect is loaded; never changes kwinrc, and unload
// restores the user's preference.
template<typename Options>
class PerOutputDesktopsPolicy {
public:
    explicit PerOutputDesktopsPolicy(Options *options) : m_options(options) { refresh(); }
    PerOutputDesktopsPolicy(const PerOutputDesktopsPolicy &) = delete;
    PerOutputDesktopsPolicy &operator=(const PerOutputDesktopsPolicy &) = delete;
    ~PerOutputDesktopsPolicy() { m_options->setPerOutputVirtualDesktops(m_perOutput); }
    // Called after KWin reloads user configuration: keep the new preference
    // for unload, and keep switching whole while this effect is loaded.
    void refresh() {
        m_perOutput = m_options->isPerOutputVirtualDesktops();
        m_options->setPerOutputVirtualDesktops(false);
    }
private:
    Options *m_options;
    bool m_perOutput = false;
};
}
