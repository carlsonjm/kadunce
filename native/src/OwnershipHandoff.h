/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

namespace Kadunce {
// Ownership changes in one published step. These sequences state the order of
// that step against the work around it, so a caller cannot express the wrong
// order rather than merely avoiding it. They hold no state and make no native
// call of their own; every effect belongs to a caller-supplied step.

// Publication is the only step that changes an owner. Anything that describes
// the new owner - restore records, projection provenance, stacking provenance -
// is written after it, never before: a record written first describes an owner
// that does not exist yet, and a refused publication would leave it behind.
template<class Publish, class Record>
bool publishOwnershipThenRecord(Publish publish, Record record)
{
    if (!publish()) return false;
    record();
    return true;
}

// Native adoption of a card the destination already owns. Publication is a
// precondition rather than an earlier statement: an unpublished window has no
// owner to restore it, so placing and resizing it would leave a window that
// release cannot return. Capture sits between the output change and Kadunce's
// own geometry because a restore record describes KWin's accepted placement on
// the destination output, not the presentation Kadunce is about to apply.
// Every step may synchronously close the client, remove the output or disable
// the effect, so the sequence stops at the first invalidation and reports it.
template<class Client, class Output, class Published, class Capture,
         class Geometry, class Valid>
bool adoptPublishedCard(Client *client, Output *output, Published published,
                        Capture capture, const Geometry &geometry, Valid valid)
{
    if (!published()) return false;
    const auto step = [&](auto action) {
        if (!valid()) return false;
        action();
        return valid();
    };
    if (!step([&] { client->sendToOutput(output); })) return false;
    if (!step([&] { capture(); })) return false;
    return step([&] { client->moveResize(geometry); });
}

// Both ends of a resume handback. `accept` is the last point at which the
// transaction can be refused, so nothing before it may be deferred past it and
// nothing after it may be skipped. On the releasing side `commit` retires the
// projection presentation, so a resumed session can never be observed while
// live projection state still exists. On the resuming side `commit` publishes
// the session and `settle` runs the releasing side's native restoration, which
// has no session to restore into until publication has happened.
template<class Accept, class Commit, class Settle>
bool commitResumeHandback(Accept accept, Commit commit, Settle settle)
{
    if (!accept()) return false;
    commit();
    settle();
    return true;
}
} // namespace Kadunce
