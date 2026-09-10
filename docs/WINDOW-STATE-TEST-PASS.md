# Window-state repair: first test pass

Scope: preserve floating and fullscreen restore rectangles; stop recursive Active
geometry correction; explicit move/resize/maximize/fullscreen/quick-tile requests
release Card Stage presentation without restoring over the new request. The effect
stays enabled. Card Line geometry and the 6–48 px gutter range are unchanged.

Kadunce-owned entry/restoration mutations are guarded. Asynchronous geometry
notifications may schedule at most two deferred Active-size corrections per entry,
never recursive writes or cancellation of manual moves/resizes. Restoring or
releasing the window cancels pending correction. Verify small-window previews
after entering Active and returning to Card Line; this candidate restores the
bounded equivalent of the old normalization, not a change to preview scaling.

Manual acceptance checks (do not automate against unsaved user windows):

1. Place a floating window at a distinctive size/location. Enter Card Line, select
   Active, return to Card Line, then release. Check original placement.
2. Maximize that window first. Repeat, then unmaximize after leaving Kadunce.
   Check its original floating size, not the maximized frame, is restored.
3. Repeat with fullscreen and quick-tile states, including partial maximization.
4. While Active, use titlebar move, resize, maximize, quick tile, and fullscreen.
   The ordinary desktop should take over without snapping the request back.
   Swipe back into Card Line to resume Kadunce.
5. Repeat entry/exit with a slow-resizing application and a constrained-size one;
   there should be no repeated resize correction. Document remaining visual jumps.
6. Verify the persistent kill switch before and after testing.
7. Fullscreen ChatGPT, enter Card Line and Active, then use Ctrl+Esc to release
   without disabling. The content must remain visible and fullscreen. Repeat
   release directly from Card Line, and with a second ordinary window open.
   This previously produced a black client until reentering Card Line. The
   candidate tears down presentation/redirection before restoring fullscreen
   and reasserts fullscreen focus after restoring stacking (normal session only).

Existing native tests and source guards do not emulate KWin's asynchronous window
configure/acknowledge cycle. The above live tests are still required.

Cleanup candidate: Active and Bento use the same restoration mechanics; output
placement, focus and mutation guards remain with their respective controllers.
Gutter values are cached and updated through KConfig notifications from the tray.
The window-handling test verifies ordered restoration with a recording client,
display-removal restore fallback, tiling precedence, and cross-process settings
updates on an isolated bus/config directory. After installation, repeat fullscreen
Ctrl+Esc and maximize/unmaximize, then save a gutter change and reenter Active.

Deferred audit items: output/work-area changes, minimize and desktop/activity
membership changes, constrained-window admission policy, dialog ownership, and
complete cross-output snapshot restoration. No claim of full lifecycle coverage.
