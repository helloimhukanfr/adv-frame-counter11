# Advanced Frame Counter (Geode mod, Android 64 + 32)

Measures and displays Geometry Dash **gameplay frame timing** with a HUD, an on-player label, history,
search, a method comparison view, and a record/replay system.

> **Read "Verification status" first.** The pure logic is unit-tested. The Geode/GD-facing code was
> written against the Geode SDK but has **not been compiled, run on a device, or tested in Geometry Dash**
> by the author of this project.

## What a "frame" means here
One **Geometry Dash physics tick**, identified by `GJBaseGameLayer::m_gameState.m_currentProgress`.
It is **not** a render frame and **not** FPS. The mod measures the real step length itself
(`dt` of one `processCommands` call ÷ ticks it advanced) and stores it as `tps` in recordings; it does not assume 240.
No random numbers, timers, wall-clock time or monitor refresh are used as a frame source
(wall-clock appears only in saved *file names* and in UI fade timing).

## Buckets
`10-15`, `7-9`, `4-6`, `3`, `2`, `1` (ASCII hyphen: the game font has no en dash; `4–6` is accepted when parsing).
The exact measured count is always stored; the display mode picks bucket, exact, or both (`8 / 7-9`).
Counts above 15 land in `10-15` and are flagged as capped (`21+`).

## The three methods (genuinely different pipelines)
| | Measures | Output kind | Status |
|---|---|---|---|
| **1 Direct tick** | tick at which each real input was processed (hook on `GJBaseGameLayer::handleButton`) and ticks since your previous press by that player | **gap** (not a window) | implemented |
| **2 State analysis** | restores a game-state snapshot from `range` ticks before a press, re-runs the game's own physics with the press (and its release) shifted by −range…+range, checks the player survives `horizon` ticks; the contiguous surviving run around the real press is the window | **window** | implemented, experimental |
| **3 Recorded replay** | resets the level, replays a recorded run tick-by-tick, and runs the same probe at every recorded press | **window** | implemented, experimental |

Methods 2 and 3 share one probe core (`WindowProbe`) but differ in how states and inputs are obtained
(live pre-roll snapshots vs. deterministic replay of a recording).

**Honest limits of Method 1:** a tick stamp cannot say how early or late an input could have been, so Method 1 reports the
*gap* between presses, labelled `INPUT GAP` / `G`. It is never presented as a survivable window.
First press of an attempt has no gap → `UNAVAILABLE`.

**How windows are calculated (2/3):** `solveWindow` (unit-tested) probes offset 0 first. If the real input does not survive inside
the probe, the result is `METHOD LIMITATION` (state restore didn't reproduce the live game) — a number is never invented.
Otherwise it walks outward until the first death on each side (or the range limit → capped).
`window = latest − earliest + 1`. Deaths in probes are intercepted (`PlayLayer::destroyPlayer` returns early while probing), so no death effects play.
Each probe step is verified to advance exactly one tick; otherwise the probe fails → `METHOD LIMITATION`.

Validity states shown: `VALID`, `MEASURED`, `UNAVAILABLE`, `ANALYSIS REQUIRED`, `METHOD LIMITATION`. There are no confidence percentages.
Accuracy is not claimed for any method; no comparison against NaNGD or any other tool has been performed.

## Macros, recording, playback
* **Observation:** every input that reaches `handleButton` is observed *before* the game applies it, then forwarded unchanged. An external macro that drives the normal input path is therefore seen as real input; the mod injects nothing for observation. A macro that bypasses that function would not be seen.
* **Record Run:** panel → RECORD, play or start a macro, STOP. Stores tick, player, button, press/release, position, mode, dual. One recording = one attempt (death/restart/quit closes it).
* The recording is the *inputs as the game processed them, keyed by tick*. It does **not** make an external macro "more accurate"; it records what happened and lets Method 3 analyze it.
* **Save/Load:** JSON in `Mod::get()->getSaveDir()/runs/`, versioned (`version`, level, timing convention, tps, inputs). Corrupted/future-version files are rejected, never crash.
* **Playback:** off by default (`Allow Playback`). When on, recorded inputs are fed to `handleButton` at their ticks after restarting the level. Play/Pause/Stop/Restart in the panel. Cancelled by death or exit.

## UI
* **Pause button** (icon in `resources/icon-pause.png`): added by node ID `…/afc-pause-button` into the existing `right-button-menu` (layout re-flows around other mods) or, if that node is missing/`floating` mode is chosen, into its own menu at a configurable % position. Duplicate-checked, recreated with each pause layer.
* **Panel:** Methods (switch live), History, Search (bucket/player/method chips + input-index stepper; there is no text box, by design, for touch reliability), Compare, Record, Settings, Method info. Min button height ≈ 26–34 GD units; panel size derives from window size (16:9 → tablets).
* **Player label:** a child of the UI layer positioned from the player's parent node via `convertToWorldSpace`/`convertToNodeSpace`, so Cocos handles camera move/zoom/rotation/flip; Y offset flips with gravity; hidden for P2 unless dual.
* **Analysis progress** is real: Method 2 = finished jobs ÷ total jobs, Method 3 = finished presses ÷ total presses.

## Threading
No worker threads. Analysis is time-sliced on the main thread (`Analysis Time Slice`), and only while the game is **paused**, because probing re-steps the live game. No Cocos object is touched off-thread.

## Gameplay-mode notes
Timing is read from the same tick counter in all modes, but Method 2/3 window meaning differs by mode and has **not been studied per mode**: cube/ball/spider/robot (press-edge timing), ship/UFO/wave/swing (hold timing — only the press/release pair is shifted), dual (each player probed separately), platformer (left/right buttons are probed as normal buttons). Mini, speed and gravity changes are inside the snapshot, mirror/teleport portals were not specially handled. Treat modes other than cube as unvalidated.

## Verification status
**Verified (by running code in this authoring environment):**
`bash tests/run_tests.sh` — assertions covering bucket classification, display formatting, search parsing, window solver (including limitation paths), Method 1 gap logic, history bounding, run JSON round-trip, validation, corrupted/future-version/path-traversal rejection.

**Verified by reading only:** `mod.json` parses as JSON and every setting key used in code exists in it (and vice versa); GitHub workflow follows the official `geode-sdk/build-geode-mod` example.

**Expected, not verified:** that the project compiles against Geode 5.9.0 for Android64 and Android32; that the hooks below behave as assumed; the whole UI; everything on a device.

**Not locally testable:** Geode build, Android32/Android64 binaries, Geometry Dash behaviour, macro interaction, Methods 2/3 correctness.

### Geode/GD API assumptions to check if the build or runtime fails
All field access is centralized in `src/Platform/Game.hpp`.
`GJBaseGameLayer::handleButton(bool,int,bool)`, `::processCommands(float)`, `m_gameState.m_currentProgress`, `m_gameState.m_isDualMode`,
`PlayLayer::createCheckpoint()`, `::loadFromCheckpoint(CheckpointObject*)`, `::destroyPlayer`, `::levelComplete`, `::resume`, `::onQuit`, `::resetLevel`, `::postUpdate`, `::setupHasCompleted`,
`m_level->m_levelID.value()`, `m_levelSettings->m_platformerMode`, `PlayerObject::m_isShip/m_isBall/m_isBird/m_isDart/m_isRobot/m_isSpider/m_isSwing/m_isUpsideDown`,
node ID `right-button-menu`, `CCMenuItemExt::createSpriteExtra`. If a hook signature differs, fix it per the Geode bindings.
Known risks: `processCommands` may not be one-call-per-tick (then Method 2/3 report `METHOD LIMITATION`); `createCheckpoint()` each tick has a cost and may have side effects in practice mode; probing may not perfectly restore held-button state, random triggers or audio.

## Install
Download `Build Output` from the Actions run → get the `.geode` file → import it with the Geode launcher / Geode's mod manager on your device (follow the current Geode Android instructions; the exact folder is not asserted here). Requires Geode (≥ 5.9.0 as pinned in `mod.json`) and a compatible GD version.

## Build
1. Create a GitHub repo, upload this folder's contents (keep `.github/`).
2. **Before pushing:** change `id` (`yourname.advanced-frame-counter`), `developer`, `repository` in `mod.json`, and the LICENSE name.
3. Actions → *Build Geode Mod* → Run workflow. It runs the unit tests, then builds `Android32` and `Android64` with `geode-sdk/build-geode-mod`, then combines them into one `.geode` (artifact "Build Output").
4. Local: install the Geode CLI, `geode sdk install`, then `geode build` (add `--platform android64` / `android32` as your CLI supports).

## Troubleshooting
* *Compile error about a field/function:* see "API assumptions" and fix in `src/Platform/Game.hpp` or the hook signature.
* *CI complains about the `geode` version:* edit `"geode"` in `mod.json` to the SDK version it names.
* *Method 2/3 shows METHOD LIMITATION:* enable *Allow Experimental Probing*; if it persists, probing does not reproduce the game on your GD version — please report it, do not trust numbers.
* *No pause button:* check *Pause Menu Button*; try `floating` placement.
* *Nothing measured:* Method 1 needs two presses by the same player; Method 2 needs the probe horizon to elapse after a press and then a pause.

## Layout
`src/Core` (types, buckets, search, solver, JSON, history) · `src/Recording` (inputs, run format) · `src/Methods` (3 methods + probe core) ·
`src/Engine` (glue) · `src/Storage` · `src/UI` · `src/Hooks` · `src/Platform` (settings/game access) · `tests/` · `.github/workflows/build.yml`.

## Research basis
Concepts only (no code copied): hyper-5/frame-window-counter loads windows from imported macros and draws markers/HUD; this project instead tries to *measure* windows. Geode CI per `geode-sdk/build-geode-mod`.
