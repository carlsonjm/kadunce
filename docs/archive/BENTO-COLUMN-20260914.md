# Occupied column + quiet rail candidate

## Follow-up: full-divider target

J passed column sizing on installedcf5011, reported off-center holds entering
Plasma edit mode, and requested90ms. New candidate302b1cedf851c4015f171311cd7e6e5cfc44a2d042d16923af9c34bbdf00b907
captures the full shared divider with32px cross-axis tolerance. Centered glyph
remains hidden until90ms; no global Plasma setting is changed.
Installer ../install-kadunce-bento-fullrail-20260914.sh; exact rollbackcf5011 below.
Build/control and live safety/hash preflight pass. Private NYkGoz passes off-center
horizontal touch, early-movement rejection and split preview/commit; l96RD3 passes
off-center vertical mouse/touch, cancel, overflow and held unload. Both logs are
under /tmp/kadunce-unload-test.<id>/session.log. Real Plasma long-hold acceptance
remains J's check; private tests do not launch Plasma shell. Not installed/pushed.

## Preceding column candidate

J accepted installed rail cd34ba in both axes. Candidate SHA256:
cf5011ede1b72da74d754347726c06b7a8de45357524990746e5a4879195b35f.
Exact rollback: cd34ba2c1d5b5f062ee8679dc060e987e30abb899cdc5128cbabd62e72e31a97.
Installer: ../install-kadunce-bento-column-20260914.sh.
Not installed/pushed. Local main freeze remains0184129; rail/column changes pending.

Occupied full-height side columns split top/bottom before whole-output replanning.
Tablet applies this to small columns/lower-edge drops; monitor also supports large
columns and upper insertion. Width/opposite pane remain; vacated third full-height
column is absorbed by its non-target neighbor. Minimum sizes include physical gaps.
Impossible splits use existing fit/overflow fallback. This is bounded column logic,
not recursive arbitrary subdivision or persistent layout storage.

Pills hide when idle. Hold divider180ms within12px to reveal, then drag preview and
release to commit. Quick movement does not resize. Both pointer/touch share timing.

Evidence: build and bento-layout/bento-transfer tests pass; verify-control passes.
Private /tmp/kadunce-unload-test.ZSD6BW/session.log passes occupied split matching
preview, quick movement rejection, horizontal touch resizing. Private mWi2jz
passes existing side/share/overflow, pointer/touch rail cancellation and unload
while held. jTTwvo did not execute due to missing executable bit on the new script;
fixed test file mode, not production code. Live installer --check verifies safety
switch and exact candidate/rollback hashes without installation.

J acceptance: with small side pane occupied, drag another card to its lower side
edge; verify column splits rather than making three columns. Hold hidden divider,
resize top/bottom, release; rail disappears. Monitor hardware test remains deferred.
