# C++ port parity — explicit status

Baseline: the mounted Spice Home v0.3.1 agent-ready archive, not the user's current
on-device checkout. This is a parallel C++ implementation, not a claim that every later
Rust change has been merged. The archive SHA-256 is in `assets/PROVENANCE.md`.

Current device status: [November 5 C3 deployment](TV-INSTALL-2026-11-05.md).
The ARM app runs as default Home with a 3840x2160 drawable. Native Back returns
Home, Processor Deck is off, and Home uses bounded caching/software pacing.
Physical input, presentation, cold boot and long-run acceptance retain their own gates.

| Contract | C++ status | Evidence / remaining gate |
|---|---|---|
| Controller library search and return context | Implemented | Native keyboard, eight-entry history, page bounds and stable selection; [November 5 evidence](FEATURE-PARITY-FLEET-2026-11-05.md) |
| Input-age correction and ordered settings saves | Implemented | Presentation-only pre-input step, post-input dwell/services, background snapshots and explicit final acknowledgments; 11 CTest gates in both builds |
| Local native themes/icons | Implemented for prepared image themes | Startup background/accent/semantic/app mappings and restore; direct `.p5theme`/DDS/SVG remain gaps; [contract](NATIVE-THEMES-IMPLEMENTATION.md) |
| PSN/Xbox profile dashboard | Working standalone demo | Import/export, explicit bounded public collector; supplied Xbox handle unverified, no PSN ID, no account stats observed; [page](../demos/profiles/index.html) |
| Main PS5 layout, rail and compact cards | Implemented and rendered natively | native home captures at 720p/1080p/2160p |
| Right-leaning 4/3/2 under fourth column | Implemented, pure geometry shared with input | core test + native D-pad/pointer scenarios |
| Time-based focus, background, panel motion | Finite retargetable focus, one queued artwork target, shared panel presentation | 60/120/240 step equivalence, exact settlement, blur/resume and reduced-motion native checks; `MOTION-UX-2026-11-04.md` |
| PS5-style ambient wallpaper motion | Implemented, opt-out via `ambient_motion` setting and reduced motion | Bounded deterministic 28-particle layer, focus parallax, breathing vignette and focus shine, drawn outside the cached home layer; November 23 checks below |
| Pointer geometry during panel/inspector entry | Shared with painted presentation | Native visible-edge activation, unpainted-edge rejection, Control Center and inspector checks at 720p/2160p and letterbox sizes |
| Reduced motion and expanded-grid settings | Implemented and persisted | motion-and-collapse/settings-reload scenarios |
| Raw GLES2 rendering | Implemented and exercised | native reports identify OpenGL ES 2.0 Mesa |
| HiDPI physical drawable and pointer mapping | Implemented | 2× DPR unit tests and actual 4K framebuffer/pointer test |
| 120 Hz TV presentation / GPU performance | **Unverified** | Requires C3 measurements; Xvfb swap interval unavailable |
| 4K/high-DPI/240 Hz design target | Native desktop functional checks pass; hardware qualification open | 24 actual 4K comparison captures; default and batched paths each pass 56 UI + 35 motion checks; fixed animation time and llvmpipe are not physical 240 Hz proof |
| Live backdrop / transparent cached Home content | Implemented (P1/P2) | Live artwork crossfade outside the cache, premultiplied transparent content layer, fixed-size tiles with live ring/shine; focus-move draws 13,550 → 3,679 at 1080p, at most one content repaint; ambient-on totals reported separately; evidence in `.ai/evidence/20261125-overnight/p1-result.md` |
| Coordinated Home, utility and concept styling | Implemented and captured natively | Accent hierarchy, darker utility surfaces, card dividers and bounded scene detail; original 4/3/2 geometry and input authority retained; human acceptance open |
| Metrics-only text measurement and bounded caches | Implemented and checked | Same width checksum with 186 measurement textures / 5,236,044 bytes avoided in the recorded probe; native metrics/raster agreement and cache-admission tests |
| Explicit TV metadata/artwork preparation | Implemented and exercised on C3 | Current root-assisted preparation: 186 registry records, 58 visible apps, 128 hidden records and 53 admitted graphics; original developer snapshot retained |
| Local snapshot startup with no automatic metadata requests | Implemented and checked natively | Hash-validated immutable generation, declared graphics uploaded before frames; both render modes, configuration/package discovery and corrupt-data behavior |
| Single-app preparation after an install | Implemented and checked | Explicit `-AppId`; other 20 real records retained; no full inventory/device query; graphics admission protects other apps; re-stage/relaunch to apply |
| Footer refresh, new items and prior launcher review | Implemented in working tree | Pointer/D-pad/R, worker metadata scan, scope-aware baseline, six current naming-based candidates, full identity and correct named pin target; no uninstall action; `REFRESH-SCAN-2026-11-04.md` |
| Direct Darkatek7/Prospero archive support | Researched, not implemented | Prepared image themes work; archive/catalog, DDS/zstd and SVG conversion remain separate |
| LG Home / full installed inventory / marketing feeds | Full installed inventory and packaged artwork prepared with existing root helper | Ordinary runtime caller remains narrower; live marketing feeds and automatic privileged refresh are absent |
| Painter-order batch renderer | Implemented, experimental opt-in | 1,247 primitives: 428 submissions, zero changed channels in recorded 720p/4K fixture; 4.5× vertex payload; target GPU benefit unverified |
| One-pass backdrop lighting and tint | Default, previous path retained for comparison | Steady 4 passes → 1; crossfade 5 → 1; interior RGBA delta ≤2/255 in fixture, outer-edge differences recorded; see `VISUAL-PERFORMANCE-2026-11-04.md` |
| Desktop keyboard, pointer, wheel and controllers | Implemented | keyboard/pointer automated; physical controller hardware not tested |
| LG Magic Remote scancodes | Configurable adapter present | Must record actual SDL codes on device |
| Library search/paging and native app shortcuts | Implemented | Desktop sample inventory tests; on-TV discovery permission pending |
| Favorites and manual processor shortcut | Implemented | Persistent settings; real TV app launch pending |
| Six themes | **Partial** | Six accent palettes, not six complete original console UI engines |
| LG settings | Read-only categories/custom keys and stored pins implemented | API/permissions not device-tested; pins not shown as Control Center widgets |
| Native public Luna transport | Implemented, allowlisted, bounded, off render thread | Core process safety tests; native jail identity/permissions pending |
| Direct LS2 native binding | **Not implemented** | Current transport runs luna-send-pub/luna-send with argv, never a shell |
| Installed apps / HDMI signal inventory | Private read-only code, default off | No claim of public API or automatic privilege grants |
| TV identity / network queries | Worker request path implemented | Device report presentation is minimal; live subscription not ported |
| Processor Deck | Disabled on TV by owner request | No native CPU/RAM sampling or Home/Control Center targets; desktop development sampler retained |
| RetroPie | Status client and HDMI shortcut; optional existing companion | Python API tests; no actual Pi pairing tested |
| RetroPie remote EmulationStation start | **Not used by client** | Companion opt-in endpoint retained but launcher never invokes it |
| Homebrew management | Explicit launch and existence probe | No unattended updates or privileged package installer |
| Preview Lab / native handoff | Event log, pause, Back suppression, confirmation, cooldown | Core state tests + native safety/confirmation scenarios |
| Inline TV/HDMI or other app video | **Not implemented** | Requires verified native decoder/video-plane integration, not tv:// inside SDL |
| Theme Studio | **Not ported** | Original browser theme studio not disguised as native UI |
| 50-concept design catalog browser | Ported as POC presentation data only | Settings entry, focus/return restoration, category/search/reset, paging boundaries and inspector stepping; `native_qa.py` + GLES2 captures; entries are never launchable |
| Per-design procedural scene art (15 families) | **Implemented** in `src/ui_scenes.cpp` | Shared by tiles and inspector hero with the primitive renderer; fictional art only, no images, decoders or downloads |
| Native on-screen search keyboard | **Implemented for library and design collection** | A–Z / 0–9, Space, UTF8-aware delete, Clear, results count, D-pad / OK / pointer; hardware typing suppresses launcher shortcuts |
| General OSK / screen reader / complex script shaping | **Not implemented** | Collection search keyboard is scoped to this view; FreeType glyph layout is not a full accessible text engine |
| Native Windows executable | Source/build helper provided, **not built here** | Requires MSVC/vcpkg validation |
| ARM webOS IPK | **Built, installed and running** | Real SDK/headers, ELF32 ARM, 79 verified payload files, live process hash/library checks and C3 compositor capture |
| Native Home / Home key / boot | **Configured by owner request** | Native default API and existing Input Hook/boot routes, guarded recovery; physical button and cold-boot acceptance remain open |

### November 23 ambient-motion increment

The home screen now carries a bounded, deterministic ambient layer adapted as
*concepts only* from LiveShell, ps5-theme, PS5_ArtSwap and the edalgrin /
singharyan006 references staged in `.ai/inbox/20261123-ps5-animations-and-likeness/`:
28 seeded drifting particles in the middle band, focus-driven parallax capped at
14 logical pixels, a breathing vignette and a 0.5 s focus-shine pulse. It is drawn
after `renderer_.home_layer`, so the cached home content is unchanged, and it
pauses while the window is hidden, frozen under reduced motion or disabled with
`"ambient_motion": false` in settings. Desktop verification on this date:
ASan/UBSan core build 64/64 (CTest 6/6), ASan/UBSan UI build CTest 12/12 incl. 6 native renderer checks, core checks 64/64, CTest 12/12, `native_motion_qa.py` 35/35, `native_qa.py` 56/56 and
`native_pacing_qa.py` 4/4, plus 3840x2160 ambience-on/off captures in
`.ai/evidence/20261123-ps5-ambient-motion/captures/`, with a measured
ambience-on/off software-renderer comparison in
`.ai/evidence/20261123-ps5-ambient-motion/perf-comparison.md`. These are Mesa
`llvmpipe` software-renderer observations: no physical GPU timing, target-GPU
qualification, C3 presentation, video wallpaper or `.p5theme` ingestion is
claimed by this increment. Design and reference provenance are in
`.ai/outbox/20261123-ps5-ambient-motion/`.

### November 25 widget motion increment

The three right-column widget cards (`widget:0`, `widget:1`, `widget:2`) now
share the home tile/ambient motion layer instead of sitting still: the same
capped focus parallax (at 20 % depth, so a card stays inside its shared
`home_nodes` rectangle and focus outline) plus a per-card breathing accent wash
and rim driven by the fixed-step `AmbientMotion` clock. Cards are painted after
`renderer_.home_layer`, so a moving card never invalidates the cached home
content; reduced motion, the `ambient_motion` opt-out and every non-home view
return the exact static rectangle with no extra draws. `motion_report` gains an
additive `widget_motion` object (`enabled`, `cards`, `drift_x`, `drift_y`,
`breath`, `layer`); no existing field changed. Desktop verification on this
date: desktop build clean, CTest 12/12, ASan/UBSan UI build CTest 12/12,
`native_motion_qa.py` 35/35, `native_qa.py` 56/56 and `native_pacing_qa.py`
4/4, with reduced-motion traces reporting `drift_x`/`drift_y`/`breath` exactly
`0` and ambience-on traces showing drift rising with the fixed-step clock
(0.19/0.10 px at 4 frames to 1.24/0.65 px at 40 frames at 120 Hz). Evidence,
including 1080p/2160p captures, is in
`.ai/evidence/20261125-widget-tile-motion/`. These remain Mesa `llvmpipe`
software-renderer observations: no GPU timing or C3 acceptance is claimed.

### November 25 cache and lifecycle increment

Three bounded changes, no visual change: a service reply now invalidates the
cached home layer only when it carries the app inventory (widgets, panels and
toasts are painted live, so a processor/settings/device/input reply no longer
spends a full-drawable repaint); the clock change test moved from every draw to
once per simulated second in `update()`; and `SDL_APP_TERMINATING` now flushes
the ordered settings writer before exit while `SDL_APP_LOWMEMORY` releases the
derived GPU caches (text textures, metrics, ellipsis and the home layer) through
`Renderer::release_caches()`, leaving prepared artwork resident so the render
thread never re-reads files. `home_cache_renderer_checks` now asserts the
release empties both caches, that the next frame repaints rather than hits, and
that the rebuilt layer still matches a direct draw (max channel delta 2).
Desktop verification: desktop build clean, CTest 12/12, ASan/UBSan UI build
CTest 12/12, `native_motion_qa.py` 35/35, `native_qa.py` 56/56,
`native_pacing_qa.py` 4/4. The same-day performance, TV-integration and
webOS-essentials audit, with measured per-frame draw counts and the remaining
priorities, is [NATIVE-AUDIT-2026-11-25.md](NATIVE-AUDIT-2026-11-25.md).

## 2026-11-26 speed pass (multi-model, host evidence)

A second GLM 5.3 / Kimi K3.1 Pro review round plus ERNIE and HY4 web research
produced four low-risk changes: the clock is a live label driven by the cached
once-per-second string (a minute tick no longer repaints the whole Home FBO),
`glGetIntegerv(FRAMEBUFFER_BINDING/VIEWPORT)` runs only on FBO-leaving paths,
metric/ellipsis caches evict least-recently-used instead of smallest-key, and
`FrameStats::summary()` is recomputed every 15 rendered frames instead of per
frame. A deadline-wait rounding change was tried and reverted: it broke the
wall-clock pacing QA gate, so the original truncated-millisecond wait remains.

Measured host Mesa llvmpipe evidence after the change: CTest 12/12 Release,
12/12 UI ASan/UBSan, `native_qa.py` 56/56, `native_motion_qa.py` 35/35,
`native_pacing_qa.py` 4/4. The `game-home` report records 87 draw calls versus
236 in the older catalog baseline; that comparison spans earlier cache work as
well as this pass, so it is not an isolated delta. No physical C3 GPU timing,
120 Hz claim or new TV install was part of this pass. Full receipts:
`.ai/evidence/20261126-perf-review/SPEED-SYNTHESIS.md`.

No sample app is labelled installed on the TV. If private discovery fails, explicitly
unverified configured shortcuts remain. Desktop metrics refer to the host, not the C3.
