# Carry paint boundary

CarryPaintPlan projects frozen pickup size at CarrySession's anchored position.
Coordinates remain logical until the viewport maps the clip to device pixels.
Each output receives its intersection with the same target. Invalid/empty/nonfinite
geometry rejects; a valid off-output target paints nothing.

paintCarryWindow reuses KWin's proportional cover transform, as Card Line does,
and intersects the renderer region with the output-local clip. It does not resize
or assign windows, choose destinations, or mutate source membership. The caller
must match the explicit carried window, flag transformed prepaint and arrange
elevation. Frozen size is not a frozen texture: content remains live.

The private UnloadProbe connects this helper to NativeCarryHandoff and
CarryInputRoute during Active/Bento mouse/touch handoffs. Other windows retain
native painting. Effect.cpp remains unchanged. CURRENT_STATE records evidence.

## Remaining integration gates

- This helper is rectangular. Integrate the existing aperture deliberately;
  no rounded-corner or visual-quality approval is claimed.
- Keep passive tablet neighbours on cardPaintRoute's existing fence. This probe
  is not the full Card Line renderer.
- Coordinate prepaint, elevation, entry/motion/exit repaint, output/source loss,
  lifecycle drainage and restoration in Effect.
- No preview destination is supplied in tests, so release returns to source.
  Destination acceptance remains disconnected.
- Virtual paint callbacks and geometry checks are not screenshot comparisons or
  hardware frame-pacing evidence. Fractional scaling needs visual verification.

No runtime takeover enabled, install, push or repair promotion.
