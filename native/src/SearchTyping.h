/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QString>

class QDBusServiceWatcher;

namespace Kadunce
{
// Typing in Spread opens Tettegouche's Search there with what was typed
// (TETTEGOUCHE-CONTEXT.md § Typing in Spread). Tettegouche is found at run
// time and never started: while it is absent, Spread takes no typing. Every
// call is asynchronous, and no reply is ever waited for.
class SearchTyping final : public QObject
{
    Q_OBJECT

public:
    explicit SearchTyping(QObject *parent = nullptr);

    // Tettegouche is on the session bus.
    [[nodiscard]] bool present() const { return m_present; }
    // Text was sent a moment ago and Search may not have the keys yet: what
    // follows is sent after it rather than lost to the card behind Spread.
    [[nodiscard]] bool carrying() const;
    // Sends the text for Search's field, opening Search ready to type.
    void type(const QString &text);
    // Search has the keys, so it is typed into directly.
    void settle() { m_sent.invalidate(); }

private:
    void setPresent(bool present);

    QDBusServiceWatcher *m_watcher = nullptr;
    bool m_present = false;
    // Since the text of the typing now under way was first sent.
    QElapsedTimer m_sent;
    // A Tettegouche without typeToSearch was asked to open ready to type.
    bool m_openedPlain = false;
};
} // namespace Kadunce
