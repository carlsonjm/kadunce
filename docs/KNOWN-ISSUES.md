# Known issues

## Bento-derived Card Line presentation

Bento-owned windows keep correct ownership, order, and large-pane selection when the
tablet returns to Card Line. The current candidate removes the oversized fixed
backing and gives every member of an open projected stack one proportional common
height derived from the widest member aspect. The canonical slot continues to own
layout and input. Mixed-aspect geometry, fan-envelope containment, and private
ownership lifecycle are automated; physical appearance review remains open.
Common height does not guarantee that an extremely narrow rear member protrudes
past a much wider active face under the existing fan offset. Physical review must
include that ordering before this candidate is accepted.

A client that renders black inside its own surface will retain it. A genuinely
narrow pane also remains narrow because Kadunce does not crop, stretch, natively
resize, or retain a second rendered source for Card Line.

## Card manipulation finish

Stack extraction, reorder intent zones, centered labels, and stack-position labeling
remain incomplete. Native-to-stack arrival is not yet one atomic membership and
insertion transaction.

## Motion accessibility

Some compositor-owned durations do not yet follow the platform animation scale or
reduced-motion preference. Preserve causal feedback and final geometry while removing
unnecessary travel under reduced motion.

## Native plugin installation

The installer targets the native KWin effect directory used by the supported system.
Distributions with another Qt/KWin library directory must adapt the destination. The
plugin must be rebuilt against the installed KWin version after an ABI change.

The tray's on-disk factory-version check cannot prove that an already running,
pre-upgrade compositor can load the rebuilt plugin. Cached plugins may require a
normal logout/login. In-session hot reload is not promised.

## Guided compatibility repair

Repair builds the source snapshot retained at installation as the normal user, runs
tests, and requests authorization for one plugin file. It performs no download,
effect toggle, or session restart. Build dependencies and the retained snapshot are
required; a future source incompatibility may need a maintained Kadunce update.

The repair log is `$XDG_STATE_HOME/kadunce/repair.log` or
`~/.local/state/kadunce/repair.log`. Snapshot checksums detect accidental edits, not
changes by an actor who already controls the user account.

## Hardware-specific edge input

Ordinary hardware uses native compositor touch edges. A supported posture helper
enables richer direct four-edge behavior. Without it, top/bottom workspace entry is
available, while side-edge paging depends on compositor/hardware support.
