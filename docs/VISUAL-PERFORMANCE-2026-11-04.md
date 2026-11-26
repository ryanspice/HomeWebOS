# Native visual and performance pass — 2026-11-04

The working C++ prototype now has a coordinated visual pass, cheaper text layout,
single-pass backdrop lighting, more explicit performance instrumentation and an
experimental ordered batch renderer. Two requested `gpt-6.1-terra` HIGH tasks produced
scoped code; root integrated their work and the justified specialist ideas. The
updated Linux executable is `build/desktop/spice-home-cpp`. This is a native desktop
result; the 4K / high-DPI / 240 Hz target still needs real hardware qualification.

## Appearance and interaction

Home has a restrained active-tab underline, canvas rules and a hero accent. Control
Center and settings use darker surfaces, consistent edges and clearer dividers.
The concept collection shares that treatment, with bounded static detail in the
full arcade/orbit scene families. The welcome copy is now “Ready when you are.”
No fonts, dependencies or bitmap assets were added. Hero source artwork remains
1600×900 and covers remain 500×500; native 4K text and shapes do not add image detail.

The fourth-column right-leaning 4/3/2 staircase, five bottom cards, focus/node IDs,
input geometry and existing motion authority remain. Focus still settles in 140 ms,
artwork uses 300 ms coalesced segments and panels use the existing 200 ms entrance.
Reduced motion, moving hit bounds, hidden/focus suspension and exact settlement
retain their prior checks. New decoration is static and bounded, with no timer.

These are current native framebuffer captures from the same accepted matrix:

| Home | Control Center |
|---|---|
| ![Native Home](../evidence/visual-performance/matrix-accepted/immediate/home.png) | ![Native Control Center](../evidence/visual-performance/matrix-accepted/immediate/control.png) |

| Design collection | Settings |
|---|---|
| ![Native concept collection](../evidence/visual-performance/matrix-accepted/immediate/catalog.png) | ![Native settings](../evidence/visual-performance/matrix-accepted/immediate/settings.png) |

The captures prove native output and let a person inspect the result. They do not
constitute human acceptance or TV-distance readability approval. Desktop samples,
real host counters and unavailable TV values retain their existing labels.

## Rendering and layout changes

- **Measurement no longer rasterizes text.** FreeType glyph metrics and kerning supply
  widths. Density is canonicalized to eighths, and the cache key includes the actual
  font pixel size and canonical density. Native checks compare measured and raster
  advances across sizes/densities, including UTF-8 and negative bearings.
- **Caches have explicit limits.** Text textures admit/evict before upload, capped at
  384 entries / 24 MiB. The metrics map is capped at 1,024 entries and ellipsis results
  at 512. Ellipsis keys include exact width bits and canonical text/raster parameters.
  Long keys are not cached; reused raster scratch and texture-size checks bound work.
  These limits describe these caches, not all application or driver memory.
- **Backdrop lighting is fused.** Existing source/target artwork, lighting scrims and
  tint are composed in one shader pass without an extra texture or render target.
  It reproduces the legacy RGB and SRC_ALPHA alpha equation before final conversion.
  `--legacy-background` keeps the old path for comparison. The test found and prevented
  an initial alpha-only regression; that failed intermediate evidence is retained.
- **Shader precision follows capability.** GLES2 queries fragment high-float support,
  chooses matching highp varyings when supported and retains a mediump fallback.
  Reports state the choice. Desktop GL retains its float path. No hardware speed or
  universal image-quality improvement is inferred from the precision setting.
- **Batching preserves painter order.** `--batch-renderer` groups adjacent compatible
  texture runs; it never globally sorts translucent UI. Transform and opacity values
  are captured on enqueue. Capacity, texture changes, upload/eviction/deletion,
  screenshots and frame boundaries flush deliberately. A fixed 512-quad CPU page and
  VBO page each hold 221,184 bytes; driver orphan backing is not quantified. The batch
  vertex is 18 floats versus 4 in the default path, a 4.5× payload per primitive.
  Batching therefore stays opt-in until representative target-GPU timing is available.

Root rejected an extra global canvas scale suggested by Crouton because the existing
viewport/drawable/pointer mapping already scales correctly. It would have scaled
twice. Root also rejected partial-width scrims that create a visible seam. The
implemented backdrop follows the useful overdraw idea with separately checked math.

## Measured software work

The text probe executes the same 50 measurements and five ellipsis operations with
the pre-pass and current binaries. Both report width checksum **48,540**. Before,
measurement added **186 texture entries / 5,236,044 RGBA bytes**; afterward it adds
**zero entries / zero bytes**. The recorded host times are single software samples,
not a latency benchmark. Evidence: `text-measurement-before.json` and
`text-measurement-after.json` under `evidence/visual-performance/`.

The mixed shader/alpha/transform/overflow/eviction/temporary-text fixture records
**1,247 logical primitives**, **1,247 → 428 submissions**, **0 changed channels** and
**0 maximum channel delta**, independently at 1280×720 and 3840×2160. The automated
guard allows a small stated tolerance; these actual recorded runs were byte-exact.
See `batch-fixture-1.log`, `batch-fixture-2160.log` and the final CTest output.

Backdrop-only checks record **4 → 1** steady submissions and **5 → 1** during the
sampled crossfade/end state. Interior RGBA differences are at most **2/255** from
multi-pass rounding. The outer two-pixel band differs by up to **12/255** because
the old layers independently antialiased their edges. This is distinct from the
batch-equivalence result and is not described as byte-identical.

The accepted 4K matrix uses six scenes × four paths, 48 fixed animation steps each.
The table shows final-frame GL submissions, not FPS or GPU duration:

| Scene | Pre-pass binary | Current default | Current batch | Current legacy backdrop |
|---|---:|---:|---:|---:|
| Home | 235 | 244 | 90 | 247 |
| Concept collection | 233 | 251 | 92 | 251 |
| Inspector | 68 | 71 | 47 | 71 |
| Search keyboard | 96 | 100 | 91 | 100 |
| Control Center | 332 | 345 | 107 | 348 |
| Settings | 331 | 348 | 110 | 351 |

Richer styling adds primitives relative to the pre-pass UI. The meaningful batch
comparison is current default against current batch, using the same current UI.
Host CPU text and clock values can vary slightly between captures. Logical texture
byte counts are not a physical VRAM measurement. The inspector's recorded cached
RGBA total falls from 60,149,248 to 56,224,380 bytes after eliminating measurement
textures. Source images were not enlarged or recompressed.

## Timing and remaining performance gates

Reports now separate input/events, update, rendered-loop CPU work, render submission,
swap, software pacing wait, idle wait and framebuffer capture. Rolling 240-sample
summaries include p50/p95/p99/max. Active long intervals above 250 ms are retained;
intentional focus/idle/capture discontinuities have separate accounting. Final
reports refresh samples at exit. Build configuration, compiler, sanitizer mode,
requested ceiling, VSync and accepted swap request are recorded.

`gpu_time_available=false` and `physical_presentation_verified=false` are explicit.
No timer query, GPU trace, physical 240 Hz presentation, target ARM compile, TV install,
Magic Remote validation, thermal run or human motion/readability acceptance occurred.
Xvfb/llvmpipe and fixed animation time cannot establish a 4.167 ms hardware budget.
The existing synchronous settings `Deck::save()` / atomic file write also remains
known interaction-path debt; this pass does not claim that all file I/O was removed.

## Verification on the final code

Environment: `WebOSBuild` WSL, actual development headers, Mesa 25.2.8 GLES2,
llvmpipe LLVM 20.1.2 and Xvfb. Code freeze: 29 code/test/build files in
`.ai/runs/20261104-visual-performance/code-freeze.json`; all matched at closeout.
Release executable SHA-256:
`e77fd6a3d4f2e1ee9bdd1edbf2f750d74c8f902c9d07d610eeba0318cff92685`.

| Check | Result | Evidence under `evidence/visual-performance/` |
|---|---|---|
| Strict Release configure/build | PASS | `build-release-final.log` |
| Strict ASan/UBSan configure/build | PASS | `build-sanitize-final.log` |
| Release CTest | 4/4 | `ctest-release-final.log` |
| Sanitized CTest | 4/4 | `ctest-sanitize-final.log` |
| Core scenarios, Release and sanitized | 53/53 each | `core-release-final.log`, `core-sanitize-final.log` |
| Default native UI + motion | 56/56 + 33/33 | `final-native/native-qa.json`, `final-motion/motion-qa.json` |
| Batch native UI + motion | 56/56 + 33/33 | `final-batched-native/native-qa.json`, `final-batched-motion/motion-qa.json` |
| Batch sanitized UI motion | 33/33 | `final-sanitized-motion/motion-qa.json` |
| Native 4K comparison captures | 24/24 completed | `matrix-accepted/matrix.json` |

Core sanitizers keep ordinary leak detection. Native renderer and UI motion checks
explicitly disable leak detection and retain address/undefined-behavior checks. The
previously isolated **1,032-byte SDL/libdbus startup allocation leaves full UI LSAN
open**. Intermediate `matrix/` and `matrix-final/` evidence predates the corrected
backdrop alpha math; use `matrix-accepted/` for the final comparison.

## Specialist synthesis and limits

Both requested Terra HIGH producers completed code. Fugu supplied **three accepted
advisory reports**: OpenRouter Crouton, Inception Mercury and TokenHarbor's exact
`deepseek-v4.1-flash:free` route. Primary packets contained frozen pre-pass source
excerpts and a repository map. They did not inspect native images, compile code,
read the entire integrated repository or prove hardware behavior. Root owns those
integration and verification claims.

Crouton's overdraw/cache ideas informed the checked one-pass backdrop and bounded
ellipsis cache. Mercury's typing-rebuild coalescing remains a measured-work candidate;
the current 50-entry collection and input ordering did not justify a new invalidation
schedule. Its 185 ms panel proposal would replace an already eased/tested 200 ms path
without visual evidence. TokenHarbor's ordered command proposal overlaps the implemented
batch path; a second command layer was unnecessary. A glyph atlas and extra scene-detail
tiers are deferred until counters show a working-set or painter cost that warrants them.

NVIDIA GLM 5.3 Flash ended with HTTP 504; CleanAPIs account 03 with HTTP 401. InferX
and CleanAPIs accounts 01/02 ended with `TextBoundaryDenied`, without a proven cause.
Qwen's proposal and the separate final integrated-source review were truncated and
remain unaccepted, including the latter's requested 32,768-token cap. No findings
were extracted from failed/truncated responses. There was no provider substitution,
purchase, top-up or automatic extra wave. The final independent review gate remains
open; passing root checks does not turn that failed review into a pass.

The owner-reported 500,000 tokens on each existing CleanAPIs account is recorded as
an allowance, not a verified balance, context window or proof of entitlement. See
[the full synthesis and route receipts](../.ai/runs/20261104-visual-performance/SYNTHESIS.md)
for exact profiles, dispatch history, accepted report hashes, transport qualification,
known usage and expiry. Mercury used the explicitly allowed existing Inception route;
TokenRouter was not used.

## Reproduce and recover

Inside `WebOSBuild`, from `/mnt/b/Dev/webos/spice-home-cpp`:

```bash
cmake -S . -B build/desktop -G Ninja -DCMAKE_BUILD_TYPE=Release -DSPICE_STRICT=ON
cmake --build build/desktop --parallel 4
ctest --test-dir build/desktop --output-on-failure
cmake -S . -B build/san -G Ninja -DCMAKE_BUILD_TYPE=Debug -DSPICE_STRICT=ON -DSPICE_SANITIZE=ON
cmake --build build/san --parallel 4
ctest --test-dir build/san --output-on-failure

LIBGL_ALWAYS_SOFTWARE=1 MESA_GLES_VERSION_OVERRIDE=2.0 \
xvfb-run -a -s '-screen 0 3840x2160x24' \
python3 tests/visual_performance_matrix.py --binary build/desktop/spice-home-cpp \
  --baseline /mnt/b/Dev/webos/_backups/spice-home-cpp/20261104-visual-performance/binary/desktop-before \
  --out build/visual-performance-reproduction
```

For interactive use, start the rebuilt executable with `--assets ./assets --width 3840
--height 2160 --fps 240`, and add `--batch-renderer` to compare batching. `--fps` is
the software ceiling if VSync is unavailable; it does not set panel refresh. Do not use
`--step-ms` for a throughput benchmark.

No usable project Git HEAD exists. No commit, push, publication, TV install or changes
to the Rust checkout were performed. Recovery uses the 65-file pre-pass SHA manifest
and byte-verified copies at `B:\Dev\webos\_backups\spice-home-cpp\20261104-visual-performance`.
The source and binary change receipt is `evidence/visual-performance/change-receipt.json`.
`Restore-VisualPerformance.ps1` in that directory checks every current after-hash and
every original copy before any restoration; check-only is its default. Any later edit
blocks recovery. Evidence is retained. Rebuild after a source restoration.

Fugu runtime recovery is separate and described in the synthesis. App recovery does
not change profiles, credentials, shared Python, adapters or retained provider receipts.
