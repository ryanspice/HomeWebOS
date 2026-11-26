# C3 installation, native Home and physical follow-up — November 5, 2026

The owner-authorized C++ launcher is installed on `c3` as `com.spice.home.cpp`
version `0.1.0`, build cut `r13`. The TV is an OLED48C3PUA, firmware 43.21.84,
SDK 11.2.0. It runs as an ordinary jailed native app; the separate operator
scripts used the TV's existing root helper to prepare data and select Home.

The final package is
`../.ai/outbox/20261105-tv-install/r13/com.spice.home.cpp_0.1.0_arm.ipk`, SHA-256
`9b4518c201e3ab68b2a264c6d095d2c561eb6b6e98487ac210c06366b8e7292a`.
Its installed executable hash is
`b230a5e1908518956e275164fb88055aae6be5441f78ac033ca3781769bc75b8`.
The live process hash matches the verified staged payload. The original archive's
Linux `bin/` executable and other launcher installations are preserved.

## Resulting behavior

Native `setDefaultApp` selects this app for category `home`; the setting reads
back correctly and `launchDefaultApp` succeeds. Existing Input Hook bindings
125/773/774, the existing early/late launch hooks and the existing recovery-kit
mirror now target this ID. Input Hook logged a reload. The late boot hook was
executed successfully. A full power-cycle test remains open.

Back and held Back are claimed through SDL-webOS before window creation. Native
Back, mouse Back/right-click, controller Back and the Control Center's on-screen
back arrow return to Home. The arrow's former exit-confirmation action was
removed. Native Back on Home stays there. Desktop panels retain their local
return context; the Control Center toggle still restores its invoker.

Fresh delivered keyboard, mouse and navigation-controller input can recover a
stale focus/hidden notification. Older queued input is rejected, hidden time is
excluded from animation, and background controller events are disabled. The TV
Quick Settings overlay was opened and closed through App Manager: OS focus moved
to the overlay and returned to the C++ fullscreen surface. This is separate from
physical mouse/remote testing and the owner's exact firmware-update popup.

The owner subsequently requested Processor Deck off. The TV build removes its
Home card, Control Center card and bar target, blocks the native processor page
and rejects processor sampling requests in the backend. The service worker starts
only when an allowed explicit request needs it. Normal navigation leaves extra
profiling off; bounded captures and the explicit Display Lab enable it. Desktop
monitoring remains a development feature. The device probe records processor
disabled/unavailable, zero processor requests, and no service worker started.

## Native 4K and rendering work

The app requests a 3840x2160 card rather than desktop-fullscreen, which coerced
the previous window to the 1080p UI mode. Ordinary App Manager diagnostics showed
3840x2160 window coordinates, a 3840x2160 drawable and a 2x scale for the logical
1920x1080 layout. The TV compositor screenshot API still emits 1920x1080 PNGs.
Physical output resolution and presentation rate are not certified by those PNGs.

Material draws keep their existing shaders and painter order. Geometry is staged
in bounded chunks and uploaded before the draws consume it. The universal batch
shader remains opt-in because the tested Mali-G52 result was slower. A dedicated
backdrop program avoids carrying the general shape shader through every background
pixel. A reusable Home framebuffer caches unchanged content at drawable density;
the moving focus frame is drawn separately. Its ceiling is 32 MiB, with uncached
fallback when unavailable or over budget. Background transitions use direct painting.
Logical content, artwork, clock, data and viewport changes invalidate the cache.
The selected tile's small emphasis is immediate; focus and artwork motion remain timed.

The native default uses swap interval 0 with a 60 fps software ceiling. `--vsync`
retains the previous driver-sync comparison route. This changes app pacing, not
the panel mode. In the final 180-frame prepared-Home renderer probe:

- Window/drawable: 3840x2160; ARM / Mali-G52 / OpenGL ES 3.2.
- Active-present interval: median 17.245 ms; p95 27.842 ms; p99 30.902 ms.
- CPU render submission, including driver waits: median 11.506 ms.
- Home cache: 33,177,600 bytes; 168 hits and one repaint in the recorded run.
- Posted requests in the app's Services queue: zero, including processor sampling.

This bounded probe used the installed binary in its existing app jail with test
preferences. It did not inherit SAM lifecycle registration, and the SDL helper
reported a denied `getAppInfo`. Its rendering timings are separate from production
permissions, physical input latency, GPU timer measurements and human acceptance.
The screenshot frame is excluded from the interval distribution. Cold work and
occasional longer frames remain. LG lists this C3's panel as
[120 Hz native](https://www.lg.com/ca_en/tv-soundbars/oled-evo/oled48c3pua/);
physical 240 Hz is outside this TV's capability.

## Offline data and package qualification

The existing root helper collected 186 registry records at preparation time:
58 visible app records, 128 hidden records, LG Home metadata and 62 launch points.
The committed bundle admits 53 normalized graphics / 46,880,768 RGBA bytes;
17 graphics were omitted by the bounds. It is an explicit local snapshot. The
launcher loads it before interaction and makes no automatic content-metadata calls.
Ordinary in-app refresh still has the narrower permissions of its native caller;
it is not the root collection route. New artwork requires explicit preparation
and relaunch. Descriptions/art come from installed packages and local exports;
live store marketing feeds are absent.

The real NDK 11.2.0 / GCC 13.3 ARM toolchain, bundled SDL-webOS 2.32.10 headers,
official FreeType 2.13.2 headers and an isolated libpng 1.6.41 ARM build were used.
The existing C3 runtime resolved the dynamic dependencies. `$ORIGIN/lib` DT_RPATH
selects the bundled SDL; system FreeType and GLES were observed in process maps.
No fonts are packaged. The first Windows IPK lost its executable bit; final
packaging uses Linux staging plus permission normalization and byte verification.
All 79 final payload files match: directories 0755, executable 0755, data 0644,
safe paths, ELF32 ARM. The shared SDK and CLI device configuration were unchanged.

Private app settings now live under `/media/internal/spice-home-cpp`, owned by the
actual app UID, mode 0700. Existing C++ preferences were copied without resetting
them. Settings writes remain ordered and asynchronous. SIGTERM handling is installed
after native window setup so orderly shutdown can drain saves; desktop Release and
sanitized termination fixtures verify that behavior. Native forced-termination
report export remains best-effort.

## Checks and evidence

Strict Release and ASan/UBSan CTest pass 12/12, including native Back/disabled-
processor checks and the framebuffer-cache test. The latter covers 720p, 4K,
letterbox, revision invalidation, reuse and the 32 MiB ceiling. Window/texture
render targets can differ at subpixel SDF edges: the test permits 4/255 outside
the long diagonal's two 3-pixel endcap neighborhoods, 16/255 inside them, and fewer
than 0.01% changed RGBA channels overall. Earlier failures and images are retained.
Nearest filtering on the 1:1 cache copy avoids a second interpolation of glyphs.

Core leak checks run. UI address/undefined-behavior tests explicitly disable leak
checks; full UI LSAN and device GPU-resource longevity remain open. Broader native
UI, motion, inventory, theme, prepared-data and pacing receipts are under
`../.ai/evidence/20261105-tv-fixes/`. The final acceptance JSON lists their exact
completed counts and source/binary pins. Failed wrapper qualifications are retained.

New project work products are under `.ai/`. Earlier task products created under
`evidence/` and `dist/` were ingested with byte checks and originals retained.
Task-owned ext4 staging under `/home/builder` is a recorded transport exception:
DrvFS/Windows packaging did not preserve native permissions. Its payload was
ingested and verified locally. Device-side captures, probe temporaries and Home
backups are also device staging/recovery exceptions. Credentials and shared
runtime databases remain tool-managed. No Git initialization, commit, push or
publication occurred.

## Recovery

The original four Home files are retained locally and under
`/var/lib/webosbrew/spice-home-cpp-default-backup-20261105` on the TV. The recovery
helper verifies all current hashes before changing anything. Check mode passed
with zero changes. Use the delivered `recovery/Restore-Home.ps1` without arguments
to check, or `-Apply` to restore. It restores the previous PS5 Home key/boot files
and selects the prior native Home setting, using stock LG Home when it was implicit.
Other default categories are retained. The C++ app and its data stay installed.
A later configuration hash mismatch stops recovery. The configured `c3` route
and the existing root helper must remain available.

## Remaining features and acceptance

Direct Darkatek7/Prospero `.p5theme` import, catalog browsing/downloading, DDS/zstd
and SVG conversion, live theme application, audio/video themes and a native theme
studio remain separate work. Prepared image themes, semantic/app icon maps and
six palettes exist; 1600x900 artwork does not acquire native 4K source detail.

The player-profile page is a standalone browser demo. Native profile integration,
verified PSN identity/data, Xbox account statistics, trophy/achievement/game feeds
and authentication remain open. No supplied email was searched or placed in artifacts.

Inline tuner/HDMI/Pi video, PIP, OS recent-app switching, richer system controls,
full runtime inventory/art refresh, automatic legacy-build removal, generalized
IME/accessibility and end-to-end RetroPie pairing are not complete. Existing
handoffs and explicit shortcuts retain their documented limits.

The physical mouse/remote retest, exact update-popup recovery, pointer corners at
native density, stable pacing during heavy artwork changes, physical 4K/120 Hz,
cold boot, sleep/resume, OLED/screensaver behavior, long-run GPU memory and human
visual acceptance remain explicit gates. Processor Deck is intentionally disabled
on the TV at the owner's request.
