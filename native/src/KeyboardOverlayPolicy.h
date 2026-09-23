/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

namespace Kadunce {
// Runtime-only ownership of the compositor's virtual-keyboard window lift.
// KWin otherwise pins the window holding text focus to the top of the work
// area and cuts it off at the keyboard, which is a second authority over a
// window this effect owns. Overlaying is KWin's own setting for declining
// that. Never changes kwinrc; unload restores the user's preference.
template<typename Options>
class KeyboardOverlayPolicy {
public:
    explicit KeyboardOverlayPolicy(Options *options) : m_options(options) { refresh(); }
    KeyboardOverlayPolicy(const KeyboardOverlayPolicy &) = delete;
    KeyboardOverlayPolicy &operator=(const KeyboardOverlayPolicy &) = delete;
    ~KeyboardOverlayPolicy() { m_options->setOverlayVirtualKeyboardOnWindows(m_overlay); }
    // Called after KWin reloads user configuration: keep the new preference
    // for unload, and keep the keyboard overlaid while this effect is loaded.
    void refresh() {
        m_overlay = m_options->overlayVirtualKeyboardOnWindows();
        m_options->setOverlayVirtualKeyboardOnWindows(true);
    }
private:
    Options *m_options;
    bool m_overlay = false;
};
}
