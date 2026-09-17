# Card geometry audit — September 13

J passed front-first depth/order candidate6d551f44. Installed SHA:
6d551f44ba6780dcbd26b5cf7e52b8171142de810eda100f8c32f6f7a78bf19a.
Screenshot codex-clipboard-8cf6e7f4-a947-4912-9d90-bbb5afcb70e3.png shows a
tilted front card with a flat lower edge and enlarged/cropped content.
Do not infer texture memory growth or a leak from this image.

## Source findings, not yet a runtime fix

- Effect::paintWindow calls setRotationAngle and uses a bottom-right pivot:
  genuine rotation is requested, not an intentional shear.
- Tilted cardClip intersects fanBaseline whose bottom is deviceTarget.bottom,
  an unrotated horizontal boundary. That can slice rotated corners. It is a
  strong candidate for the apparent warping, not complete proof of all artifacts.
- Aperture coordinates, scale and pivot also need checking together against
  KWin's actual transformation implementation. Keep the hard output fence.
- KeepAspectRatioByExpanding is a fill/crop policy. It preserves aspect ratio
  but can magnify content rather than showing a complete window miniature.
- drawWindow uses OffscreenEffect redirection; captureCardTransition stores
  poses/rectangles, not client bitmap snapshots. No memory-growth measurement yet.

## Bounded next implementation order

1. Establish one geometric contract for Active, resting Card Line, browsed fan,
   insertion/placeholder, held card, release/cancel and output handoff. Document
   content bounds, card aperture, pivot, clip and source dimensions for each.
2. Reproduce corner clipping using a grid/test window. Rotate content and rounded
   aperture rigidly together; remove only erroneous per-card axis clipping, never
   the tablet/output fence. Check positive/negative tilt and fractional scale.
3. Define a faithful preview fit policy for mismatched aspect ratios; compare
   whole-window contain with current cover/crop. Do not change native app geometry
   simply to produce a thumbnail. Check stale native dimensions after Active.
4. Restore44% held presentation as a separate contact-anchored transform, using
   corrected content/aperture/pivot geometry. Do not resurrect failed timing code.
5. Use the existing normal-black translucent fill for the placeholder and retain
   its thin neutral outline; keep native desktop destination styling separate.
6. Then polish measured transitions. Preserve passed slotting and held-row intent.

J's phrase 'bloated snapshots' is provisionally interpreted as magnified previews.
If it means disk backups or memory use, clarify before expanding the work.
Physical checks: readable whole-window preview, rigid grid rotation, no corner
chop,44% pickup under the finger, release into the same slot, unchanged paging.
Focused geometry tests plus mandatory safety; no broad ownership rewrite.
