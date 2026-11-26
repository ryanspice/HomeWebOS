# Native implementation and demo fleet — November 5, 2026

This phase implements the next useful PS5-inspired launcher gaps and delivers
working theme and profile demos. It preserves the approved fourth-column 4/3/2
staircase, five bottom cards, separate app identity and existing TV installations.
There is no usable project Git HEAD; no Git initialization, commit, push,
publication, TV install, privilege change or provider/profile repair occurred.

## Implemented behavior

Library Search is a focusable action with the bounded A–Z/0–9/Space controller
keyboard. Back closes the keyboard while retaining the query; Show matches
focuses a result. Down from the search action selects the first visible result,
or Refresh when the result list is empty. Hardware text entry and UTF-8 deletion remain supported.
Empty Delete keeps the keyboard open. This is not a general platform IME.

An eight-entry app-local return history retains view, focused node, selected app,
page, query, review filter and settings category. Control Center toggles back to
its invoker. Nested panels, confirmations and the design collection return through
that context. Missing page directions are omitted, item/page context is visible,
and the selected launch/pin target stays synchronized. An immediate accent edge
identifies the actionable target while the decorative focus surround travels;
the library keeps a selected-item mark when an action button has focus.

Control Center uses a stronger, coupled existing gradient. The toast sits above
its cards rather than covering the focused-control label. Its first control names
Return when it will return to a non-Home view. No running/suspended-app Switcher
or PlayStation system authority is implied.

Live animation advancement is separated from service/dwell processing. Existing
presentation tracks consume their time before the input batch; queued input is
handled before services or native-input dwell. Newly started tracks begin at
age zero. An interval preceding interaction is not credited to a new dwell
target. Fixed-step QA preserves its event-then-step semantics, reduced motion
settles before painting, and hidden time remains excluded. The 50 ms stall clamp
remains. No PS5 animation timing or physical refresh measurement is inferred.

Settings saves now submit immutable JSON snapshots to one background worker,
with one in-flight and one coalesced pending job. The UI tracks sequence-specific
pending/writing/saved/failed states; older acknowledgments cannot replace the
current result. Malformed startup files remain protected. Orderly exit drains
the latest job before the final report and returns an error on a failed submitted
save. Unknown allowed settings fields survive. Atomic replacement is not a
power-loss durability guarantee, and a blocked filesystem can block final drain.

## Native themes

The actual launcher now preloads a validated local theme before the first frame.
It supports an optional accent, opaque background, six semantic icon slots and
exact app-ID hero/icon overrides, with prepared and procedural fallbacks. Runtime
theme replacement is not attempted. [Commands and contract](NATIVE-THEMES-IMPLEMENTATION.md)
cover import, default/previous selection and relaunch-to-apply.

The original [Geometric Blue illustration](../demos/themes/demo.html) is separate
from the actual native framebuffer evidence in `evidence/parity-fleet/theme-final-v2/`.
The 4K native fixture adds one clearly synthetic `com.example.geometric` record
to the existing local 21-record developer snapshot to test mappings. The image
does not prove 22 installed TV apps or a fresh TV scan.

The store admits 24 MiB RGBA including a crossfade reserve and 16 MiB compressed.
The fixture uploads 11,978,752 actual theme texture bytes on the tested desktop;
its admitted accounting is 17,738,752 bytes including reserve. Prepared data,
text, temporary decode memory and driver overhead are separate. Bitmaps are
normalized to 1600x900 backgrounds and 128x128 icons; 4K output does not create
additional source detail. Partial uploads roll back; late uploads are rejected;
compressed buffers are released while stable mapping records remain alive.

Direct `.p5theme`, DDS/zstd, SVG, sound and video compatibility remain unimplemented.
The [original source-pinned research](PS5-THEMES-SYNTHESIS-2026-11-04.md) supports
the next converter/catalog work. No GPL implementation, firmware-original artwork,
ThemeHelper or font files were copied into this app.

## Profile page and available web data

Open the standalone [player profile dashboard](../demos/profiles/index.html).
It supports handle editing, explicit public-profile links, local JSON import/
export, per-field source/time, labelled samples, keyboard navigation and reduced
motion. Native profile-panel integration is separate; the page is a browser demo.

The supplied `pikldspratmn` is treated as an owner-provided, unverified Xbox handle.
Anonymous Microsoft pages returned a generic shell without a matching structured
profile; no avatar, score, achievements or games were verified. An additional
indexed public lookup found no results. This establishes neither account absence
nor any particular privacy setting. Microsoft documents its
[profile settings API](https://learn.microsoft.com/en-us/gaming/gdk/docs/reference/live/rest/uri/profilev2/uri-usersbatchprofilesettingspost?view=gdk-2604)
as requiring authentication. No PSN online ID is confirmed; that public connector
remains untested. The supplied email was not forwarded to workers, searched or
placed in demo files. No sign-in or credential extraction was attempted.

The explicit [preparation command](../demos/profiles/README.md) uses ordinary TLS,
fixed public origins, same-handle redirect restrictions, size/deadline bounds,
no cookie jar and atomic local snapshots. Failed refresh preserves last-good
fields with stale status. The HTML parser's successful structured-data path has
synthetic coverage; a real successful account-data lookup remains open.

Lead review reproduced an avatar admission defect: a valid 4096x4096 PNG under
the compressed 96 KiB ceiling was accepted. Both the CLI and page now inspect
still-image headers/containers and cap each side at 1024 pixels before browser
decode. PNG/WebP animation and compressed PNG metadata are rejected. Bounded
undecodable images fall back to platform initials. Header admission is not full
codec validation; no remote avatar URL is fetched. The final browser checks
actually decode small PNG/JPEG/WebP fixtures and reject oversized/animated ones.

## Fleet, integration and evidence

Six owner-authorized, disjoint T3 tasks used exact configured routes, at most
three children plus lead. The three native Terra tasks used
`codex-native/gpt-6.1-terra` with `reasoningEffort=max`; the Fugu council used
the required T3 surface with exact OpenCode routes. T3 accounting/receipts are
distinct from the standalone Fugu adapter. Expired historical lanes were not
enabled; no route fallback or extra child was launched.

- [Theme implementation](../.ai/runs/20261104-parity-fleet/reports/theme.md): useful
  loader/preparer/demo code and producer checks; lead integrated startup/UI.
- [Profile implementation](../.ai/runs/20261104-parity-fleet/reports/profile.md): useful
  page/collector code and public-request evidence; lead added avatar limits.
- [Motion/writer handoff](../.ai/runs/20261104-parity-fleet/reports/terra-motion.md):
  useful clock/writer code, but its full pre-input update was not acceptable.
  Lead replaced it with presentation-only advancement plus post-input side effects.
- [Mercury proposals](../.ai/runs/20261104-parity-fleet/reports/mercury-ux.md), route
  `opencode/inception/mercury-3`, variant high: substantive controller/code proposals.
- TokenHarbor DeepSeek, route `opencode/tokenharbor/deepseek-v4.1-flash:free`,
  variant high: failed after preliminary source reads because the free allowance
  was exhausted. No report, proposals or substantive findings were delivered.
  The provider reported a reset on November 11, 2026 at 04:23 UTC. No retry or
  paid-route substitution was made; see the [terminal failure receipt](../.ai/runs/20261104-parity-fleet/deepseek-failure.json).
- Crouton, route `opencode/opencode/crouton-alpha-free`, variant high: T3 marked
  the run completed, but it returned only an inventory update. No report or
  findings exist; this review is classified incomplete and is not acceptance.

All six tasks are terminal: four substantive handoffs, one incomplete review and
one quota-failed review. There are no pending fleet receipts. The two unavailable
reviews provide no additional acceptance; native theme confidence remains tied
to the producer's implementation and the lead's source, sanitizer and native
checks. Full feature parity and the separate device/human gates below remain open.

Original review snapshots and handoff preimages are retained under
`.ai/runs/20261104-parity-fleet/`. Lead changes and final hashes are separate.
Model agreement does not replace the following checks.

| Gate | Final evidence |
|---|---|
| Strict Release build, actual desktop SDK headers | PASS; `evidence/parity-fleet/build-release-final-v2.log` and `build-release-v2-main.log` |
| ASan/UBSan build | PASS; `build-sanitize-final-v2.log` and `build-sanitize-v2-main.log` |
| CTest | 11/11 Release and 11/11 sanitized; core leak checks enabled, UI renderer leak checks explicitly disabled |
| Existing native UI | 56/56 Release |
| Motion/geometry/lifecycle | 33/33 Release and sanitized |
| Inventory/refresh/pin identity | 14/14 Release and sanitized |
| Prepared local data | 12/12 Release |
| New navigation/final settings flush | 16/16 Release and sanitized |
| Native theme startup/mapping/fallback | 9/9 Release and sanitized |
| Live/fixed-step native pacing | 4/4 Release and sanitized; first new artwork progress 0 in the live fixture |
| Theme preparation | 14/14 Windows Python; C++ loader is also a strict/sanitized CTest entry |
| Profile preparation | 18/18 Python after lead fix |
| Profile UI | 26 isolated headless Edge interactions after lead fix; no page network resources |

The seven integrated native input-order/settings checks include actual Deck
instances near the supported 3-second dwell threshold: no-input launch remains
possible, while queued Back, blur and navigation prevent the old interval from
triggering it. The desktop service only records/blocks the command; no TV path
was operated. Real Deck tests also cover rapid saves/extra fields, malformed
original preservation and final replacement failure without false success.

Retained qualification failures are distinct from app defects: the first native
theme driver needed Windows Pillow in WSL and was changed to read producer
fixtures while the C++ loader validates them; the first dwell fixture used a
1-second value that actual preferences clamp to 3 seconds; a damaged PNG proved
unsuitable for the codec-failure browser test because Chromium recovered it,
so that negative control now uses a bounded JPEG with no scan. All final checks
above pass on their stated artifacts. Initial failing receipts remain retained.

## Remaining parity and device work

The next useful increments are bounded catalog/archive inspection and converters;
a native profile view once useful owner data is available; broader controller
text input/localized IME; richer authorized artwork; and target-device pacing/
latency measurements. App-local history is not OS recent-app authority. Full LG
Home/store inventory, protected APIs, PlayStation social/trophy authority and
authenticated Xbox data are not supplied by this standalone app.

ARM/webOS execution, physical controller/Magic Remote mappings, GPU timing,
physical 240 Hz, full UI LSAN and human visual acceptance remain open. ThreadSanitizer
could not execute in the worker's WSL runtime (`unexpected memory mapping`).
The separate webOS ARM/header gates remain intact. No Linux executable is a TV IPK.

The [change receipt](../evidence/parity-fleet/change-receipt.json) and check-only
recovery helper retain baseline preimages and refuse later file/hash drift.
No live restoration is run. Native evidence, run receipts and prior prepared
generations are kept. The profile worker's optional recursive QA cleanup was
automatically rejected with only “blocked by policy”; that exact isolated
temporary directory remains retained rather than retried.
