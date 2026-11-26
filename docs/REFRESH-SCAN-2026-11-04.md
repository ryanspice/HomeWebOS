# Explicit refresh and prior-launcher review — 2026-11-04

Home now has a **Refresh apps / R** shortcut in the footer. Pointer, D-pad/OK and
the R key activate the same action. The approved 4/3/2 staircase and five compact
bottom cards keep their existing geometry. R remains ordinary text input while a
library or concept search accepts text.

Refresh opens the library, scans on the service worker, and presents the source,
app count, new-item count and build-candidate count. Repeated activation while a
scan is pending is ignored. If the user has not navigated/edited during the request,
the first new item receives focus; if there are none,
the first build candidate is selected. The **Review builds** control filters the
library to candidates, and **Show all apps** returns to the full collection.

New items receive a green marker and NEW label. Candidate builds receive an amber
outline and BUILD CANDIDATE label; a card can carry both states. Real app IDs and
versions are displayed so similarly named builds can be distinguished. A cached,
wrapped selected-identity area preserves the full ID/version; the named Pin/Unpin
action retains that stable target when focus moves to its button. Candidate
classification is a naming heuristic, with reasons recorded in the native report.
There is no removal or uninstallation operation in this flow.

Known candidates include the earlier Spice Home ID, PS5 Home and its Flutter build,
XMB/dashboard, Orbis/Metro/TV Home and named launcher/dock families. Conservative
Home/Start/launcher title matching is limited to known owner namespaces. Current
`com.spice.home.cpp`, LG system apps, Homebrew Channel, ordinary apps and illustrated
samples are excluded. Different launchers may be intentionally retained; the badge
identifies an item for review rather than deciding its status.

## Source and comparison behavior

- On the desktop, refresh rereads metadata from the configured prepared-data root.
  It does not contact the TV. Run the explicit preparation tool after a TV app
  install/update so the local snapshot can expose the new record.
- In a webOS build, refresh explicitly requests the supported SDK
  `luna://com.webos.applicationManager/dev/listApps` route. Separately opted-in
  private discovery can use the global launch-point list where permissions allow.
  Failure stays visible; no privileges or caller identities are changed.
- Startup still sends no automatic TV metadata request. A scan compares IDs only
  when there is a baseline for the same inventory scope. Changing between developer,
  private-full or unknown sources establishes a baseline instead of labelling the
  newly visible scope as newly installed apps.
- New badges remain during the session and disappear when an item leaves that
  source. A scan of unchanged data does not repeatedly classify existing entries as new.
- Prepared icon/background textures remain the startup set. The scan loader checks
  pointer/manifest/metadata hashes and bounded asset descriptors without opening
  artwork files. New graphics use neutral fallbacks until an explicit preparation
  and relaunch loads the new generation. Metadata-only data cannot be passed to
  the renderer as validated graphics.

The real local C3 snapshot currently contains 21 developer-app records. Six match
the review heuristic. These are prepared data, not a new live TV installation scan
from the desktop. Protected LG Home/global store inventory remains unavailable through
the configured Developer Mode connection; source availability is still recorded by
the preparation tools.

## Verification

Source is pinned for the requested research tasks in
`.ai/runs/20261104-refresh-ps5-themes/review-code-freeze.json`. Parent owns integration;
reviewers do not edit shared application sources. Native captures and logs are in
`evidence/refresh-scan/`.

Strict Release and ASan/UBSan CTest pass 6/6 targets: 53 existing core scenarios,
14 prepared-data scenarios, seven inventory scenarios and three renderer checks.
The integrated native refresh matrix passes 14/14 in each build. It covers footer
clicks at 720p/4K, R, D-pad access, review filtering, R-as-search-text, new items,
pin identity after discovery/paging/filtering, intervening user navigation, persistent
failure and complete selected identity. Synthetic cases load owner-import generation
A and atomically publish generation B without reloading graphics; they are not
evidence of new TV installations. The ordinary native suite passes 56/56, motion
33/33 and prepared-data regression 12/12. Final hashes and recovery guards are
recorded in the completed acceptance receipt.

No app was uninstalled, no TV installation or Home binding changed, and no Git
commit/publication was performed. The native desktop tests and fixed-step captures
do not establish physical 240 Hz or C3 GPU performance. Existing full UI LSAN remains
open; UI sanitizer checks explicitly retain address/undefined checks with leak
detection disabled.

## Requested independent perspectives

Exactly four GPT-6.1 Terra MAX tasks were requested: spatial/visual PS5 comparison,
interaction/navigation comparison, motion/rendering comparison, and native theme/icon
integration of Darkatek7's site and ProsperoThemes. The task packet, exact target
settings, terminal receipts and owned reports live under
`.ai/runs/20261104-refresh-ps5-themes/`. The theme work investigates app-local asset
and icon support; PS5-only system-file helpers are outside the standalone app design.

All four completed. [The synthesis](PS5-THEMES-SYNTHESIS-2026-11-04.md) distinguishes
their original 46-file review version from the lead's later integrated corrections.
Custom-theme import/apply is not implemented; [the native design](NATIVE-THEMES-DESIGN.md)
records the conversion, icon mapping, admission and startup work needed.
