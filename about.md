# Advanced Frame Counter

Shows **real Geometry Dash gameplay timing** — physics ticks, not FPS.

- **Method 1 – Direct tick:** the tick each input was processed at and the tick gap since your previous press. Live, tiny cost.
- **Method 2 – State analysis (experimental):** re-simulates the game's own physics from a snapshot before each press to find how many ticks it could shift and still survive. Runs while paused.
- **Method 3 – Recorded replay (experimental):** record a run (manual or external macro), then replay it from the start and probe every press.
- Buckets: **10-15, 7-9, 4-6, 3, 2, 1**, plus the exact number.
- HUD + on-player label, history, search, comparison view, record/save/load, optional playback.
- Pause-menu button, touch-friendly panel, Android64 + Android32 builds.

Observation-only by default: it never injects inputs unless you enable playback.
