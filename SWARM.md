# Swarm runtime coordination

This file contains only current cross-agent runtime coordination. It is not
project history.

## Rules

- Keep at most three live handoffs.
- Keep each handoff to at most 50 words.
- Put cross-agent communication only in this file.
- Delete completed handoffs; do not archive them here.
- Record durable architectural and engineering decisions in the canonical
  architecture or decision documents.
- Let Git history record implementation changes.
- Use code comments only for code behavior and reasoning, never agent conversation,
  handoffs, product-management instructions, implementation history, or authorship.

## Active handoffs

- Shuffle Keyboard owner: next is the 22 September rebuild (Block 9): side gutter, top grab retired. Arrival motion with the dock follows it (Block 5). Ask J how the keys are put away. `SHUFFLE-KEYBOARD-1.0-CONCEPT.md` governs. Branch `shuffle-1.0`; build tests fresh, `build/` points at Itasca.
