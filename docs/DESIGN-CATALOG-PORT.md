# Design catalog port: direction, provenance and pushback

Status: working notes, 2026-11-04. This is a plan of record, not a deployment claim.

## Direction

The project target is the **C++20 / SDL2 / raw GLES2 deck** (`com.spice.home.cpp`), not a
Rust continuation. The Rust `spice-home` work remains the source of the 50 authored
design concepts, its tests and its catalog, and it keeps running as the deployed app
until the C++ path passes real device gates.

The 50 designs have **not been reviewed** as product designs. They are treated here as a
**POC and a porting guideline**: a structured, validated set of concept metadata that
tells us what each destination wants to show, not a finished catalog to ship.

## Why the earlier "stay Rust" note is superseded

`docs/CPP-REWRITE-TRADEOFF.md` in the Rust checkout recommended staying in Rust unless
profiling showed a bottleneck. That reasoning still holds for cost, but the owner chose
the C++ deck for the long-term render and control path. The decision is theirs; the job
here is to make it correct rather than to re-argue it.

What that decision costs, and how each cost is handled:

- **The 50-design catalog and 64 Rust tests** - catalog copied byte-for-byte and parsed
  by a C++ validator that ships in `spice_core`, so it is available to host-only tests.
- **The ARM IPK pipeline** - the C++ project already has `scripts/build-webos.sh` and a
  cross-build gate; it stays a gate, not a claim.
- **The launcher identity** - unchanged. `com.spice.home` and the TV install are not
  touched. The C++ app keeps its own id.

## Provenance

- Archive: `spice-home-cpp-v0.1.0.zip`,
  SHA-256 `fd59fb2684348aea20de53062c505477a28ee814f9c4c983f73255d9300502b4`.
- Linux binary: SHA-256
  `fde28cbef056039ad4d351e8eef2cb04879443e0009f93a817934e01a8e29ae2`, matches the shipped
  `SHA256SUMS.txt`.
- Working tree: `B:\Dev\webos\spice-home-cpp`, a copy of the extracted
  `B:\Dev\webos\_imports\spice-home-cpp-v0.1.0\spice-home-cpp`. The import stays as the
  immutable reference.
- Catalog: `assets/designs/catalog.json`, copied unmodified from
  `spice-home/assets/app-designs/catalog.json` (86,803 bytes, schema 1, version 0.3.5).

## What is implemented

- `src/designs.cpp` / `include/spice/designs.hpp`: bounded, validating loader for the
  catalog. Rejects wrong schema, a missing design-only flag, duplicate keys, malformed
  `#rrggbb` colours, and out-of-range counts or string lengths.
- `src/ui_designs.cpp`: native GLES2 `designs` view - category strip with counts, text
  search, 15-per-page grid, paging, focus ring, and a per-design inspector (hero line,
  tagline, description, simulated metadata, example cards with progress, expected fields,
  previous/next stepping inside the current filter, position counter). Settings opens
  the collection; a native search keyboard supports D-pad/OK and pointer input.
  Category focus, paging boundaries, clear/reset actions and inspector/launcher return
  focus are preserved. The bounded catalog is loaded at startup, before interaction.
- `src/ui_scenes.cpp`: a reduced per-family **thumbnail motif** (~6 draws) for the grid
  tiles and one bounded procedural painter for each of the 15 scene families
  (broadcast, guide, creator, arcade, library, cinema, settings, browser, sport, input,
  devices, orbit, music, gallery, telemetry). The grid uses the motif, the inspector uses
  the full painter; both are drawn only with the native primitive renderer, so there is
  no image, decoder, download or per-design texture. Art is explicitly fictional and
  never live data.
- `--view designs` on the CLI; `designs` is in the accepted view list and the help text.
- No entry is launchable. Activating a tile opens the inspector; it never resolves to a
  package id, URL, command or device action.

## Evidence run here

All captured from the native executable's own `glReadPixels` / state report, not a
browser. Mesa llvmpipe under Xvfb, i.e. desktop software GL, not the C3.

| Check | Command | Result |
| --- | --- | --- |
| Core tests | `build/desktop/spice_core_tests` | 44 passed / 0 failed |
| Core tests, sanitizers | `build/san/spice_core_tests` (`SPICE_SANITIZE=ON`) | 44 passed / 0 failed, no ASan/UBSan report |
| Native scenarios | `tests/native_qa.py` | 56 passed, including 19 new entry/search/focus/recovery/paging/return scenarios |
| Catalog load | `--view designs --report` | `designs: 50`, `design_category: All` |
| Filter | click `Streaming` chip | `design_category: Streaming`, 15 tiles on page 1 of 2 |
| Search | text `orbit` | 3 matches (Disney+, Stremio, Moonlight) by name/scene/alias |
| Inspector | click first tile | `design_selected: live-tv` |
| Inspector step | click `Next` twice | `design_selected` advances, inspector stays open |
| Scene art | tile + inspector captures | 15 distinct family painters + reduced tile motifs, no image/decode path |
| Draw calls | `--report` per capture | catalog 233/frame, Streaming 216, Live TV inspector 68, search keyboard 96; covered Home painting removed |

Current captures and per-scenario reports: `evidence/catalog-ux/native/`. Earlier scene
family captures remain in `evidence/designs-browser/`. The current full UI
LeakSanitizer gate is open due to a 1,032-byte libdbus allocation also reproduced by
startup alone; address/undefined-behavior UI checks pass with leak detection disabled.
See [UI-UX-2026-11-04.md](UI-UX-2026-11-04.md) for the exact evidence boundaries.

## Pushback: what "raw webOS, max FPS forever" does and does not mean

1. **"Most FPS at all times forever" is not a guarantee.** The ceiling is the C3's GPU,
   the display mode and the compositor, and it is unmeasured here. The achievable,
   testable goal is a **frame budget**: a fixed 8.33 ms (120 Hz) or 16.67 ms (60 Hz)
   per-frame allowance, one static full-screen FBO, batched draws, no allocation or
   file/network/subprocess work on the render thread, and adaptive quality that degrades
   deterministically rather than dropping frames. We measure p95, not a promise.
   The current `designs` grid issues 233 draw calls per frame after eliminating the
   covered Home drawing pass. Text strings and primitive quads still submit separately.
   Combining compatible submissions while preserving painter order is the remaining
   batching work; draw-count reduction is not a measured TV FPS result.
2. **"Raw access to webOS entirely and completely" needs one boundary.** Inside the
   app's own jail we can and should speak LS2 directly (a native binding instead of
   `luna-send` argv) and read only what our ACL allows; that is a real, allowed upgrade
   and it is currently listed as **not implemented** in `PARITY.md`. Bypassing ACLs,
   installing root services, replacing the system Home, or turning off updates is a
   different risk class - bricking and lost updates - and this project's own
   `AGENTS.md` explicitly excludes it. It needs a separate, explicit decision; this
   document does not authorize it.
3. **A desktop screenshot is not TV proof.** Inline video, HDMI planes, real refresh
   and armv7 ABI still need the device. Nothing here changes that.

## Remaining milestones

1. **Batch the renderer.** One dynamic vertex buffer + order-preserving batches; reuse the
   static chrome into a cached layer. Target: `designs` and `home` under ~10 draw calls.
   The covered Home painting has been removed; the catalog remains at 233 draws, so
   batching is still a meaningful next renderer milestone.
2. **Direct LS2 binding.** Replace the `luna-send` subprocess with an in-process,
   allowlisted LS2 call path behind the existing opt-in, keeping the same size/deadline
   limits and the render-thread ban.
3. **Device gate.** Cross-build armv7 with real SDL-webOS/FreeType headers, install as
   `com.spice.home.cpp` alongside the current Home, and measure frame intervals and
   refresh on the C3.
