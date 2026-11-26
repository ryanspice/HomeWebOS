# Native Home audit — performance, TV integration, webOS app essentials

Date: 2026-11-25. Status: **dated source-review snapshot plus desktop measurement.**
The findings below are preserved as written at review time; `file:line` anchors
are historical and several have since drifted — prefer symbol names.

Disposition as of 2026-11-26 (see `.ai/evidence/20261125-overnight/`):

- **P1/P2** implemented and measured (`p1-result.md`).
- **P4 text keys** implemented (interned `TextKey`, per-frame raster counter);
  **P4 idle-rate cap** mechanism added as opt-in `ambient_idle_hz`, default 60.
- **P5** event-aware pacing implemented (`p5-pacing.md`), no `sleep_until` remains.
- **P6** Magic Remote wheel mapping implemented in `src/main.cpp`.
- **T3** `deeplinkingParams: spiceHomeView` declared in `appinfo.json`.
- **T1 persistent LS2, T2 power-state, T5 keyboard/localization, T7 CEC,
  T8 display-mode negotiation** and physical acceptance remain open.

No TV install, Luna permission grant or physical acceptance is claimed here.

Evidence: `.ai/evidence/20261125-native-audit/` (reports, input scripts, settings
snapshots). Measurement host: WSL `WebOSBuild`, llvmpipe/Xvfb desktop build, deterministic
fixed step (`--no-vsync --fps 60 --step-ms 16.666… --frames N`), 1920x1080 unless stated.
These are draw/CPU-side numbers, not C3 GPU timings.

## 1. Landed in this increment

1. **Home-cache invalidation is now limited to content that is actually cached**
   (`src/ui.cpp:1120`). `Deck::update` used to advance `home_content_revision_` for every
   drained service reply, including processor/settings/device/input/retro replies that only
   feed widgets, panels and toasts — all painted live since the widget split. A full
   drawable-sized home repaint per reply is gone; the app inventory reply still invalidates.
2. **No per-frame clock work in `draw()`** (`src/ui.cpp:1954`). The clock change test now
   runs once per simulated second in `update()` (`src/ui.cpp:1109`); drawing no longer calls
   `localtime`/`strftime` and no longer bumps the revision from the draw path.
3. **webOS lifecycle notifications are handled** (`src/main.cpp:433`, `src/main.cpp:438`):
   `SDL_APP_TERMINATING` flushes the ordered settings writer before exit (webOS may terminate
   without a signal), and `SDL_APP_LOWMEMORY` releases derived GPU caches.
   `Renderer::release_caches()` (`src/render.cpp:584`, declared `include/spice/render.hpp:164`)
   frees text textures, metrics, ellipsis and the Home layer and forces a rebuild; prepared
   artwork stays resident so the render thread never re-reads files.
   `Deck::release_caches()` (`src/ui.cpp:1100`) bumps the revision so the next frame repaints.
   Covered by `tests/home_cache_renderer_checks.cpp`: after release the cache bytes and text
   entries are zero, the next `home_layer` repaints rather than hits, and the rebuilt layer
   matches a direct draw (last comparison `max=2`, `changed=142` channels of 1280x720x4).

Measured effect (60 frames, fixed step, 1080p):

| case | draws before | draws after | repaints before | repaints after |
|---|---|---|---|---|
| settled home, ambient on | 5,534 | 5,280 | 2 | 1 |
| 10 focus moves in 1 s | 19,577 | 19,323 | 4 | 3 |

Checks: desktop build clean; `ctest` 12/12; ASan/UBSan `build/ui-asan` `ctest` 12/12;
`tests/native_motion_qa.py` 35/35; `tests/native_qa.py` 56/56; `tests/native_pacing_qa.py` 4/4.

## 2. Measured frame-cost structure

Marginal cost of a presented frame on the home view (62-frame run minus 2-frame run):

| state | draws per presented frame | notes |
|---|---|---|
| ambient motion on (default) | 83.7 | ambient layer ~40 draws + widget cards ~30 |
| reduced/ambient off | 43.0 | Home layer blit 1 + focus + live widget cards |

One focus move costs ~7,100 extra draws over the following 62 frames (9,761 vs ~2,666),
i.e. about 22 frames at ~322 draws each. Home cache memory: 7.91 MiB at 1080p, 31.64 MiB at
4K against a 32 MiB cap (`src/render.cpp:551`) — **4K sits at 95 % of the cap**.

## 3. Performance findings

**P1 — every focus move runs a 0.3 s full, uncached home repaint.** `Deck::draw` bypasses the
cache whenever the artwork crossfade animates (`src/ui.cpp:1956`), so `draw_home()` (rail,
hero text, rules, backdrop) is re-issued every frame for ~18 frames per selection change. This
dominates interaction cost and matches the owner's "sluggish" complaint from the November 5
research. Fix (plan R1): split backdrop from content. Draw the backdrop as two live full-screen
quads (a crossfade needs no cache) and cache only a transparent content layer keyed by a
content revision. Gate: a focus move repaints at most once and presents ≤ ~60 draws/frame;
cached-vs-direct pixel comparison extended to the transparent layer, including alpha edges
(premultiplied `glBlendFuncSeparate` must be validated). **Blocker to size first:** a second
4K RGBA layer does not fit the 32 MiB cap, so the backdrop must stay live (as proposed) or the
cap/format policy must change.

**P2 — focus changes still invalidate the whole home layer.** `Deck::focus_changed`
(`src/ui.cpp:518`) bumps the revision unconditionally, because the cached layer contains the
focused tile's 2 px boost and the hero title/description. With P1 done, moving the hero block
and the tile emphasis outside the cache (emphasis as the existing focus ring/shine) is what
makes "a focus-only move never repaints stable composition" true. This is a visual decision
(tile growth vs ring emphasis) and needs owner sign-off.

**P3 — idle home presents continuously at 60 fps.** Ambient motion is on by default
(`include/spice/core.hpp:144`); each idle frame re-blits a 31.6 MiB cache texture at 4K and
submits ~84 draws. On an OLED this is continuous panel excitation and power for a screen the
user is not touching. Options: rate-limit ambient-only redraws (e.g. 30 fps after N seconds
without input), or reduce ambient drift amplitude over time. Deterministic QA already runs on
the fixed-step clock, so a time-based cap is testable. Product decision; not changed here.

**P4 — text is still per-string FreeType rasterisation + upload.** `Renderer::rasterize`
(`src/render.cpp:697`) builds one texture per (string, size) into a 384-entry / 24 MB LRU
(`evict_texts`, `src/render.cpp:535`); `text_key` allocates a string per call
(`src/render.cpp:601`). Public app titles change per focus move and per library page, and the
live widget values change as data arrives, so this is a steady trickle of raster+upload. Plan
T1 (glyph atlas, per-run texture cache) remains the right fix; a cheap interim is an interned
`TextKey` (no string concat) and a per-frame total rasterisation counter in `motion_report`
for observability.

**P5 — pacing still sleeps uninterruptibly.** `std::this_thread::sleep_until(next_frame)`
(`src/main.cpp:612`) delays input by up to one frame budget (~5 ms at 60 fps with a ~11 ms
frame). Plan I1 asks for an event-aware deadline wait that carries the waited event through the
next dispatch. The idle branch still requeues via `PushEvent` (`src/main.cpp:621`), a
round-trip the plan removes. Both are one restructure of the event loop and need the
`input_order_checks` ordering gate rerun.

**P6 — Magic Remote wheel maps vertically to horizontal focus.** `src/main.cpp:507-511` sends
`up`-wheel to `left`/`right`, so a vertical list cannot be scrolled and the direction is
inverted for the horizontal rail. Wheel should map to up/down in the grid views and
left/right only on the home rail.

## 4. TV integration findings

**T1 — all Luna traffic is one `luna-send` process per call.** `bounded_process`
(`src/services.cpp:29`) spawns a helper per request; the allowlist was nine endpoints at review time (eight now)
(`src/services.cpp:24`). Each call costs a fork/exec and a helper start, so live widgets
(volume, foreground app, power/screen state) are impractical this way. The structural fix is a
persistent LS2 client (`libluna-service2`) or a single `luna-send -i` subscription worker, with
the same bounded, allowlisted, no-token rules. Verify the SDK's LS2 headers before planning
the build gate; nothing about the current public/private split changes.

**T2 — no power/screen-state awareness.** Nothing queries or subscribes to
`com.webos.service.tvpower` (power state, screen off). If the panel is turned off (or the
screensaver runs) without an SDL background event, the app keeps presenting ambient frames.
Pair with P3: stop presenting on screen-off and resume on screen-on.

**T3 — no launch/deep-link contract.** `appinfo.json` has no `deeplinkingParams`, so the
`spiceHomeView` launch argument parsed in `src/main.cpp:40` is not declared to webOS. Declare
it, and restore the last view on relaunch (`handlesRelaunch` is already true) instead of always
starting at home.

**T4 — permission model is not expressed in the documented form.** `appinfo.json:14` lists
`requiredACG`; webOS app manifests document `requiredPermissions` for LS2 service groups.
Only public services are used by default and private reads are opt-in (`SPICE_CPP_PRIVATE_LUNA`),
but the manifest key should be verified against the SDK the next IPK is cut with, and the
private-read path re-proven on device. Note the installed r13 manifest also predates
`"resolution": "3840x2160"` in the source file, so the next package differs from r13.

**T5 — no system keyboard or localization.** Text entry is the in-app keyboard only; there is
no `com.webos.keyboardService` invocation, and all labels are English even though the language
is read from `getSystemSettings`. An accessibility bridge (audio guidance/screen reader) is
also absent. These are user-facing gaps for a Home replacement, not prototype details.

**T6 — app lifecycle beyond SDL.** No `com.webos.service.applifecycle` registration or
foreground-app subscription, so resume context ("return to the app you left") and
relaunch-into-the-same-item cannot use system signals. The new `SDL_APP_TERMINATING`/
`SDL_APP_LOWMEMORY` handling is the part that can be done without new transport.

**T7 — no CEC/HDMI input switching.** Only the stored handoff dwell exists; there is no
`com.webos.service.eim`/CEC command and no HDMI input switch. Keep as an explicit non-goal
until a hardware acceptance path exists.

**T8 — no display-mode (120 Hz) negotiation.** `opt.fps` is a software ceiling
(`src/main.cpp:268` prints exactly that when vsync is unavailable); no
`com.webos.service.tv.display` query or mode request exists, so physical 120 Hz remains
unverified as documented.

**T9 — no structured logging.** Output is stdout/stderr only; a webOS app that misbehaves on a
customer TV benefits from `PmLog` context IDs so the launcher's log can be filtered.

## 5. App-essentials findings

- **A1** Icon is a single 512x512 `icon.png`; `largeIcon` points at the same file. The launcher
  card can distinguish them.
- **A2** Memory ceiling: 31.64 MiB home layer + up to 24 MB text + prepared artwork (~48.06 MB verified in the upgraded bundle
  admitted). Low-memory release now exists for the derived caches only; artwork purge would
  need a reload path that is not on the render thread.
- **A3** Settings live under `/media/internal/spice-home-cpp`; confirm the webOS-recommended
  writable location for a jailed native app when the next package is cut.
- **A4** Debug affordances are all CLI flags; a device logging level and an in-app diagnostics
  export would make field reports usable without a shell.
- **A5** No crash/relaunch backoff: if the launcher restarts the app in a loop, nothing detects
  it. A small launch counter in the settings file would let the app fall back to a safe view.

## 6. Suggested order

1. P1 + P2 together (backdrop live, content layer transparent, hero/emphasis out of the cache)
   with an extended cache-equality gate. Biggest interaction win; needs the tile-emphasis
   decision and an alpha-blending validation.
2. P6, P5, P4 interim (wheel mapping, event-aware pacing, key interning) — small, testable.
3. T1 (persistent LS2) once the SDK headers are confirmed, then T2 and T3 on top of it.
4. P3 (idle ambient policy) as a product decision before any long unattended OLED run.
