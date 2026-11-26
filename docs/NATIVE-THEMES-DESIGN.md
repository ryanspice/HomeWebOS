# App-owned native themes and icons — original November 4 design

Historical design below. The local image/icon foundation was implemented on
November 5 and now has native startup/rendering integration. Use
[NATIVE-THEMES-IMPLEMENTATION.md](NATIVE-THEMES-IMPLEMENTATION.md) for the shipped
local schema, commands, precedence, resource limits and evidence. The archive,
catalog, DDS/zstd and SVG portions below remain future compatibility work.

This design reconciles the requested investigation of [Darkatek7's theme site](https://themes.darkatek7.com/)
and [ProsperoThemes](https://github.com/Darkatek7/ProsperoThemes). The completed
[Terra MAX report](../.ai/runs/20261104-refresh-ps5-themes/reviews/native-themes.md)
contains source links and inspection receipts. Upstream source was pinned to
`96a92a862ea68e55edc51b38d8ad19f2c8521f98` during the study.

At the time of this design, the application had six built-in accent palettes and prepared per-app
graphics. It did not import/apply `.p5theme` files or override system glyphs
from a theme. This document defines the smallest useful implementation boundary.

## Compatibility facts

The observed public catalog is GET `/api/themes?sort=new&page=1&pageSize=4`, with
pagination, metadata, layer summaries, relative preview/download URLs, archive
size and SHA-256. The study fetched catalog metadata and one small upstream test
fixture; it did not fetch the advertised community download or apply a pack.

The inspected format uses ZIP or USTAR with numeric manifest format 2. Targets
describe logical path, layer, payload file, declared length and hash, plus optional
resource/screen information. This is a content-integrity contract, not publisher
authentication. The upstream fixture verified one hashed Zstandard background
payload, not successful background decoding or icon conversion. See the pinned
[manifest](https://github.com/Darkatek7/ProsperoThemes/blob/96a92a862ea68e55edc51b38d8ad19f2c8521f98/Shared/ThemePackManifest.cs)
and [reader](https://github.com/Darkatek7/ProsperoThemes/blob/96a92a862ea68e55edc51b38d8ad19f2c8521f98/Shared/ThemePack.cs).

| Upstream layer | App-owned interpretation | Work required |
|---|---|---|
| Raw local PNG/JPEG/WebP | Opaque background, app cover or icon | Reuse existing normalization pattern; new theme schema and selection |
| Hub/app DDS wrapped in Zstandard | Normalized app background | Bounded zstd + DDS BC7/BC1 conversion, independent implementation/dependency review |
| System SVG resource | Named UI icon | Semantic mapping and bounded offline SVG rasterization; external resources/scripts disabled |
| Optional PNG icon preview | Explicitly labelled preview-derived icon | Validate independently; presence and exact SVG equivalence cannot be assumed |
| System-tile PNG | Mapped app-owned tile | Explicit semantic slot or local app-ID mapping; PS title IDs are not LG app IDs |
| Music/UI sounds/video | Unsupported initially | Separate runtime/codec, lifecycle, volume, memory and target evidence |
| Stock originals, PS5 paths/helper/system patch | No app destination | Never interpret upstream paths as writable device paths |

## Proposed app contract

Keep themes separate from prepared inventory and its app identities. A new
`spice-home.theme.v1` schema should contain a name/version, source/provenance,
optional accent, global background policy, semantic icon slots, explicit app-ID
art overrides and a validated content-addressed image manifest. The exact parser
is future work; the format name here is a proposal, not a shipped interface.

Suggested precedence is: explicit per-app theme override; global theme background
according to the user's chosen missing-art/all-app policy; prepared artwork;
built-in neutral fallback. Preserve a source/app-specific fallback for every icon.
Theme data never marks an app installed and never invents a catalog identity.

Use semantic slots such as `navigation.search`, `navigation.home`,
`navigation.control-center`, `actions.refresh`, `actions.settings`, and
`library.collection`. Map upstream resources explicitly. The integrated UI now
uses a distinct `controls` glyph instead of reusing refresh for Control Center;
future theme overrides should remain keyed by action, not by a shared drawing name.
App covers and heroes remain keyed by real local app IDs. Preserve colored icons;
offer an explicit tint policy only for monochrome assets.

The theme generation follows the proven data-preparation pattern:

1. Explicit browse/download or local import on a worker. Catalog use is optional.
2. Validate the archive and logical target allowlist before extraction/decoding.
3. Convert supported content offline into opaque PNG backgrounds and RGBA icons.
4. Verify every generated asset and a complete memory budget in an immutable stage.
5. Atomically select an app-owned active-theme pointer, retaining prior/default data.
6. Load/apply at the next manual app launch, before interactive frames. Restore is
   the same pointer operation to the previous generation or built-in defaults.

Display pending/active/error state clearly. Preserve existing preferences, the six
accents, focus/input geometry, reduced motion and prepared inventory. No PS5 shell
restart, TV system write, ThemeHelper, console ID, likes or telemetry is needed.

## Admission and resource limits

Use a distinct credential-free HTTPS client, not the token-bearing RetroPie client.
Keep certificate/hostname verification enabled. Constrain URLs and redirects to the
expected origin, enforce announced/streamed lengths and archive hash, then verify
manifest entries independently. The inspected upstream certificate bypass is not
an acceptable application template.

Reject duplicate normalized names/targets, traversal and absolute paths, links,
special files, encrypted/unsupported archive methods, CRC/hash/size mismatches,
oversized JSON, excessive dimensions/pixels and nested decompression bombs. Write
to app-generated staging names; input target paths are never filesystem destinations.
Archive, expanded-payload, zstd-output, decoded-RGBA and temporary-memory ceilings
must be separate. ZIP-first support is reasonable; unsupported layers and USTAR
must remain explicit until implemented and tested.

A proposed initial incremental theme budget is 24 MiB RGBA, including both
crossfade textures and icon textures, with content-hash deduplication. Two 1080p
RGBA backgrounds and 32 unique 128×128 icons require about 17.8 MiB before driver
overhead. One full 4K RGBA texture is about 31.6 MiB and exceeds that proposed cap.
These are admission estimates, not target-GPU allocations or performance results.
Count prepared/text textures and decoder scratch separately. Preserve uniform
background aspect/opacity until mixed-aspect crossfade behavior is qualified.

The existing renderer uploads startup PNG textures and refuses prepared uploads
after the first frame. Start with relaunch-to-apply; hot replacement would require
a separately designed texture-lifetime and frame-budget contract. No network,
decompression, file read or unbounded upload belongs in interactive rendering.

## Implementation order and open gates

First implement a local, app-owned image/icon theme with semantic mapping,
fallback/default restore and startup loading. Then add explicit catalog access and
bounded format-2 ZIP inspection; then independently sourced zstd/DDS conversion
and safe SVG rasterization. Audio/video and broader archive support follow only
when needed and separately qualified.

ProsperoThemes declares GPL-3.0, while this prototype's source is MIT. Do not
transplant its implementation or assume a pack's artwork/firmware resources share
that source license. Review exact converter dependencies and asset provenance;
keep proprietary originals/fonts and PS5-only libraries outside the app.

Required future evidence: malformed archive and conversion tests; mapping/fallback
tests; interruption/restore tests; no-interactive-I/O assertions; strict/sanitizer
and native themed captures at 720p/4K; startup/CPU/memory/upload/GPU measurements;
ARM/webOS runtime and human acceptance. No such theme-runtime pass is claimed here.
