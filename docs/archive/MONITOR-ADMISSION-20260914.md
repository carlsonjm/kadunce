# Fresh-state monitor admission / tablet ownership

J's reset-between-tests results: 1/2/3/5/7/8/9/10 passed. Test4 hid the fourth
monitor window; test7 later admitted four. Test6 transferred at Active size but
allowed native resizing during the first upward Card Line swipe.

J installed and passed both failure retests after accepted884e5c3.
Freeze/push authorized. Installed candidate hash and post-install live safety
control verified; source and installed build match.

- Monitor edge fallback compares its fitting subset to ordinary transfer
  admission. Only a strictly larger visible set wins; required arrival and edge
  card must both remain visible. Equal-count layouts keep side intent. Successful
  occupied-column splitting stays first; tablet side behavior is unchanged.
- Fresh/inactive tablet receiver now commits membership and enables Card Stage
  before native Active placement. The supplied restore record is retained;
  existing enterActive and managed-resize protection apply. Previously the
  inactive branch resized/activated an ordinary window without retaining ownership.
  Eligibility checks now apply to inactive arrivals too.

Evidence: production build and bento-layout, bento-transfer, workspace-state,
card-line-layout pass. New layout regression covers four600x600 minimum windows
on2540x1410 and both monitor sides. Existing state tests cover empty receiver
admission and source rejection. J reports both physical retests passed;
this is not a claimed private-compositor reproduction of J's report.

Installer ../install-kadunce-monitor-admission-20260914.sh.
Candidate5df69825c2d593e9416a1f3e0633aad4f519aa43955b19a39b57cbf04510dd48.
Rollback211f7c2e8751f83c840eadb9c1c39a6df06f2f0f9869debfb78e8062574fb3ba.
No Tette/Temperance, engine, repair, renderer or motion changes.

Physical: fresh enable then repeat test4; fresh enable then repeat test6 and
first upward swipe; disable afterward to verify original geometry restoration.
Do not broaden this into speculative layout/ownership refactoring.
