# Local native themes and icons

Implemented November 5, 2026. Themes now load into the actual C++/GLES2 launcher
before its first interactive frame. The original Geometric Blue fixture provides
a background, six semantic icon mappings, one explicit app artwork mapping and
an accent. The app keeps its approved fourth-column staircase and five cards.

The theme store is independent of prepared TV inventory. It neither installs
apps nor changes their identities or availability. Built-in geometry, procedural
icons, prepared artwork, palettes and reduced-motion behavior remain fallbacks.

## Prepare, select and restore

From the project root, using the already installed Windows Python/Pillow:

```powershell
py -3 tools/prepare_theme.py --data-dir demos/themes/demo-data verify
py -3 tools/prepare_theme.py --data-dir my-personalization import demos/themes/geometric/source.json
py -3 tools/prepare_theme.py --data-dir my-personalization default
py -3 tools/prepare_theme.py --data-dir my-personalization previous
```

The helper writes only the `themes/` child of the selected data directory. Import
normalizes local PNG/JPEG/WebP sources, publishes an immutable generation and
atomically selects it. `default` selects built-in defaults while retaining the
previous generation; `previous` validates and swaps the selections. Interrupted
files and malformed-pointer recovery copies are retained. The helper does not
delete stale locks automatically. These operations have their own Python tests.

Start the native executable with `--theme-data PATH/TO/themes`. An explicit bad
selection stops with an actionable error. With no flag, the app checks the
configuration directory's `themes/`, then the assets directory's `themes/`;
an implicit bad selection reports an error and uses defaults. A valid selected
default is supported. Changes apply on the next manual launch. No live texture
replacement, catalog request, file read or image decode is scheduled by a theme
draw call.

The built Linux desktop executable is `build/desktop/spice-home-cpp`; use Linux
paths when running it inside WebOSBuild. The prepared example store is
`demos/themes/demo-data/themes`. Source/build files do not constitute an ARM
binary or TV installation. For a future authorized webOS package, prepare the
selection beneath the new stage's `assets/themes` and keep the existing ARM,
SDK, runtime-library and font gates in [WEBOS-BUILD.md](WEBOS-BUILD.md).

## Admitted data and precedence

`spice-home.theme-source.v1` is the local preparation input. The generated
`spice-home.theme.v1` manifest and `spice-home.theme-pointer.v1` pointer contain
bounded metadata, provenance, hashes, image descriptors and mappings. The
implementation rejects unknown layers/keys, duplicate keys, unsupported image
profiles, links, hard links, unsafe paths, size/hash/PNG-CRC/decode failures and
budget violations. Publisher identity is not established by a content hash.

Runtime backgrounds are opaque 1600x900 RGB PNGs; icons are 128x128 RGBA PNGs.
An app-specific theme hero wins. Otherwise the global background follows its
`all` or `missing` policy; `missing` retains a valid prepared app hero. Theme app
icons override their exact app ID; missing mappings retain prepared or built-in
art, then a procedural icon. A new metadata-only app scan cannot upload artwork;
missing resident images get a fallback until the next prepared launch.

Semantic mappings are explicit at action call sites: `navigation.search`,
`navigation.home`, `navigation.control-center`, `actions.refresh`,
`actions.settings`, and `library.collection`. Shared glyph names do not assign
semantics automatically. Unmapped actions keep their procedural glyphs. Original
icon colors and alpha are preserved. An optional local-theme accent overrides
the base palette; Settings names that relationship.

The incremental admission is **24 MiB RGBA**, including a 1600x900 crossfade
reserve, with a **16 MiB compressed** ceiling. Unique textures are uploaded once
under `theme/<sha256>` keys. The example admits 17,738,752 bytes including the
reserve and uploads 11,978,752 actual texture bytes on the tested desktop path.
The reserve is accounting headroom, not an additional allocated texture.
Prepared textures, text, transient decoder memory and driver overhead are
separate. A 4K framebuffer does not increase the source detail of these bitmaps.

Uploads are transactional at startup: failure removes that attempt's textures
and leaves the active table/accounting intact. Uploads after the first frame are
rejected. Compressed source strings are released after successful preload while
the stable image/mapping records remain alive for the UI.

## Evidence and compatibility limits

The preparation tests, strict/sanitized loader tests, native renderer rollback
checks and native launcher captures are linked by the [fleet report](FEATURE-PARITY-FLEET-2026-11-05.md).
The native theme matrix exercises actual startup, 720p/4K, explicit app mappings,
global/prepared fallback, default/previous selection and invalid-selection
handling. Its extra `com.example.geometric` inventory record is a labelled
synthetic fixture. The underlying C3 source has 21 developer records; the fixture
does not establish an additional installed app. Native tests never contact the TV.

The standalone [theme illustration](../demos/themes/demo.html) is separate from
those native captures. The implementation does **not** admit `.p5theme`
containers, DDS/zstd, SVG, sounds or video. The researched Darkatek7 format and
catalog contract remain in the [original design](NATIVE-THEMES-DESIGN.md) and
the prior source-pinned report. Next increments are a credential-free catalog
browser and bounded archive inspection, followed by independently sourced
DDS/zstd and safe SVG conversion. No upstream GPL implementation, firmware
originals, ThemeHelper or font files were imported.

GPU allocation/timing, physical 240 Hz, ARM/TV execution, real remote/controller
mapping, full UI leak detection and human visual acceptance remain open. Local
path checks are not a sandbox against a hostile same-user filesystem race;
atomic publication is not a power-loss durability guarantee.
