/* SPDX-License-Identifier: GPL-2.0-or-later */
// Behavioral coverage of the ownership handoff sequences. Each case drives the
// real sequence with recording steps and asserts the observable order, the work
// a refusal leaves undone, and the state a refusal leaves unchanged. Nothing
// here inspects the shape of the calling implementation.
#include "OwnershipHandoff.h"
#include "CardWorkspaceState.h"

#include <QString>
#include <QStringList>
#include <cstdlib>
#include <iostream>

using namespace Kadunce;
using namespace Qt::StringLiterals;

namespace {
void require(bool value, const char *message)
{
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}

struct Output {};

// Stands in for the native client. Every call is recorded in the order the
// sequence issues it, so the assertions read as the sequence a window observes.
struct Client {
    QStringList *calls;
    void sendToOutput(Output *) { *calls << u"output"_s; }
    void moveResize(const QString &geometry) { *calls << u"geometry:"_s + geometry; }
};
} // namespace

int main()
{
    Output output;

    // Publication before the records that describe the new owner.
    {
        QStringList calls;
        require(publishOwnershipThenRecord(
                    [&] { calls << u"publish"_s; return true; },
                    [&] { calls << u"record"_s; }),
            "Accepted publication reported failure");
        require(calls == QStringList({u"publish"_s, u"record"_s}),
            "Provenance was recorded before the owner was published");
    }
    {
        QStringList calls;
        require(!publishOwnershipThenRecord(
                    [&] { calls << u"publish"_s; return false; },
                    [&] { calls << u"record"_s; }),
            "Refused publication reported success");
        require(calls == QStringList({u"publish"_s}),
            "Refused publication still recorded provenance");
    }

    // Against the real membership owner: a refused publication leaves it
    // untouched, and the records only ever see a published owner.
    {
        CardWorkspaceState<QString> destination;
        destination.reset({u"a"_s, u"b"_s}, 0);
        const auto members = destination.windows();
        const auto admission = destination.prepareAdmission(u"incoming"_s, false);
        bool recorded = false;
        require(admission && !publishOwnershipThenRecord(
                    [&] { return destination.commitAdmission(*admission, [] { return false; }); },
                    [&] { recorded = true; }),
            "Source refusal published the destination");
        require(!recorded && destination.windows() == members,
            "Source refusal recorded provenance or changed membership");
        require(publishOwnershipThenRecord(
                    [&] { return destination.commitAdmission(*admission, [] { return true; }); },
                    [&] {
                        recorded = true;
                        require(destination.indexOf(u"incoming"_s) >= 0,
                            "Provenance was recorded before publication");
                    }),
            "Accepted admission reported failure");
        require(recorded, "Accepted admission recorded no provenance");
    }

    // Native adoption: capture sits between the output change and Kadunce's
    // own geometry, so the restore record describes the accepted placement.
    {
        QStringList calls;
        Client client{&calls};
        require(adoptPublishedCard(&client, &output, [] { return true; },
                    [&] { calls << u"capture"_s; }, u"active"_s, [] { return true; }),
            "Valid adoption of a published card was refused");
        require(calls == QStringList({u"output"_s, u"capture"_s, u"geometry:active"_s}),
            "Adoption did not capture between output placement and geometry");
    }
    // Publication gates adoption. An unpublished window has no owner to restore
    // it, so it is never placed, captured or resized.
    {
        QStringList calls;
        Client client{&calls};
        require(!adoptPublishedCard(&client, &output, [] { return false; },
                    [&] { calls << u"capture"_s; }, u"active"_s, [] { return true; }),
            "Unpublished window was adopted");
        require(calls.isEmpty(),
            "An unpublished window was placed, captured or resized");
    }
    // Any step may synchronously close the client, remove the output or disable
    // the effect. The sequence stops at the first invalidation, so no later step
    // acts on a stale owner, and it never reports success afterwards.
    for (int survives = 0; survives <= 2; ++survives) {
        QStringList calls;
        Client client{&calls};
        const auto valid = [&] { return calls.size() <= survives; };
        require(!adoptPublishedCard(&client, &output, [] { return true; },
                    [&] { calls << u"capture"_s; }, u"active"_s, valid),
            "Adoption reported success after invalidation");
        require(calls.size() == survives + 1,
            "Adoption did not stop at the first invalidated step");
    }

    // Resume handback: acceptance decides, the commit runs before anything
    // settles, and a refusal leaves both sides untouched.
    {
        QStringList calls;
        require(commitResumeHandback(
                    [&] { calls << u"accept"_s; return true; },
                    [&] { calls << u"commit"_s; },
                    [&] { calls << u"settle"_s; }),
            "Accepted handback reported failure");
        require(calls == QStringList({u"accept"_s, u"commit"_s, u"settle"_s}),
            "Handback settled before it committed");
    }
    {
        QStringList calls;
        require(!commitResumeHandback(
                    [&] { calls << u"accept"_s; return false; },
                    [&] { calls << u"commit"_s; },
                    [&] { calls << u"settle"_s; }),
            "Refused handback reported success");
        require(calls == QStringList({u"accept"_s}),
            "Refused handback committed or settled");
    }

    // The two sides composed as the resume actually composes them: the
    // releasing side retires its projection inside the commit it reports, so
    // the resuming side never publishes while live projection state exists,
    // and native restoration never runs before the session is published.
    {
        QStringList calls;
        bool projectionLive = true;
        bool sessionPublished = false;
        require(commitResumeHandback(
                    [&] {
                        return commitResumeHandback(
                            [&] { calls << u"release"_s; return true; },
                            [&] { calls << u"retire"_s; projectionLive = false; },
                            [] {});
                    },
                    [&] {
                        calls << u"publish"_s;
                        require(!projectionLive,
                            "Session was published while the projection was still live");
                        sessionPublished = true;
                    },
                    [&] {
                        calls << u"restore"_s;
                        require(sessionPublished,
                            "Native restoration ran before the session was published");
                    }),
            "Accepted resume reported failure");
        require(calls == QStringList({u"release"_s, u"retire"_s, u"publish"_s, u"restore"_s}),
            "Resume handback ran its two sides out of order");
    }
    // A releasing side that refuses leaves the resuming side unpublished and
    // its projection still live, so the card stage keeps presenting it.
    {
        QStringList calls;
        bool projectionLive = true;
        require(!commitResumeHandback(
                    [&] {
                        return commitResumeHandback(
                            [&] { calls << u"release"_s; return false; },
                            [&] { calls << u"retire"_s; projectionLive = false; },
                            [] {});
                    },
                    [&] { calls << u"publish"_s; },
                    [&] { calls << u"restore"_s; }),
            "Refused release reported a resumed session");
        require(projectionLive && calls == QStringList({u"release"_s}),
            "Refused release retired the projection or published a session");
    }
}
