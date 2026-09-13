# Live stack lag investigation — September 13

Installed file: c063a199850224fbc7ed364fadc9be6fd4f53bb5fadef14a8eb88ff323022906,
the archived firm-pass renderer. No production changes/install during this test.
Host KWin process88170 started after J's rollback login. Its memory map was
permission-denied even with workspace sandbox disabled; disk hash is not a mapped
binary hash. Later process119015 is crash recovery, not a requested restart.

## Measurements

- Existing Discord/ChatGPT two-member stack; shortcuts invoked via kglobalaccel.
  Six calls reached journalled stack selection about7–9ms after shell timestamps.
- Controlled temporary evdev UInput touchscreen: two upward108px/182ms gestures.
  The38px threshold is crossed on step7, about71ms after down. Journal selection
  occurred at that time, before touch-up. Device closes at script exit.
- At60Hz, WITHOUT recording,124 D-Bus nativeCarryState samples had median5.87ms,
  max11.62ms round trips. Transition coordinates advanced through the280ms
  OutCubic interval. This does not measure GPU presentation or physical touch
  hardware latency; it excludes a large event-loop stall in these sampled cycles.
- Spectacle recordings contain100–250ms timestamp gaps at180Hz and up to166ms
  at60Hz. Screencast overhead/selection and variable-frame recording confound
  these observations: do not claim these are measured display frame times.
  The initial60Hz recording's first gesture selected the recorder; only the
  second gesture selected a stack member. Exclude the first from gesture analysis.

Artifacts in /tmp: kadunce-stack-cycle.webm, kadunce-touch-verified.webm/.log,
kadunce-touch-60hz.webm/.log, kadunce-touch-no-recording.log,
kadunce-touch-observe.py. No user files deleted or caches cleared.

## Concrete source finding, live causal proof still pending

Effect::prePaintScreen requests addRepaintFull while animationsRunning, but
Effect has no postPaintScreen continuation. In local KWin6.7.5 compositor.cpp:
prePaint at725 precedes renderLayers resetRepaints at806; postPaint is at881.
OutputLayer::resetRepaints clears both scheduled state and damage. A pre-paint
request can therefore be consumed by the current frame rather than keeping
subsequent animation frames dirty. KWin SlideEffect schedules continuation in
postPaintScreen. This fits changing animation coordinates with intermittent
visible updates, and dependency on unrelated application damage. It is not yet
a verified explanation of all historical differences or Zen's sampling waves.

Bounded correction to consider: schedule animation continuation after paint,
while retaining pre-paint masks. No timers, cache, geometry or input changes.
Verify final frame and idle termination; avoid permanently requesting frames.

## Diagnostic crash — do not repeat on live desktop

Loading showfps and starting a Spectacle recording for a repaint witness was
followed by KWin SIGSEGV at16:14:01; Zen SIGSEGV at16:14:03. The witness recording
is empty, and its result is INVALID. KWin stack starts EglContext::currentFramebuffer
→ pushFramebuffer → WorkspaceScene::paint → ScreenCastStream::record via QTimer.
The trace establishes the screencast crash path, not which component caused it.
No deliberate kill/restart/close was performed. The assistant's initial claim
that GPT was only behind Discord was incorrect and was corrected to J.

Recovered session: ChatGPT focused, showfps absent, no Spectacle/test process
running, Kadunce safety switch verified. ShapeCorners remains restored/enabled.
Do not repeat overlay-plus-recording or disturb the recovered foreground chat.
