# Mandatory safety control

Kadunce's persistent tray enable/disable switch is a release-blocking requirement,
including when the workspace effect is disabled or incompatible. Never remove it
or make it optional. Do not mark an install/update ready based on effect tests alone.

For Kadunce updates, run `bash tests/verify-control.sh` and, in the user's graphical
session after installation, `bash tests/verify-live-control.sh`. Check session
startup wiring too: the control must be wanted by `graphical-session.target`, not
only `default.target`, so logout/login under a surviving user manager restarts it.
Do not log out the user, stop the graphical session, or toggle the effect as a test
without permission. Ask for visual confirmation when needed; report any safety
control failure as blocking further feature testing.
