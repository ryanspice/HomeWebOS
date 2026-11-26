# PS5 comparison and native-theme synthesis — 2026-11-04

The requested refresh feature is implemented. Exactly four independent
`codex-native/gpt-6.1-terra` tasks ran with `reasoningEffort=max`: three PS5
comparisons and one Darkatek7/ProsperoThemes investigation. All four completed.
They researched/inspected evidence and did not edit shared app source. Parent
retained all 46 reviewed file preimages, reconciled findings and made a separate
integration version with its own tests and recovery boundary.

| Perspective | Report | Most useful conclusions |
|---|---|---|
| Spatial/visual layout | [10 findings](../.ai/runs/20261104-refresh-ps5-themes/reviews/ps5-layout.md) | Footer focus safety, readable candidate identity, stronger selected-item hierarchy, deliberate backdrop and Control Center separation |
| Interaction/navigation | [12 findings](../.ai/runs/20261104-refresh-ps5-themes/reviews/ps5-interaction.md) | Focus/selected/pin mismatch, response focus stealing, persistent source status, controller search, Back stack, honest Switcher naming |
| Motion/rendering | [11 findings](../.ai/runs/20261104-refresh-ps5-themes/reviews/ps5-motion.md) | Immediate target association, pre-input elapsed-time scenario, synchronous preference-write debt, conditional mixed-aspect crop, measured pacing gates |
| Native themes/icons | [11 findings](../.ai/runs/20261104-refresh-ps5-themes/reviews/native-themes.md) | Explicit catalog contract, format-2 archives, DDS/zstd and SVG conversion, semantic mappings, app-owned staging and restart-to-apply |

The reports include dated official Sony references, pinned upstream code links,
native image evidence, objections and implementation orders. Their findings overlap
and are not a count of independent defects or votes. Dated 2020 pre-production
video stills and 2023–2025 Sony images are composition references, not current
physical-console timing measurements. Native desktop frames are not TV app proof.

## Changes integrated by the lead

The Home footer has Refresh apps / R, reachable by pointer and D-pad/OK. Explicit
scans run metadata work on the worker. Desktop reads the selected local prepared
root; the TV source uses the permitted SDK developer inventory and separately
opted-in private discovery. New IDs are compared only within a known matching
scope, and first/scope-changing scans establish a baseline. The library shows NEW
and naming-based BUILD CANDIDATE states, real IDs/versions and a Review builds
filter. No uninstall/removal operation exists. The current local C3 developer
snapshot yields six review candidates among 21 app records.

The first integrated native fixture independently showed a concrete defect:
the new item had focus while the pin target remained TV Home. The correction
synchronizes stable selection on library focus/open/filter/page transitions,
retains it on action buttons and names the Pin/Unpin target. Native regressions
actually exercise pinning after discovery, paging and filtering and inspect the
persisted favorite; this is stronger evidence than source review alone.

Other focused corrections:

- Scan completion auto-focuses only if the user has not navigated/edited after
  starting it. Otherwise the stable focused target is restored. A controlled
  same-frame refresh/navigation case verifies this behavior.
- Scanning, unavailable and source/scope status persist in the library. Failed
  scans keep their previous data/source visible. Incomparable scans report a
  baseline rather than a misleading numeric new-install result.
- The shared refresh rectangle moved from y=1025 to y=984. Its calculated focus
  surround now ends at y=1037, leaving 43 logical units below it. Home toast space
  is allocated to the left, so it does not overlap that action. This is source
  geometry plus native focus/pointer evidence, not a measured TV overscan pass.
- The selected ID/version is available in up to three cached wrapped lines,
  with the candidate reason where space allows. The long-ID/version fixture
  checks that the complete identity survives; per-card metadata stays compact.
- The card that opens Control Center is now named Control Center, with a distinct
  locally drawn controls glyph. The top control shortcut uses the same meaning.
  This avoids presenting an unimplemented recent-app Switcher.
- The library no longer advertises F as a pin shortcut while its text mode
  consumes F. Ordinary F/R typing remains intact; the named action is focusable.
- The synthetic sanitizer test now uses a PTY instead of `stdbuf`. The retained
  probe demonstrated ASan loader-order failure before app code; a private PTY
  qualification passed before that harness correction was integrated.

The approved fourth-column 4/3/2 staircase and five bottom cards remain. Parent
did not replace established motion timings with unmeasured PS5 guesses, add a
blur stack, change system Home, or pretend unknown store/TV data is available.

## Main remaining PS5 differences

1. **Selected content and art hierarchy.** Sony's referenced Home compositions
   give the selected item and its hub/background more weight. The app retains its
   approved custom geometry, small selected-art boost, prepared hero coverage and
   neutral fallbacks. Better selection emphasis and authorized artwork coverage
   are useful next experiments; a 4K framebuffer cannot add bitmap source detail.
2. **Library and controller text entry.** This app uses a compact modal metadata
   grid for provenance/build review. The references show a larger artwork-led
   collection. A focusable library search with the existing bounded on-screen
   keyboard pattern, page context and boundary-aware controls would improve the
   controller path. The current catalog keyboard is not a general IME.
3. **Control Center and Switcher authority.** Sony documents a system quick-access
   overlay and a recent-app Switcher. This standalone app has app-local controls,
   not OS running/suspended-app authority. Correct naming is implemented; an
   honest local recent-launch history and a return stack are separate features.
   [Official Control Center documentation](https://www.playstation.com/en-us/support/games/customize-ps5-control-center/).
4. **Layering and continuity.** The Control Center can leave Home visually busy
   behind its cards, and its scrim/content use different entry treatment. A
   stronger/coupled existing gradient is a lower-cost prototype than live blur,
   but bright/dark captures and reduced-motion behavior need comparison first.
5. **Action target versus traveling focus.** Rapid input can update the actionable
   selection before the decorative focus rectangle arrives. A bounded immediate
   target treatment deserves a separate visual/activation test. Keep immediate
   input, shared hit geometry and finite retargetable motion.
6. **Timing and persistence debt.** The idle-wait/event/dt ordering is a source-derived
   scenario requiring a controlled-clock/real-time reproduction. Synchronous
   preference saves are known interaction-path debt; an ordered writer needs
   completion, failure and shutdown tests. These were not measured C3 stalls and
   are not claimed fixed by the refresh work.
7. **Rendering compatibility and physical gates.** Mixed-aspect fused crops,
   fractional viewport edges, first-use text work and target GPU/presentation
   remain separate qualification work. Uniform prepared heroes avoid the current
   mixed-aspect input; future theme admission should preserve that contract until
   a narrow shader fix is qualified. 240 Hz is still a design target.

Suggested next order is controller library search/return behavior, deliberate
selection and Control Center hierarchy, measured event/persistence pacing fixes,
then the app-owned image/icon theme foundation. Each is a bounded change with its
own visual/interaction and performance gate, not a combined renderer rewrite.

## Theme and icon decision

Native personalization is feasible. Direct `.p5theme` application is not implemented
and the format is not simply a PNG theme folder. The inspected upstream format is
ZIP or USTAR with a format-2 manifest. Common backgrounds are Zstandard-wrapped DDS
(BC7/BC1); named system icons are SVG, with optional preview PNGs. Hashes establish
declared content integrity, not publisher signatures. PS title IDs and firmware
target paths do not identify LG apps or writable app destinations.

The lead recommends a separate app-owned theme generation: explicit local import
or catalog/download, bounded validation/conversion, semantic icon and local app-ID
mapping, atomic staging, defaults/previous-generation restore and apply on the next
manual launch. Raw images and PNG tile/icon inputs come first. DDS/zstd and exact
SVG conversion require independently sourced, license-reviewed decoder work. Audio,
video, firmware originals and PS5-only ThemeHelper/system modifications are outside
the initial feature. See [the concrete native-theme design](NATIVE-THEMES-DESIGN.md).

The study inspected one 539,090-byte upstream fixture in memory; it verified the
container and one hashed zstd payload but did not decode it or validate an icon
layer. No community pack was downloaded/applied. No compatibility or themed-runtime
acceptance is inferred from that fixture. The upstream snapshot is
`96a92a862ea68e55edc51b38d8ad19f2c8521f98`; the app remains MIT and no upstream
implementation, Sony firmware assets or fonts were copied into it.

## Evidence, recovery and open gates

Final integrated captures/results are under `evidence/refresh-scan/`. The 14-case
matrix includes the initial eight paths plus real pin-target regressions, source
failure persistence, interaction preservation and a complete identity. Broader core,
sanitizer, existing UI/motion and prepared-data checks are recorded in the final
acceptance receipt. UI leak detection remains explicitly disabled in UI ASan/UBSan
runs; full UI LSAN is still open.

The complete original 46-file review freeze and reviewed-source copies remain in
`.ai/runs/20261104-refresh-ps5-themes/`; reviewers' line references refer to that
version. Final code hashes, four exact MAX completion receipts, source/binary changes
and check-only recovery are recorded separately. Restoration refuses later drift
and retains native evidence/prepared snapshots. No live restoration, Git commit,
publication, TV installation, privilege/profile change or Home binding was performed.

ARM/TV execution, actual remote/controller mappings, protected LG sources, GPU timing,
physical presentation, full UI LSAN and human acceptance remain open for this app.
