# Verification — November 5, 2026

## Owner feedback and architecture follow-up

The owner confirms basic arrows and Back work, but rejects overall r13 speed,
responsiveness, scrolling and animation coverage. Smoothness is not accepted.
The [native performance/language decision](NATIVE-PERFORMANCE-ARCHITECTURE-2026-11-05.md)
and [implementation plan](../.thoughts/native-webos-performance-plan.md) supersede
any interpretation of the settled-scene probe as a fluid-UX verdict. This research
pass changed no application code or TV setting. Earlier acceptance/source hashes
remain immutable phase records; this documentation update is a later revision.

## Current C3 installation

At the recorded installation check, the real ELF32 ARM r13 payload's live process
hash matched the verified package and the TV reported the app focused fullscreen.
The later read-only research check found SkiFree foreground and left it undisturbed.
Native default Home,
existing Home-key bindings and boot hooks target it; Input Hook reload and manual
late-hook execution passed. Ordinary diagnostics showed a 3840x2160 drawable / 2x
scale. Compositor capture is still 1920x1080. Quick Settings focus moved to its
overlay and returned to the app. Physical input and exact update-popup acceptance
remain open.

Processor Deck is disabled on the TV, with no Home/Control Center targets or CPU/RAM
requests. The service worker starts on demand. Normal render profiling is off.
The final 4K renderer probe records zero app service requests, no service worker,
17.245 ms median / 27.842 ms p95 active-present interval, and a 33,177,600-byte Home
cache. That bounded jail child did not inherit SAM registration; its graphics
timings are separate from production permissions and physical latency/presentation.

Strict Release and ASan/UBSan CTest pass 12/12, including cache RGBA/reuse/resize/
budget checks and Back/disabled-processor checks. UI leak detection is explicitly
off in native ASan checks; core leak checks and the remaining UI LSAN distinction
are preserved. Full results, artifact hashes, recovery and remaining features are
in [the deployment report](TV-INSTALL-2026-11-05.md) and `.ai/evidence/20261105-tv-fixes/`.
The sections below retain the earlier phase results on their original artifacts.

## Current fleet integration

Strict Release and ASan/UBSan builds pass **11/11 CTest entries**. The integrated
Release native suites pass 56 UI, 33 motion, 14 inventory, 12 prepared-data,
16 new navigation/settings, 9 theme and 4 pacing checks. The selected sanitized
native suites pass 33 motion, 14 inventory, 16 navigation/settings, 9 theme and
4 pacing checks. They are desktop GLES2/Mesa/Xvfb evidence. Core leak checks run;
UI renderer/native ASan checks explicitly disable leak detection. Physical 240 Hz,
GPU timing, real controller/remote, full UI LSAN and ARM/TV execution remain open.

Windows Python passes 14 theme-preparation and 18 profile-preparation tests.
After the lead's avatar budget fix, the profile dashboard passes 26 isolated
Edge interactions, including actual bounded PNG/JPEG/WebP decoding and oversized/
animated image rejection. No public account statistics were verified. One Fugu
review returned only an inventory update and is not a completed review.

See [the full fleet implementation report](FEATURE-PARITY-FLEET-2026-11-05.md)
for immutable logs/captures, source/binary hashes, retained failing qualifications,
model-task receipts, recovery and the remaining feature parity gates.

## Current refresh/review working tree

The bottom refresh shortcut, candidate filter, stable pin identity and scan interaction
guards are implemented. The integrated Release native matrix passes **14/14** cases,
including new-item pinning, paging/filter pin targets, intervening navigation, persistent
failure and a full-length selected ID/version. Strict CTest adds the inventory suite
and passes **6/6 in Release and ASan/UBSan**. The integrated refresh matrix passes
**14/14 in both builds**; existing native UI, motion and prepared-data matrices pass
**56/56, 33/33 and 12/12**. Current completion receipts live in `evidence/refresh-scan/`.
UI sanitizer runs explicitly disable leak detection; the full UI LSAN gate stays open.

Exactly four requested `gpt-6.1-terra` / MAX tasks completed: three PS5 comparisons
and one native Darkatek7/ProsperoThemes investigation. Their 46-file source freeze
and preimages are retained; parent corrections are a later version. See
[the synthesis](PS5-THEMES-SYNTHESIS-2026-11-04.md). These research reports are not
PS5 timing measurements or proof of custom-theme runtime support.

## Current offline-preparation working tree

The preparation tool was run against the configured C3 using its existing Developer
Mode connection. The supported SDK `dev/listApps` route reports 21 app records; the
prepared bundle contains 21 launchable records and 26 graphics. TV model, firmware,
SDK and UHD information were read successfully. LG Home's protected package and
launch-point inventory remain unavailable on this connection and are recorded as
such. No app was installed/launched and no TV file or CLI configuration was changed.

Release and ASan/UBSan builds pass 5/5 CTest checks, including 53 core scenarios,
13 prepared-data scenarios and three renderer checks. The existing native suite
passes 56/56 and motion passes 33/33. The new prepared-data native suite passes
12/12 in Release and ASan/UBSan with UI leak detection disabled. The Python
preparation suite passes 14/14 and the Node transport boundary suite passes 13/13.

See [OFFLINE-PREPARATION-2026-11-04.md](OFFLINE-PREPARATION-2026-11-04.md) and
`evidence/offline-preparation/`. The readonly TV metadata collection is separate
from native app execution on the TV. ARM/TV app launch, physical presentation and
the existing full UI LSAN gate remain open for this C++ application.

## Current visual/performance working tree

The current Release and ASan/UBSan builds each pass **4/4 CTest checks**, including
the **53/53 core scenarios** and three native renderer checks. Both the default and
experimental batch paths pass **56/56 native UI scenarios** and **33/33 motion
checks**. The batch motion suite also passes ASan/UBSan with UI leak detection
explicitly disabled. The existing full UI LeakSanitizer gate below remains open.

The comparison matrix contains **24 native 3840×2160 captures** across the original
pre-pass binary, current default, current batch and current legacy-backdrop paths.
These use fixed animation time and Mesa llvmpipe. They establish rendering behavior
and software work counts, not hardware throughput, GPU time or physical 240 Hz.
The separate mixed-renderer fixture recorded zero changed channels between immediate
and batch output at 720p and 4K. Backdrop fusion has a distinct rounding/edge tolerance
and is not claimed byte-identical to the old layered path.

See [VISUAL-PERFORMANCE-2026-11-04.md](VISUAL-PERFORMANCE-2026-11-04.md) for changes,
exact checks, native images, provider outcomes, reproduction and guarded recovery.
The current executable is `build/desktop/spice-home-cpp`, SHA-256
`e77fd6a3d4f2e1ee9bdd1edbf2f750d74c8f902c9d07d610eeba0318cff92685`.
The packaged original `bin/` executable and its delivery evidence remain unchanged.

## Current working-tree UI/UX follow-up

The latest motion slice passes **53/53 Release and sanitizer core scenarios**, the
existing **56/56 native UI scenarios**, and **33/33 deterministic native motion checks**.
The 33-check UI matrix also passes ASan/UBSan with leak detection disabled. A preserved
pre-fix executable records 13 passed / 20 failed using that same final driver.
See [MOTION-UX-2026-11-04.md](MOTION-UX-2026-11-04.md) for shared moving hit bounds,
finite focus, artwork coalescing, lifecycle behavior, independent Fugu review, pixel
comparison, exact reproduction and recovery. Evidence is in `evidence/motion-ux/`.
Fixed-step animation is explicitly labelled QA and does not establish physical refresh.

The paragraph below records the earlier collection follow-up, before the motion slice.

The original v0.1.0 delivery evidence below remains the archive baseline. The newer
`B:\Dev\webos\spice-home-cpp` working tree was rebuilt and exercised on `WebOSBuild`
WSL with complete development headers, Xvfb and Mesa 25.2.8 / llvmpipe LLVM 20.1.2.
The Release and sanitizer core suites pass 44/44; the Release native suite passes
56/56, including 19 new collection interaction cases and 720p/2160p search captures.

The full UI LeakSanitizer gate is **open**: a 1,032-byte libdbus allocation reproduces
on startup alone. The combined search/reset/inspector/return sequence passes address
and undefined-behavior sanitizers with leak detection explicitly disabled. Those are
separate results, not a clean full UI leak check.

See [UI-UX-2026-11-04.md](UI-UX-2026-11-04.md) for the changes, exact evidence, native
visual checks, reproduction steps and remaining hardware/human gates. Fresh logs,
captures and candidate hashes are under `evidence/catalog-ux/`. The original packaged
binary hash below does not identify this newer build.

## Classification

**Compiled and executed native Linux desktop prototype. webOS source candidate only.**
The delivered executable is an x86_64 ELF, not an ARM binary, Windows EXE or webOS IPK.
No TV, Pi, user's source checkout, credentials or installed applications were contacted.
The only companion HTTP tests used local test fixtures.

## Actual environment

- Debian 13, x86_64; GCC 14.2.0; CMake 3.31.6; Ninja.
- SDL2 2.32.4 and FreeType runtime libraries. Complete SDL/FreeType development headers
  were unavailable, so the documented **desktop-only public ABI declaration fallback**
  was used. Target builds explicitly reject that fallback.
- JSON-C 0.18, libpng and libcurl supplied by the build environment.
- Mesa 25.0.7-2, llvmpipe (LLVM 19.1.7, 256 bits), Xvfb.
- GLES runs forced `MESA_GLES_VERSION_OVERRIDE=2.0` and report **OpenGL ES 2.0**.
- Swap-interval requests were unavailable under this test display; captures used the
  explicit software pacing path. Results are not physical refresh-rate measurements.

The Linux binary requires the installed OS dependencies shown in
`evidence/build/binary-info.txt`. Its symbol requirements include GLIBC_2.38 and
GLIBCXX_3.4.32; rebuild for older distributions rather than replacing their system libraries.
No runtime shared libraries or fonts are redistributed.

## Results

| Gate | Result | Evidence |
|---|---|---|
| Release C++20 configure/compile/link, warnings as errors | PASS | evidence/build/configure-release.log, build-release.log |
| CTest native core | PASS | evidence/build/ctest-release.log |
| Core assertions/scenarios | **44/44 PASS** | evidence/build/core-tests.log |
| Core AddressSanitizer + UndefinedBehaviorSanitizer | **44/44 PASS** | evidence/build/asan-ubsan-tests.log |
| Actual Release native executable UI scenarios | **28/28 PASS** | evidence/native/native-qa.json, evidence/build/native-qa.log |
| Agent inventory/migration tests | **12/12 PASS** | evidence/build/adapter-tests.log |
| Existing RetroPie companion API/security tests | **6/6 PASS** | evidence/build/retropie-companion-tests.log |
| Shell script parsing and Python compilation | PASS | run during packaging |
| Refuse host toolchain as webOS | PASS: expected configure failure | evidence/build/webos-host-rejection.log |
| Missing target SDK fails explicitly | PASS: expected early stop | evidence/build/webos-missing-sdk.log |
| Native Windows build / PowerShell execution | NOT RUN | No Windows compiler/runtime here |
| ARM/webOS compile/link/package | NOT RUN | No target SDK, SDL-webOS runtime bundle or device here |
| LG permissions, Magic Remote, 4K/120 Hz on C3 | NOT TESTED | Requires real device |
| Native inline Live TV / HDMI video | NOT IMPLEMENTED | Poster + explicit native handoff only |

Core tests cover coordinate transforms, 4/3/2 navigation, 60/120-step time equivalence,
frame sample bounds, JSON types/size/depth/UTF8, app/setting/IP/token validation,
preferences, inventory filtering, preview dwell/pause/Back/cooldown, procfs CPU deltas,
Luna allowlisting, desktop refusal, atomic preferences and subprocess deadlines/output
limits (including descendants holding stdout open).

Native scenarios cover actual 1280×720, 1920×1080 and 3840×2160 rendering; 720p/4K
pointer click mapping; D-pad movement; overlay focus restoration; library search and
UTF8 backspace; preference persistence; blur/focus recovery; blocked desktop TV actions;
handoff confirmation/cancel; malformed-file preservation; game hub and eight panel
captures; and rejection of host --webos, unknown views and missing fonts.

## Reproduce

```bash
cmake -S . -B build/desktop -G Ninja -DCMAKE_BUILD_TYPE=Release -DSPICE_STRICT=ON
cmake --build build/desktop --parallel 4
ctest --test-dir build/desktop --output-on-failure
./build/desktop/spice_core_tests

cmake --preset core-asan
cmake --build --preset core-asan
ctest --preset core-asan

python3 -m unittest discover -s tests -p 'test_*.py' -v
(cd companion && python3 -m unittest -v test_retropie_bridge.py)

LIBGL_ALWAYS_SOFTWARE=1 MESA_GLES_VERSION_OVERRIDE=2.0 \
xvfb-run -a -s '-screen 0 3840x2160x24' \
python3 tests/native_qa.py --binary build/desktop/spice-home-cpp \
  --assets assets --out build/native-qa
```

A native GL framebuffer screenshot verifies rendering and layout, not the C3's graphics
resolution limit, panel refresh, decoder behavior, app permissions or thermal performance.
The packaged original backgrounds were decoded from 1600×900 artwork; high-DPI text
and shapes are native-resolution, but the background source has not gained new detail.

## Delivered binary

SHA-256:
`fde28cbef056039ad4d351e8eef2cb04879443e0009f93a817934e01a8e29ae2`

Path: `bin/spice-home-cpp-linux-x86_64`.
The archive-level checksum manifest is supplied alongside the ZIP/TAR.GZ. The internal
`SHA256SUMS.txt` records the packaged files; it excludes itself.
