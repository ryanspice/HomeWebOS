# Resolution contract for the render/resolve path

Date: 2026-11-25. Written for the `render_resolution` preference work in the parallel
thread. Scope: what already follows the drawable, what has fixed bounds, and what has to
move together. Files in this thread's ownership: `src/render.cpp`,
`include/spice/render.hpp`, `src/ui.cpp`, `include/spice/ui.hpp`.

## 1. Already resolution-agnostic — do not change these

- **The design canvas stays 1920x1080** (`include/spice/core.hpp:13`). Every node rect,
  hero block, widget rect, ambience particle and focus outline is in design units, and the
  keyboard/pointer policy assumes it.
- **Scale comes from the drawable, not from a constant.** `Viewport::make`
  (`src/core.cpp:17-26`) computes `scale = min(drawable_w/1920, drawable_h/1080)` and the
  letterbox `left`/`top`. It is recomputed from `GL_GetDrawableSize`
  (`src/main.cpp:319`, `src/main.cpp:581`) and on `RESIZED`/`SIZE_CHANGED` and fullscreen
  flips (`src/main.cpp:468`, `src/main.cpp:481`), so a mode change mid-run is picked up
  without a restart.
- **Hit-testing survives the override.** `Viewport::pointer` (`src/core.cpp:28-33`) maps
  window coordinates through `drawable/window` into design units, then rejects anything
  outside 1920x1080 — correct for HiDPI, letterboxing and a granted-but-different drawable.
- **glViewport and the clear cover the whole drawable**; the content region is
  `DesignWidth*scale x DesignHeight*scale` at `(left,top)` (`src/render.cpp:430-433`). A
  non-16:9 drawable therefore shows the clear colour in its bars instead of garbage.
- **Screenshots read `view_.drawable_w/h`** (`src/render.cpp:1008`), so QA captures exactly
  what was presented, bars included.
- **Antialiasing is in device pixels**: radius/thickness are divided by `view_.scale`
  (`src/render.cpp:616`, `src/render.cpp:852`).
- **Text follows the scale.** `density()` (`src/render.cpp:599-601`) is
  `round(clamp(scale,.5,4)*8)/8`; glyphs rasterize at `size*density`
  (`src/render.cpp:698`), and `width()`/ellipsis return design units again
  (`src/render.cpp:790`). `text_key` includes the pixel size (`src/render.cpp:603`), so a
  mid-run switch cannot mix densities. Exposed as `Renderer::text_density()` for the report.

## 2. Fixed bounds the override must respect

1. **Home cache budget: 32 MiB** (`Renderer::HomeCacheBudgetBytes`,
   `include/spice/render.hpp:165`). The layer is `1920*scale x 1080*scale`
   (`src/render.cpp:559`): 1080p = 7.91 MiB, 1440p = 14.06 MiB, 2160p = 31.64 MiB.
   Measured today on the desktop build (`--view home`, one repaint, cache on):

   | request | drawable | scale | text density | cache | cached |
   |---|---|---|---|---|---|
   | 1920x1080 | 1920x1080 | 1.0000 | 1.000 | 7.91 MiB | yes |
   | 2560x1440 | 2560x1440 | 1.3333 | 1.375 | 14.06 MiB | yes |
   | 3840x2160 | 3840x2160 | 2.0000 | 2.000 | 31.64 MiB | yes |
   | 4096x2160 | 4096x2160 | 2.0000 | 2.000 | 31.64 MiB | yes (128 px bars) |

   Threshold: a content height above **2172 px** (scale > ~2.011) or width above 3862 px
   turns the cache off, and the home view then repaints fully every frame while ambience
   animates. **2160-line panels cache; 2880-line panels do not.**
   `Renderer::home_cache_fits()` (`include/spice/render.hpp:168`) reports the decision for
   the current drawable; `tests/home_cache_renderer_checks.cpp` locks the contract at
   1080p/1440p/2160p/4096x2160 = cached, 5120x2880 = uncached.
2. **GPU texture limit**, queried at startup (`src/render.cpp:109`); above it the cache is
   also skipped. Every webOS GPU in this fleet is 8192 or larger.
3. **Prepared artwork admission — three gates that must move together:**
   - `tools/prepare_tv_data.py:73` (≤ 1920x1080) — pipeline thread
   - `src/prepared.cpp:108-112` (manifest defaults 1920x1080, 48 MiB RGBA total) — shared
   - `src/render.cpp:373` (raised today to 3840x2160, inert until the other two move)

   The stock `herobanner` is declared 3840x936, so all three need at least 3840x2160 before
   stock art can ship. Icons keep their ≤ 256 bound (`src/prepared.cpp:110`).
4. **Text cache: 384 entries / 24 MiB by bytes** (`src/render.cpp:535`). At density 2 a
   string costs 4x the 1080p bytes, so the effective entry count drops ~4x. The home view
   needs ~45 strings, which is fine; long library/pins lists evict more at 2160p.
5. **Density clamp and quantization** (`src/render.cpp:600`): scale is clamped to
   [0.5, 4] and rounded to eighths — 1440p = 1.375, 2160p = 2.0, 8K = 4.0. Non-standard
   modes round (1.3333 → 1.375), which is correct but must not be "fixed" by
   hand-computing a scale.

## 3. Notes for the preference plumbing (parallel thread's files)

- On webOS the window is created at `opt.width x opt.height` **without**
  `SDL_WINDOW_FULLSCREEN` (`src/main.cpp:236`); the compositor decides the real card size
  and the loop re-reads the drawable every frame (`src/main.cpp:581`). A granted size that
  differs from the requested one already works.
- So `render_resolution` should choose `opt.width/opt.height` **before window creation**
  (panel mode for `"max"`), and must not touch `DesignWidth`/`DesignHeight`.
- A 4096x2160 drawable letterboxes to 3840x2160 content with 128 px bars per side, and the
  cache still fits; no special case is needed.
- Fields worth surfacing in `report`: `home_cache_fits()`, `home_cache_bytes()` (exists),
  `text_density()`, `viewport.scale` (exists). Nothing else is needed to prove the override
  took effect.
