# Native-until-entry candidate — September 12

Ordinary windows stay native until a top/left/right edge or crossing into a card
workspace. Existing cards still pick up immediately. Pending contact motion is
projected once at acquisition; no geometry retry or second native movement loop.

Candidate SHA256: 9b84cd5a66165d516666e2a7251ddeb55f3479ce49595b323adc89f844fe9d88.
Build/source: /tmp/kadunce-integrated-carry.IQIMav/production and source/native.
Frozen bundle: ../work/kadunce-native-entry-test-20260912.
Installer: ../install-kadunce-native-entry-test.sh (opt-in only); --rollback returns
7248d3fbb764eca7d5fcb9895b45dc34cde912307d8ffaf411d47387fef1dc8a.
Installed binary, user session, configuration and trusted repair remain unchanged.

## Composite evidence for this exact native source

14 CTests: IQIMav/ctest.log. Runtime private compositor evidence:

| Route | /tmp evidence directory |
|---|---|
| Wayland native/free/entry/disable | kadunce-unload-test.Bgi4uA |
| Xwayland native/free/entry/disable | kadunce-unload-test.DgU5Ol |
| Managed pickup/held unload/settle unload | kadunce-unload-test.l1Wsf2 |
| Local Bento exchange/restoration | kadunce-unload-test.azKsqs |
| Ordinary edge/withdraw/dock/restoration, including single jump | kadunce-unload-test.EMDJRI |
| X11 real decoration entry/restoration | kadunce-unload-test.fj9tHQ |
| X11 native-only baseline | kadunce-unload-test.kmkms0 |
| X11 CSD proof/rejection/held unload | kadunce-unload-test.hseRef |
| X11 real button no-action/fresh-click | kadunce-unload-test.82v0aB |
| Tablet Active and ordinary/Bento return | kadunce-unload-test.IHgDM8 |
| Tablet edge entry/existing Bento receiver | kadunce-unload-test.4J1Gkd |
| Card Line/stack/reorder/settling | kadunce-unload-test.MMe3Vb |
| Ordinary tablet source edge/restoration | kadunce-unload-test.eM7WwA |
| Bottom exit/withdraw/released native drag | kadunce-unload-test.Xn90dK |
| Bounded deferred-native trace/reset | kadunce-unload-test.kQ4nTG |

Bento transfer/restoration/unload: /tmp/kadunce-bento-candidate.xIvnMd.
Source guards, control package and read-only live safety pass. No live effect test.

This is composite validation, not one uninterrupted aggregate run. Earlier
Hrhi9l found a real pending-motion bug, fixed before IQIMav. IQIMav stopped at
fIkDlh/tablet-runtime line63, expecting immediate ownership of a returned ordinary
window. Updated tablet assertions instead verify native state before crossing,
then owned state on crossing; all tablet routes passed separately against IQIMav.
The frozen bundle contains current tests, not the stale aggregate test snapshot.
Trace attempts 8nu1Eh/0qgrex produced only six native drags: Qt interpreted rapid
repeated presses as double-clicks. The fixture now spaces clicks beyond that
interval, actually exercises twelve drags and verifies the unchanged 48-event cap.

## Physical checks after opting in and logging out/in

1. Drag an ordinary GPT/Ghostty window freely, including a window just released
   from Bento. No card outline or forced sizing before an entry target.
2. Snap that ordinary window to a side edge, both slowly and with a fast movement.
   It should become Bento. Repeat across displays when available.
3. Drag a Bento card to the bottom edge, then move the released window normally.
   Keep 10px dock clearance; surviving cards stay arranged. Verify the tray switch.

Wallpaper blur is separate and unchanged. Occupied-tablet contention and automatic
new-app admission to an existing single-card Bento remain open, not claimed fixed.
