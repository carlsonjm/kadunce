# Known limits

What Kadunce does not do yet, and what it will not do.

- A display change is answered once KWin has moved windows for it. An
  ordinary window sent to another display some other way, such as KWin's own
  window-to-screen shortcut, is answered as one that opened there. The Active
  card sent that way stops being a card and stays where KWin put it, outside
  that display's layout; a pane sent that way is not answered.
- Whether switching Kadunce off returns a window to the right place after its
  display has moved in the desktop layout is not measured.
- A Wayland dialog that names no parent window becomes a card. No application
  surveyed so far does this.
- The plugin is built for the KWin it is installed against, and KWin loads it
  only for that exact KWin version, so it needs a rebuild after every KWin
  update. A rebuilt plugin may need a logout and login before the compositor
  loads it.
- Guided repair rebuilds the source kept at installation, runs its tests and
  asks for authorization to replace one plugin file, through Shuffle's install
  key or the system's own `/usr/bin/install`, never a script from the home
  folder. It downloads nothing and
  needs the build dependencies. Its log is
  `$XDG_STATE_HOME/kadunce/repair.log`, normally
  `~/.local/state/kadunce/repair.log`. Its checksums catch accidental edits, not
  someone who already controls the account.
