# home-webos (native C++ launcher, 0.1.0)

**A native C++ launcher, installed on the C3 and tested on desktop.**
C++20, SDL2 / SDL-webOS, a raw OpenGL ES 2 shader renderer, FreeType text and CMake/Ninja.
No browser, Flutter, Rust runtime, or SDL_Renderer is used for the interface.

The November 5 ARM build is installed as `com.spice.home.cpp` 0.1.0 and selected
as native default Home, with the owner's existing Home-key and boot hooks updated.
It requests a 3840x2160 drawable, uses a bounded native-density Home cache and
measured 60 fps software pacing. Back returns Home. Processor Deck and native
CPU/RAM sampling are disabled, and service work starts only on explicit requests.
Physical input, heavy-transition pacing, panel presentation and cold boot remain
separate acceptance gates. See [deployment, recovery and remaining features](docs/TV-INSTALL-2026-11-05.md).

![Actual native framebuffer capture](evidence/native/home-1080.png)

The screenshots in `evidence/native` are captured by the executable with `glReadPixels`.
They are not HTML previews. Desktop illustrated app tiles are explicitly sample content.

The working-tree build can now start from a prepared local TV metadata/artwork bundle.
Use `scripts/Prepare-TvData.ps1` to inspect a configured connection, collect data,
refresh one app or stage the verified bundle before installation. Startup does not
query live TV metadata, even when a bundle is absent. Graphics in the bundle are
decoded/uploaded before interactive frames, and missing artwork uses local fallbacks.
See [offline preparation](docs/OFFLINE-PREPARATION-2026-11-04.md) for the tested C3
commands, available data sources, preinstall hook and native evidence.

Home's **Refresh apps / R** footer shortcut scans explicitly and opens the library.
New IDs receive a NEW marker; naming matches for earlier Home/Start/launcher builds
receive a review badge, with full selected ID/version and a **Review builds** filter.
Pin/Unpin names its actual target. On desktop this reads updated local prepared data;
the TV path requests its permitted developer inventory. See
[refresh and review](docs/REFRESH-SCAN-2026-11-04.md) and the
[four Terra MAX perspectives](docs/PS5-THEMES-SYNTHESIS-2026-11-04.md).

The November 5 fleet integration adds controller library search, a bounded return
history preserving library context, boundary-aware pages, immediate selection
emphasis, a clearer Control Center, input-age corrections and ordered background
settings saves. Local themes now supply startup-loaded backgrounds, app artwork,
semantic icons and optional accents. The standalone PSN/Xbox profile dashboard
supports local snapshots and explicit public-data preparation; no verified
account statistics were returned for the supplied Xbox handle and no PSN ID is
confirmed. See the [implementation and demo report](docs/FEATURE-PARITY-FLEET-2026-11-05.md),
[native theme instructions](docs/NATIVE-THEMES-IMPLEMENTATION.md), and
[profile dashboard](demos/profiles/index.html).

The current working-tree build adds a coordinated visual pass, metrics-only text
measurement, one-pass backdrop lighting, capability-gated shader precision and an
experimental painter-order batch renderer. The original packaged `bin/` executable
above is unchanged; the updated executable is `build/desktop/spice-home-cpp`.
See [the visual/performance report](docs/VISUAL-PERFORMANCE-2026-11-04.md) for current
native captures, measured submission counts, test results and recovery. The target is
4K / high DPI / 240 Hz; physical GPU performance and presentation remain unverified.

## Original archive contents

- `bin/spice-home-cpp-linux-x86_64`: compiled Release ELF executable, built on Debian 13.
- C++ source, all original project artwork used here, tests and native screenshots/reports.
- Windows Developer PowerShell build helper and vcpkg dependency manifest.
- ARM/webOS cross-build gate and staging script; **no ARM binary or IPK is included**.
- Read-only agent target inventory and separate-file settings migration tools.
- Optional Python RetroPie status companion, inherited from the existing source release.

This is a **parallel app**, `com.spice.home.cpp`, not an upgrade to `com.spice.home` or
`com.ryanspice.ps5home`. Other launcher packages and the Rust checkout are preserved.
The C++ app is now the owner-selected Home, with guarded recovery copies.

## Interface and behavior

The PS5-style rail, fourth-column right-leaning 4/3/2 staircase and five compact
bottom cards remain. Games/Media selection, searchable library, favorites, Control
Center, inputs, Processor Deck, RetroPie, Homebrew, read-only LG settings, display
information and a preview-event screen are present.

Motion uses elapsed time: 140 ms retargetable focus, 300 ms coalesced artwork segments
and 200 ms panel entrances with shared drawing/hit geometry. Reduced motion and
collapsed/expanded layout persist. Presentation tracks freeze while hidden, minimized
or unfocused; the restored frame consumes no hidden elapsed time. Startup loads the
small packaged artwork set; text is rasterized at drawable density into a bounded cache.
See [the motion follow-up](docs/MOTION-UX-2026-11-04.md) for the 53 core, 56 existing native
and 33 motion-check results, native captures, independent review and remaining gates.
The November 23/25 work adds a bounded deterministic PS5-style ambient wallpaper
layer (drifting particles, focus parallax, breathing vignette, focus shine) drawn
over a live backdrop and transparent cached Home content. It is opt-out through
`ambient_motion` or reduced motion; `ambient_idle_hz` (default 60) can rate-limit
ambient-only frames for OLED power, and `render_resolution` selects
`max|2160|1440|1080`. Focus moves now repaint cached content at most once.
Measured results and verification are in [PARITY.md](docs/PARITY.md) and
`.ai/evidence/20261125-overnight/p1-result.md`.

There are **six color palettes**, not six fully reimplemented historic console layouts.
See [PARITY.md](docs/PARITY.md) for the precise supported/partial/unverified distinctions.

## Design catalog browser (POC port)

`--view designs` opens the 50-concept app catalog carried over from the Rust 0.3.5
work in `spice-home`. It is presentation data ported as a **guideline and proof of
concept**, not a feature that runs on the TV.

- `assets/designs/catalog.json` is the unreviewed Rust POC catalog, copied byte-for-byte.
- Open **Settings → Browse 50 design concepts**, or use `--view designs`.
- Tiles, category strip, text search, paging and the inspector draw entirely with the
  native GLES2 primitive renderer; there is no HTML, WebView or script engine.
- Select **Search concepts** for a native A–Z / 0–9 keyboard with Space, Delete,
  Clear and a matching-result count. D-pad / OK and pointer input both work;
  a hardware keyboard can also filter directly without activating launcher shortcuts.
- Category selection keeps focus on the chip. Disabled page controls are skipped,
  empty results offer a reset action, and Back restores the inspected tile or the
  launcher control that opened the collection.
- Every entry is provenance-stamped sample data. A design is never launchable: it
  carries no package id, URL, command, credential or device action, and activating a
  tile only opens its inspector.
- `src/designs.cpp` validates the catalog (schema, design-only flag, unique keys,
  `#rrggbb` colours, bounded counts and string lengths) and ships in `spice_core`, so
  it can be exercised without SDL, GL or a TV.

```bash
./scripts/run-linux.sh --view designs
```

See [DESIGN-CATALOG-PORT.md](docs/DESIGN-CATALOG-PORT.md) for provenance, the target
rendering model and the remaining milestones. The latest interaction checks and
native captures are documented in [the UI/UX follow-up](docs/UI-UX-2026-11-04.md).

## Run on Linux or WSLg

The bundled binary was built against Debian 13 runtime libraries. It is not a portable
AppImage and may need rebuilding on older distributions. SDL2, FreeType, libpng,
JSON-C, libcurl and their OS dependencies must be installed. No fonts or external shared
libraries are shipped. Use an installed font, or pass `--font /path/to/your/font.ttf`.

```bash
./scripts/run-linux.sh
./scripts/run-linux.sh --width 3840 --height 2160 --fps 120
./scripts/run-linux.sh --view diagnostics
```

For a source rebuild, compare the experimental batching path with the default:

```bash
./build/desktop/spice-home-cpp --assets ./assets --width 3840 --height 2160 --fps 240
./build/desktop/spice-home-cpp --assets ./assets --width 3840 --height 2160 --fps 240 --batch-renderer
```

Batching reduces GL submissions in the measured scenes but sends more vertex data.
It remains opt-in pending target-GPU qualification. `--legacy-background` retains the
previous layered backdrop for controlled comparison. Reports separate CPU phases,
swap, waits and capture; they explicitly label GPU timing and physical presentation
as unavailable rather than deriving those from software-renderer frame intervals.

The `--fps` value is the **fallback render ceiling when VSync is unavailable**. It does
not set the TV panel refresh or guarantee the requested rate. The app reports actual
window/drawable sizes, accepted swap interval and observed frame intervals.

To rebuild with installed development dependencies:

```bash
./scripts/build-desktop.sh
./build/desktop/spice-home-cpp --assets ./assets
```

On Debian/Ubuntu, install CMake, Ninja, a C++20 compiler, `libsdl2-dev`,
`libfreetype-dev`, `libpng-dev`, `libjson-c-dev`, and `libcurl4-openssl-dev` through your
normal package manager. Native UI tests additionally use Xvfb; Python tests are stdlib-only.

## Build on Windows 11

Use a Visual Studio 2022 **Developer PowerShell** with C++ tools, CMake, Ninja and an
existing vcpkg installation. Manifest mode can download/build dependencies; the helper
does not install Visual Studio or vcpkg itself. This Windows path is included but was
**not executed in the Linux build environment**.

```powershell
Set-Location 'B:\Temp\@Browser\spice-home-cpp'
.\scripts\Build-Desktop.ps1 -VcpkgRoot 'C:\tools\vcpkg' -Run
```

Use your actual extracted folder and vcpkg path. The script runs CTest, copies runtime
DLLs from the vcpkg build into the output directory, and starts the native executable.
The included Linux ELF cannot be run as a Windows `.exe`.

## Controls

| Input | Action |
|---|---|
| Arrow keys / controller D-pad | Spatial navigation |
| Enter / Space / controller A | OK |
| Escape / controller B or Back / mouse Back | Native TV returns Home; desktop panels return through local context; Home stays Home |
| Pointer / wheel | Select/click or step through targets |
| C | Control Center |
| R outside text entry | Explicit app scan; Home footer is also pointer/D-pad accessible |
| E / M / T | Expanded layout / reduced motion / next palette |
| F outside text entry | Pin selected real app shortcut; library uses its named Pin/Unpin action |
| F2 / F3 / F4 | Diagnostics / library / launcher settings |
| F11 | Desktop fullscreen |
| Type in library or design collection / Backspace | Filter / delete one character |
| Search concepts in the collection | Native keyboard for D-pad / OK or pointer input |

LG-specific SDL scancodes must be captured on the TV and mapped in
`remote_scancodes`. JavaScript browser keycodes are not assumed to survive SDL.
The native SDL-webOS Back/Exit keys are claimed before window creation. Processor
monitoring and its Home/Control Center targets are disabled in the TV build.

## Integrations: strict boundaries

In desktop mode, **no Luna command is executed**. Processor measurements are from the
host machine and labelled accordingly. TV launches, inventory and settings reads report
that they are unavailable rather than inventing results.

The webOS build has bounded public Luna calls for launching an explicitly chosen app,
app availability, device/network information and read-only settings. Private installed-app
and HDMI-status queries require `SPICE_CPP_PRIVATE_LUNA=1`; permission failure remains
normal. Nothing grants ACLs, invokes a root shell, patches system files or takes over Home.

Homebrew opens the Homebrew Channel so you manage updates there; it is not an update-all
client. RetroPie status is an explicit request to an RFC1918 IPv4 endpoint with the session
`SPICE_RETROPIE_TOKEN` environment variable. HTTP is **not encrypted**; use only an isolated,
trusted LAN, never port forwarding. The launcher does not start EmulationStation remotely.

The app's own runtime keeps these permission boundaries. The owner-authorized
operator setup selected default Home separately; see the deployment/recovery report.

**Inline tuner/HDMI video is not implemented.** Posters remain in the renderer; optional,
confirmed dwell behavior requests the native Live TV/HDMI application. Back cancels reentry,
covered/unfocused states reset dwell, and failed handoffs receive a 45-second cooldown.

## Preferences and adaptation

The C++ app writes only its own `SpiceHomeCpp` / `spice-home-cpp` preferences directory
(or an explicit `--config` file). Unknown fields in that *C++* file are preserved; do not
store credentials there. Malformed existing files are not overwritten.

To migrate a previous JSON file safely:

```powershell
py -3 .\tools\agent\adapt.py migrate `
  --from 'B:\Path\To\old-settings.json' `
  --to 'B:\Temp\@Browser\spice-cpp-settings.json'
```

The migrator copies only known safe fields, drops credentials/unknown fields, requires a
new destination, and disables automatic handoff. It does not modify the source settings.

To inspect the existing implementation without modifying it:

```powershell
py -3 .\tools\agent\adapt.py inspect `
  --target 'B:\Dev\webos\spice-home' `
  --out 'B:\Temp\@Browser\spice-cpp-adaptation-review'
```

The report is static file evidence, not semantic parity. Read `AGENTS.md` before integration.

## Evidence and limits

[Verification](docs/VERIFICATION.md) · [Parity](docs/PARITY.md) ·
[Architecture](docs/ARCHITECTURE.md) · [webOS build gate](docs/WEBOS-BUILD.md)

The executable and tests ran locally with Mesa **software rendering**, including an
OpenGL ES 2.0 context and 3840×2160 framebuffer capture. This proves a native desktop
implementation and rendering path—not TV GPU performance, physical 120 Hz presentation,
LG native permissions, or a working ARM package. Those remain hardware acceptance gates.
