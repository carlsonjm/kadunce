# Known issues

## Active transition motion

Calling an existing card forward from the application dock selects and focuses
the correct real window. The current resize transition does not yet interpolate
from that window's exact prior geometry, so the motion can look disconnected
from its source. This is visual debt reserved for the Card Line flagship pass;
the final window, card identity, and focus state are correct.

## Native plugin installation

The installer currently targets
`/usr/lib/qt6/plugins/kwin/effects/plugins/`, the native KWin effect directory
used by the tested system. Distributions using another library directory must
adapt that destination before installation. The plugin must be rebuilt against
the installed KWin version after an ABI-changing system update.

## Guided compatibility repair

The tray compares the installed plugin factory version with the installed KWin
executable. This on-disk check does not prove a running pre-update compositor can
load it. Explicit repair builds the source snapshot saved at installation, as the
normal user, runs tests, and requests administrator approval for one plugin file.
A backup stays in /var/tmp. No downloads, effect toggles, or session restarts occur.
The emergency switch remains available during the build. Cached plugins may need
a normal logout/login; in-session hot reload is not promised.

Build dependencies and the retained snapshot are required. Future source API
changes may need a maintained Kadunce update, not just a rebuild. No package hooks
or automatic repair are included. Logs: $XDG_STATE_HOME/kadunce/repair.log (default
~/.local/state/kadunce/repair.log). Snapshot checksums detect accidental edits, not
malicious changes by someone controlling the user account.

## Hardware-specific edge input

Kadunce uses native compositor touch edges on ordinary hardware. A supported
posture helper enables the richer direct four-edge backend. Hardware without
that helper retains top/bottom workspace entry but may not expose the same
side-edge paging behavior until its compositor configuration provides it.
