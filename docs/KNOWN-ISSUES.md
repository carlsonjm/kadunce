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

## Hardware-specific edge input

Kadunce uses native compositor touch edges on ordinary hardware. A supported
posture helper enables the richer direct four-edge backend. Hardware without
that helper retains top/bottom workspace entry but may not expose the same
side-edge paging behavior until its compositor configuration provides it.
