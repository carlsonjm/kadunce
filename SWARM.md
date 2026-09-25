# Swarm runtime coordination

This file contains only current cross-agent runtime coordination. It is not
project history.

## Rules

- At most three live handoffs, each one bullet of at most 50 words.
- Cross-agent communication goes only here; delete a handoff when it is resolved.
- `AGENTS.md` § Where writing goes says where everything else belongs.

## Active handoffs

- Shuffle Keyboard owner: rebuild and bezel either-or passed 24 September. Arrival revisit: room made at the keys' pace and a shorter wait (keyboard e5d0cbd, shuffle c68b414) await J's pass. Concept governs. Branch `shuffle-1.0`.
- Shuffle Keyboard owner: two fixes from J's make-room pass (24 September). Hiding resets `carry` to 0 while mapped, so the panel reports the full keys for one update and the card above shrinks again; `beginArrival` already sets it. And `cardRadius` 18 should match Kadunce's card radius, 10. Branch `shuffle-1.0`.
