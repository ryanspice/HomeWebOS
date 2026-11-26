# Native responsiveness and language decision — November 5, 2026

Status: **research accepted; implementation and dynamic device acceptance pending**.

The owner confirms that arrows and Back work, but rejects the overall installed
r13 experience: slow/sluggish response, missing animations and stuttery navigation
or scrolling. The earlier functional/build results remain valid on their pinned
bytes; they do not establish a fluid product. Processor Deck remains off.

## Decision

**The full Rust-to-C++ replacement was premature on the evidence available.**
No Rust-language bottleneck was isolated. Raw GLES access was already possible
in Rust, and SDL's renderer API supports high-DPI output. The rewrite also used an
older 0.3.1 parity baseline while the actual Rust checkout contains a substantial
dirty 0.3.5 continuation. Architecture and feature differences were mixed with
the language change.

Pause C++ feature expansion. Make one bounded architecture experiment that brings
forward the Rust static/dynamic layer split, event waiting, input policies and
prebuilt text approach, while retaining qualified C++ data preparation, themes,
ordered saves and the owner's monitoring-off policy. Keep Rust as the matched
native reference. Reuse designs/tests first; an FFI layer or another framework
rewrite is not the initial optimization.

This does not select a final language winner. A rollback would also need to fix
Rust's background probes, synchronous saves, held-axis repeat and fixed target/
atlas density. A matched dynamic device comparison decides the canonical build.

## Evidence supporting this direction

- C++ invalidates its whole Home cache on every focus target; changing artwork
  bypasses it, and panels redraw Home underneath. Rust's default path keeps
  selection/focus composition outside its stable target.
- Actual r13 Release objects, forced through 48 synthetic desktop frames, submit
  5 draws/frame for settled Home, 45.8 for focus movement with the same artwork,
  239.0 with changing artwork, 287.0 for Control and 326.0 for Library. These are
  source-work counts, not relative speeds or idle CPU measurements.
- A real desktop-binary blur/direction/OK/focus script changes the target and
  posts one launch while inactive; the desktop backend blocks the external action.
  Recovery rejection currently does not stop normal action dispatch. The exact
  native popup/timestamp path is not reproduced by that host fixture.
- C++ requeues the event returned by WaitEventTimeout and sleeps uninterruptibly
  during pacing. Rust carries the waited event directly into dispatch and waits
  through SDL. Neither checked loop has complete time-driven held-axis repeat.
- The actual focus/clock objects demonstrate load-dependent timeline stretch.
  Some visible motion is also absent: page/return snaps, immediate tile emphasis
  and missing rail/tab/title behaviors differ from the newer Rust work.
- Rust prebuilds glyph atlases; C++ still creates complete string textures on
  misses, with glyph/raster/upload work available inside the interaction path.

## Platform conclusions

LG documents 1920x1080 UHD graphics separately from 4K video in its public web-app
resolution guide. That is not sufficient to declare native SDL 4K composition
impossible. Our 4K drawable, 1080p compositor PNG, framebuffer virtual storage and
advertised DRM modes are different observations. Establish the entire chain
before paying four times the pixels or claiming physical 4K presentation.

The pinned SDL-webOS Wayland GLES implementation forces EGL interval 0 and
implements nonzero SDL swap requests through frame callbacks. Its reported -1 is
not a physical adaptive-refresh certificate. Test supported pacing/double-buffer
modes on the real dynamic scene; preserve responsive event waiting.

Mali render-pass/resource guidance supports grouped updates and minimizing target
churn. Full-screen caches also consume bandwidth: reading the current 4K RGBA
cache once at 60 fps is a derived 1.99 GB/s before other traffic. CPU submission
clocks cannot establish GPU time. Use asynchronous supported timer queries and
keep unavailable measurements explicitly unavailable.

## Next work and decision gate

The [implementation plan](../.thoughts/native-webos-performance-plan.md) specifies
small input, measurement, layering, text and motion passes. First fix covered-input
dispatch and event order. Then qualify actual navigation/artwork/overlay workloads
under ordinary App Manager launches, with matching Rust/C++ resolution, assets,
font, geometry, effects and monitoring state. Keep the staircase and five cards.

If the repaired C++ design meets the bar, retain it and stop the duplicate product
line. If Rust wins at equal quality and the bounded C++ repair does not, make Rust
canonical and port the useful data/safety work back. If both fail at 4K, investigate
the display/compositor and GPU costs before changing language again.

No production application code, installed app or TV setting changed in this
research pass. A read-only C3 query found firmware/SDK unchanged and SkiFree
foreground; it was left undisturbed. Physical smoothness remains unaccepted.

## Sources and retained originals

The accepted Rust identity is the dirty `integrate/app-designs-035-20261103`
working tree at HEAD `47d520fe2bf003487f9922d9a86a4be2631b24da`, with individual
source hashes in the delegate's evidence. Its raw report names a different
branch in prose; the structured source receipt and lead verification agree on
the identity above. The preserved raw report is not rewritten during synthesis.

- [Lead platform/source report](../.ai/outbox/20261105-native-webos-research/native-webos-performance.md)
- [Independent Rust/C++ report](../.ai/outbox/20261105-native-webos-research/rust-vs-cpp.md)
- [Actual-code work counters](../.ai/evidence/20261105-native-webos-research/counter-results/workload-counters.json)
- [Inactive-input reproduction](../.ai/evidence/20261105-native-webos-research/inactive-input-report.json)
- [Read-only display metadata](../.ai/evidence/20261105-native-webos-research/display-readonly.json)
- [LG resolution specification](https://webostv.developer.lge.com/develop/specifications/app-resolution)
- [Pinned SDL swap path](https://github.com/webosbrew/SDL-webOS/blob/8f8da641e197c97a48c46c065e9a69941657bc29/src/video/wayland/SDL_waylandopengles.c#L60-L205)
- [SDL high-DPI renderer output](https://wiki.libsdl.org/SDL2/SDL_GetRendererOutputSize)
- [Arm framebuffer guidance](https://developer.arm.com/community/arm-community-blogs/b/mobile-graphics-and-gaming-blog/posts/mali-performance-2-how-to-correctly-handle-framebuffers)
- [Khronos GPU timer specification](https://registry.khronos.org/OpenGL/extensions/EXT/EXT_disjoint_timer_query.txt)
