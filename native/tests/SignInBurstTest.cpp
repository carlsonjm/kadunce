/* SPDX-License-Identifier: GPL-2.0-or-later */
// Behavioral coverage of the sign-in burst: which arriving windows wait
// behind the card already shown, and when one of them is shown.
#include "SignInBurst.h"

#include <cstdlib>
#include <iostream>

using Kadunce::SignInBurst;

namespace {
void require(bool value, const char *message)
{
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}
} // namespace

int main()
{
    // Plasma's restore: four apps 0-700 ms after the splash.
    {
        SignInBurst burst;
        require(burst.settlesAt() < 0, "nothing settles before anything arrives");
        require(burst.arrive(4200), "the first restored app starts the burst");
        require(burst.arrive(4400), "the second joins it");
        require(burst.arrive(4650), "the third joins it");
        require(burst.arrive(4900), "the fourth joins it");
        require(burst.count() == 4, "all four are counted");
        require(burst.settlesAt() == 4900 + SignInBurst::QuietMs, "it settles a quiet spell after the last");
        require(burst.open(5000), "it is open until then");
        require(!burst.open(burst.settlesAt()), "and closed from then");
    }
    // An app opened after a quiet spell is the person's own.
    {
        SignInBurst burst;
        require(burst.arrive(3000), "a burst starts");
        require(!burst.arrive(3000 + SignInBurst::QuietMs + 1), "an arrival after the quiet spell is not in it");
        require(burst.over(), "and the burst is over for good");
        require(!burst.arrive(3000 + SignInBurst::QuietMs + 100), "nothing joins it again");
        require(burst.settlesAt() < 0, "an ended burst has nothing to settle");
    }
    // A session already running when the first window comes has no burst.
    {
        SignInBurst burst;
        require(!burst.arrive(SignInBurst::OpeningMs + 1), "a window long after sign-in is shown as it comes");
        require(burst.over(), "and no burst starts later");
    }
    // A steady trickle cannot hold the cards back for ever.
    {
        SignInBurst burst;
        qint64 now = 1000;
        require(burst.arrive(now), "the trickle starts a burst");
        while (now + 1000 <= 1000 + SignInBurst::LongestMs) {
            now += 1000;
            require(burst.arrive(now), "each within the quiet spell joins");
        }
        require(burst.settlesAt() == 1000 + SignInBurst::LongestMs, "it settles at its longest");
        require(!burst.arrive(1000 + SignInBurst::LongestMs + 500), "one past its longest is not in it");
    }
    // Ending it by hand, once shown, keeps later arrivals out.
    {
        SignInBurst burst;
        require(burst.arrive(2000), "a burst starts");
        burst.end();
        require(!burst.arrive(2100), "an arrival after it was shown is the person's");
    }
    return 0;
}
