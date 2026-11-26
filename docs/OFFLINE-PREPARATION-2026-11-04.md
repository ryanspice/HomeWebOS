# Explicit TV data preparation — 2026-11-04

The C++ launcher can start from a prepared, local snapshot of app metadata and
graphics. Collection runs explicitly before installation or after an app changes.
The launcher reads and validates the snapshot once at startup, uploads its declared
graphics before interactive rendering, then uses memory lookups. There is no automatic
metadata fetch, background app-inventory subscription, artwork download or missing-data
API fallback. Startup no longer requests TV identity, including when no bundle exists.

Local file reading, PNG decoding and GPU upload still happen once per application
launch. Text rendering retains its bounded text/metrics caches. Preparation removes
live metadata/artwork acquisition from the interactive path; it does not persist GPU
allocations across processes. Local processor counters remain live and retain their
host/TV labels. Explicit launches, settings reads and refresh buttons keep their
existing permission boundaries.

## One entry point for connection, collection and preinstallation

From `B:\Dev\webos\spice-home-cpp`, in PowerShell:

```powershell
# Inspect the configured device and available read sources.
.\scripts\Prepare-TvData.ps1 -Mode Inspect -Device c3

# Collect a new bundle, or publish a new generation of an existing bundle.
.\scripts\Prepare-TvData.ps1 -Device c3 -BundleRoot .\prepared\c3

# After an app install/update: refresh that app, preserving the other records/art.
.\scripts\Prepare-TvData.ps1 -Device c3 -BundleRoot .\prepared\c3 -AppId com.example.newapp

# Verify with no TV connection.
.\scripts\Prepare-TvData.ps1 -Mode Verify -BundleRoot .\prepared\c3

# Copy the current verified generation into a new preinstall data directory.
.\scripts\Prepare-TvData.ps1 -Mode Stage -BundleRoot .\prepared\c3 `
  -StageDirectory .\dist\my-reviewed-stage\assets\prepared
```

Use the actual installed app ID in the single-app command. A missing/unreadable app
fails while retaining the committed snapshot. A targeted refresh does not query TV
identity or the full app inventory. Other app records retain their observation times;
their graphics are protected first if the new app would exhaust the graphics budget.
The changed app may have neutral graphics until an explicit later preparation admits
them. Apply the new bundle by re-staging it and relaunching the app; an already-running
process keeps its current in-memory generation.

The helper requires existing Python 3, Pillow for image preparation, Node and the
LG CLI. Verification/staging require only Python's standard library. It installs no
dependencies, pairs no device and edits no CLI profile/key. `-CliRoot` selects an
existing CLI installation when automatic discovery does not cover that machine.
An explicit device name is required; the emulator default is never selected silently.

For cross-build staging:

```bash
export SPICE_PREPARED_DATA=/mnt/b/Dev/webos/spice-home-cpp/prepared/c3
export SPICE_WEBOS_TOOLCHAIN=/your/verified/sdk/toolchainfile.cmake
./scripts/build-webos.sh
```

The build verifies the bundle before compiling and stages the committed generation
after the existing ARM/header gates pass. This step has no TV connection. A fresh
destination is required; it never overlays an older prepared directory. No build,
install or Home binding is implied by collecting or staging data.

## Connection method and exact current scope

The user pointed to
<t3-thread://v2/d00ada42-5431-41c1-901c-f3a00b22859d/9edcef9b-d5b3-4631-b2cb-33513292bc88>.
That thread uses the configured `c3` Developer Mode connection with `ares-novacom`
and the installed LG CLI session. Its developer app listing is the supported
`luna://com.webos.applicationManager/dev/listApps` route through `luna-send-pub`.
The new collector uses that same named device and existing SSH credentials through
the CLI's installed `ssh2` dependency, in one bounded read-only session. It reads
configuration directly without invoking the CLI initializer that can rewrite defaults.
The shared device configuration hash is checked again when the session closes.

The current C3 reports **OLED48C3PUA**, firmware **43.21.84**, SDK **11.2.0** and UHD.
Live inspection confirms **21 developer-registry app records**. The final bundle has
**21 launchable records and 26 graphics**: 21 icons and five backgrounds. Planned
decoded RGBA storage is **34,305,024 bytes / about 32.7 MiB**; normalized PNG files
total **762,200 bytes**. No collected graphics were dropped for admission limits.

The current profile does **not** expose the protected LG Home package or global
launch-point list. That is recorded under `source_availability`, including
`inventory_scope=developer`; this is not labelled a complete inventory of LG/store
apps. Earlier private-client and public global-route failures are retained. The
supported developer route was discovered from the referenced thread and installed
CLI declarations, then verified live. No alternate account or privilege was assumed.

`-PrivateReads` permits additional named read queries; it grants no ACG or filesystem
access. `-LunaClient private -PrivateReads` selects the private Luna executable only
for an existing appropriately permitted device profile. Neither flag changes caller
identity, enables root, installs a helper or bypasses a denial. With the current `c3`
profile, use the public developer path. `Inspect` makes source availability visible
before a preparation run.

## Metadata, marketing information and graphics

Collection combines the available developer inventory with readable packaged
`appinfo.json` files. Stored fields include app ID/title, description, vendor,
version, observation time, inventory confirmation state and image keys. Readable
launcher metadata is recorded separately. Launch parameters, credentials, serial
numbers, MAC addresses, account databases and browsing/history data are not copied.

App metadata defines icon, large-icon, description and background/splash references.
These are documented by [LG's appinfo.json reference](https://webostv.developer.lge.com/develop/references/appinfo-json).
The preparation tool normalizes readable PNG/JPEG/WebP sources into bounded PNGs:
256×256 RGBA icons and 1600×900 opaque backgrounds. It preserves icon alpha and
flattens background alpha before the fused backdrop shader uses it. This is format/
size preparation, not additional source detail or generated replacement artwork.

Live cloud promotions and authenticated LG storefront/recommendation feeds are not
queried. An owner-provided local marketing export can supply extra descriptions and
art explicitly:

```json
{
  "apps": [
    {
      "id": "com.example.app",
      "appDescription": "Description from your local marketing export",
      "largeIcon": "art/app-icon.png",
      "bgImage": "art/app-background.jpg"
    }
  ]
}
```

Pass its path with `-MarketingJson .\exports\marketing.json`. Image paths must stay
within that export's directory; remote URLs are not fetched. During single-app
refresh the overlay affects only that app. An independent offline import is also
available with `python tools/prepare_tv_data.py import --bundle ROOT --input FILE`.
Imported data is labelled owner-provided and cannot claim confirmed TV inventory.

## Bundle and startup contract

```text
prepared/
  current.json                     # atomic pointer + manifest SHA-256
  snapshots/<generation>/
    manifest.json                  # data hash, image keys/hashes/sizes/dimensions
    data.json                      # filtered metadata, source/availability, times
    images/<sha256>.png             # normalized, content-addressed graphics
```

Each generation is immutable. A complete candidate is validated before `current.json`
is atomically replaced. Preparation uses an exclusive lock and checks that the prior
pointer did not change. Failed or concurrent preparations retain the committed
generation; earlier snapshots and failed candidate evidence are not deleted.
Hashes detect content damage and bind the local files; they are not device signatures.

Limits are 200 launchable app records, 128 graphics, 16 MiB of compressed graphics
and 48 MiB of planned decoded RGBA storage, with per-file and image-dimension caps.
Icons receive priority, and single-app refresh protects other apps' graphics first.
Optional/missing art remains a neutral local icon/backdrop. It never starts a later
HTTP request. Only packaged app metadata and supported image files can be copied;
resolved paths must stay inside the explicit package/public-art roots.

Startup checks the pointer, manifest, data and image hashes; validates strings,
unique app IDs, image bindings, PNG headers/dimensions and budgets; then uploads all
declared images. Compressed buffers are released afterward. The renderer rejects
prepared uploads after its first frame. Prepared mode avoids loading unrelated
illustrated game art and retains neutral fallback art. Inventory labels say
`PREPARED SNAPSHOT`, not live installation status. Observation time and source remain
in reports; an explicit inventory read can reuse prepared art without downloading.

Data selection is explicit `--prepared-data DIR`, then `<config-directory>/prepared`,
then `<assets>/prepared`. An invalid explicit bundle fails before interaction. An
invalid automatically discovered bundle retains local fallback content, records its
error and sends no automatic metadata request. The packaged `bin/` executable was
not changed; these features are in the rebuilt `build/desktop/spice-home-cpp`.

## Evidence and verification

Actual collected data is under `evidence/offline-preparation/c3-bundle/`; the active
generation is selected by its `current.json`. The verified preinstall copy is under
`evidence/offline-preparation/preinstall-final/assets/prepared/`. Both are local,
owner-specific data artifacts; the normal source `assets/` was not populated with them.

| Check | Result | Evidence under `evidence/offline-preparation/` |
|---|---|---|
| Supported C3 connection/source inspection | PASS; global LG sources explicitly unavailable | `connection-inspect.json` |
| Live C3 collection | 21 records / 26 images | `collect-c3-supported-route.log` |
| Real single-app preparation | Other 20 records retained; no full inventory/device query | `.ai/runs/20261104-offline-preparation/single-app-verification.json` in project |
| Python preparation boundaries | 14/14 | `python-tests-final.log` |
| Node transport boundaries | 13/13; zero device requests in unit checks | `transport-checks.log` |
| Strict Release and ASan/UBSan CTest | 5/5 each | `ctest-release-final.log`, `ctest-sanitize-final.log` |
| Existing core scenarios | 53/53 in each build | `core-release.log`, `core-sanitize.log` |
| Prepared-data core scenarios | 13/13 in each build | `prepared-core-release.log`, `prepared-core-sanitize.log` |
| Existing native regression and motion | 56/56 + 33/33 | `native-regression-final/`, `motion-regression-final/` |
| Native prepared-data matrix | 12/12 Release | `native-prepared-accepted/prepared-native-qa.json` |
| Native prepared-data ASan/UBSan | 12/12; UI leak detection disabled | `native-prepared-sanitized-accepted/prepared-native-qa.json` |
| Preinstall data staging | PASS; no TV connection | `preinstall-final.log` |

The new native matrix exercises Home/library/diagnostics in both render modes at 4K,
720p pointer/key browsing, explicit refresh, both automatic discovery locations,
corrupt implicit fallback, explicit corruption refusal and an unchanged bundle pointer
after runtime use. Normal browsing posts zero Device/Apps/Inputs/Settings/RetroPie/
Probe/Launch requests. The explicit refresh case posts one Apps request; desktop mode
rejects the actual TV operation as designed. Local processor sampling is separate.

Native renderer/UI sanitizer checks explicitly disable leak detection; ordinary core
tests retain it. The existing 1,032-byte SDL/libdbus startup allocation leaves full UI
LSAN open. Native screenshots are from the Linux GLES2/llvmpipe executable using data
read from the C3, not screenshots of this app running on the TV. No physical 240 Hz,
TV GPU performance, new ARM build/install, Home takeover or human acceptance is claimed.

The dated source/binary receipt and check-only recovery tool are in
`evidence/offline-preparation/`. Recovery validates all current after-hashes and original
copies, and refuses later edits before any mutation. It retains prepared data and
evidence. The live result has not been restored. Earlier visual-pass recovery must
not overwrite this later slice; its old hashes correctly block that operation.
