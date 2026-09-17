# Stack browse local freeze — September 13, 2026

J: “that was clean and passed. Lets freeze.” No publication requested.
Base:2f63f65 (accepted row motion). Installed SHA-256:
`158820149383349c8fd179b116715577736c01f18d077f67a765eb17c3b7b35e`.

Explicit stack browsing settles in220ms. Up preserves the accepted send-away
accent; down pulls the incoming face downward while the outgoing face follows
ordinary fan interpolation. Bounded rigid accent24px/0.25degree; exact endpoints,
immediate selection and captured interruption. No input/order/ownership changes.
Directional and row motion tests, source/control and live safety checks pass.
J's pass applies to browse animation, not the separate held-card issue below.

Local installer: `../install-kadunce-stack-browse-20260913.sh`.
`--rollback` restores exact accepted row build:
`a422853a94c2b0668e8f96c12aaa3940e0a128e24b680a774385863dcedfe36b`.
Bundle: `../work/kadunce-stack-browse-20260913`; first browse c3911330 preserved.
Unrelated NEXT-ROADMAP.md edits excluded from this freeze.

## Open: held-card paging/insertion order

J reports ordering can still become wrong while paging with a dragged card.
Screenshot `codex-clipboard-78b0924c-dccc-4e82-9d3f-2f1785bf8093.png`
shows a tablet insertion preview labelled “Place in slot 2 of 4”, held Squoosh
and overlapping destination faces; external monitor remains separate.
This is evidence of the reported state, not proof that committed model order
changed. Distinguish model order, active face, paint order and insertion-preview
state before choosing a fix. Next focused reproduction should record identities
before paging, after inward movement/preview, and after release. Preserve both
accepted animation freezes. This bug takes priority over pickup/release polish.
