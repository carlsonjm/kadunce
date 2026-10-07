/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QDBusMessage>
#include <QObject>
#include <QString>
#include <QVariantList>

#include <functional>
#include <optional>

class QDBusServiceWatcher;

namespace Kadunce
{
// Gooseberry's StuckNotes interface, read from inside the compositor
// (TETTEGOUCHE-CONTEXT.md § Stuck notes from Gooseberry). Gooseberry is found
// at run time and never started: while it is absent there are no entries.
// Every call is asynchronous, and no reply is ever waited for.
class StuckNotesWatcher final : public QObject
{
    Q_OBJECT

public:
    explicit StuckNotesWatcher(QObject *parent = nullptr);

    // Whether Gooseberry has answered since it last appeared.
    [[nodiscard]] bool present() const { return !m_owner.isEmpty(); }
    // While Spread covers the windows, Gooseberry's surface over them steps
    // aside without putting any note away. A build without Pause answers
    // with an error, which nothing waits for.
    void pause(bool paused);
    // Shows a window's notes over it, or puts them away. `answered` takes
    // Gooseberry's reply when it comes: whether they are shown now, or
    // nothing when the call failed or could not be made.
    void toggle(const QString &windowId, const QString &caption, const QString &app,
                std::function<void(std::optional<bool>)> answered);
    // Reads every entry again, in case a signal was missed.
    void refresh() { fetch(); }
    // Sticks a note to another window.
    void stickTo(const QString &noteId, const QString &windowId,
                 const QString &caption, const QString &app);

Q_SIGNALS:
    // Every entry, each a map as the interface describes it; empty when
    // Gooseberry has gone.
    void windowsChanged(const QVariantList &windows);

private Q_SLOTS:
    void receiveWindowsChanged(const QDBusMessage &message);

private:
    void fetch();
    void vanish();
    void send(const QString &method, const QVariantList &arguments);

    QDBusServiceWatcher *m_watcher = nullptr;
    // Gooseberry's unique name on the bus once it has answered; signals from
    // anyone else only prompt a fresh read.
    QString m_owner;
    // Bumped by every read asked for, every signal taken and every departure,
    // so a reply older than any of them is dropped.
    quint64 m_generation = 0;
};
} // namespace Kadunce
