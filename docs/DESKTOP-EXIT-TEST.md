# Desktop exit candidate

September 12 physical result: partial pass, not a freeze. See NEXT-ROADMAP.md.
The above-dock strip below describes this candidate; user now requests the actual
bottom screen edge. Ordinary-window blur/outline and occupied-tablet entry failed.

Opt-in only. From the parent workspace, run
`bash install-kadunce-desktop-exit-test.sh`, save work, then log out/in.
The same command with `--rollback` restores the previous Bento-entry build.
Neither command promotes repair or restarts the session automatically.

1. Verify the persistent tray kill switch is present.
2. On the tablet, drag an ordinary window to a side/top edge. It should enter
   Bento without first visiting Card Line. Ordinary monitor entry must still work.
3. Release all layouts with Ctrl+Esc. Move an ordinary window in open space:
   no lingering layout or pull into Active. Check the reported blur/lag directly;
   software compositor tests cannot establish physical appearance or frame pacing.
4. On a monitor with Bento, drag one card into the strip just above the dock.
   Expect a normal-window outline and Return to desktop label. Release there:
   the window detaches, survivors reflow, and a lone survivor remains Active.
5. Move that detached window in open space again: it must remain ordinary, even
   while other windows stay in Bento. A deliberate side/top edge can rejoin Bento.
6. Repeat with a single Bento card: bottom departure leaves no layout behind.
7. Pull back from the exit strip before releasing: no detachment. The protected
   dock area itself is not the exit target. Normal dock taps remain unchanged.
8. Confirm Ctrl+B still toggles the focused display and Ctrl+S targets tablet.

Stack timing, left-neighbor selection, and touch motion tuning are unchanged.
