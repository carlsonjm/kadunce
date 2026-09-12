# Shared native-move observation

September 11, 2026. `native/src/NativeMoveObserver.h` replaces the private
ContactProbe's duplicate detection implementation. The probe is now instrumentation
around this shared component. Effect does not instantiate it yet.

The observer is passive: it never consumes input, cancels a native move, changes
geometry, or commits membership. It produces a window-specific CarryContacts
ticket after app-request serial/surface correlation or motion-driven decoration
correlation. NativeCarryHandoff must revalidate that ticket during adoption.
Tracking a held contact alone is not admission proof.

It watches multiple windows idempotently, follows surface/decoration replacement,
and disconnects on removal/destruction. Lookup is per window, never per frame.
Protocol-less windows cannot produce decoration admission. Unknown/XWayland,
keyboard/system-menu and delayed-hold starts remain unsupported. Queued decoration
candidates are short-lived but do not prove arbitrary later timer-driven starts;
do not broaden the proven motion path without additional testing.

InputDevice lifetime tokens are monotonic. Device capabilities are cached while
alive, so destruction never calls virtual device methods. Touch events expose
seat-level IDs rather than a physical device; removal of any touchscreen therefore
invalidates touch reservations conservatively. Multiple held contacts remain
ambiguous. Pointer provenance is saved before native start clears pointer focus.

`cancelTouch()` must be called before another filter can swallow cancellation.
The private probe still uses its experimental early passive cancel filter; this
is NOT a production priority decision. The observer deliberately has no release
drain/gesture policy. Its callbacks must not synchronously delete the observer.

## Validation

Expanded isolated contact suite passes app pointer/touch identification,
Active/Bento acceptance/rejection/interrupted native finish, ambiguous contacts,
keyboard MoveOp, genuine Breeze titlebar motion, observer recreation/destruction,
and new/existing clients. Additional checks cover touchscreen removal and
idempotent simultaneous multi-window watches with exact protocol identity.
See the latest evidence recorded in CURRENT_STATE.

All 12 standard tests and source/control/live safety checks pass. Installed plugin
and repair hashes are unchanged. No install, push, or production takeover enabled.

## Integration gate

Next connect owner-specific filtering/draining, frozen-geometry carry rendering,
and validated destination acceptance to NativeCarryHandoff in Effect together.
Start must reserve source before legacy release; native cancel finish must bypass
legacy drop. Ordinary unsupported starts retain their existing native behavior.
Keep passive tablet clipping, bottom dock controls, and immediate disable intact.
Only then run the full Effect candidate in the isolated compositor. Component
tests are not full Effect completion or hardware acceptance.
