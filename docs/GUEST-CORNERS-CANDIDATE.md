# Guest dismissal and Card Line aperture freeze — 2026-09-10

User-approved behavior freeze, followed by approved package artwork and an
editorial pass on package descriptions. Ready for publication.

- Outside guest contacts are held until release. A stationary tap dismisses the
  guest without selecting a neighbor; movement and canceled touches do not tap.
  Panel transactions keep their existing pass-through behavior.
- A committed guest swipe arms a one-shot guard for the underlying app's focus
  restoration. A different application activation still proceeds. The guard
  expires after one second and does not change the existing guest collapse geometry.
- Card Line's rounded aperture uses interpolated geometry coordinates, independent
  of sampled texture coordinates and their transformations. KWin 6.7.5's custom
  shader attribute names are position/texcoord. The cover transform, rotated
  fan poses, and system-rounded Active rendering are unchanged.

Seven native tests, source checks, shader vertex validation, and safety-control
checks pass. User confirmed no clipped edges found and that swiping Tette away
leaves Card Line open, and accepted this batch. Other interaction fixtures are
automated coverage, not a claim of exhaustive live touch validation.

Separate unresolved user reports: touch inputs are not reading while docked;
desktop handoff from another display is a larger issue. These remain open and
are not covered by this acceptance. Do not change display or input behavior as
part of adding the forthcoming widget artwork.

Installed, accepted plugin SHA-256 (verified at freeze):
`02580dbfc116899190fa6d9d7cbdbb2a1ff82613690d0fc1243cf9d0ccfc8df1`

Tette and Temperance behavior is unchanged. Subsequent package artwork and
description edits change build hashes but not this accepted behavior. This
native KWin update requires a new login session, not merely restarting the
Plasma panel; that login and the artwork check were confirmed by the user.
