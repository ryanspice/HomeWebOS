# Claude Code — HomeWebOS

Canonical instructions: **[AGENTS.md](AGENTS.md)**. Read it in full before making
changes; also `README.md`, `docs/ARCHITECTURE.md`, `docs/PARITY.md`,
`docs/VERIFICATION.md` and `docs/WEBOS-BUILD.md` before claiming deployment
readiness. This file is a pointer, not a replacement.

Non-negotiable boundaries (details and commands in `AGENTS.md`):

- App ID `com.spice.home.cpp`; parallel app, not a replacement for the Rust/web launchers.
- Preserve the approved right-leaning 4/3/2 staircase under the fourth rail column and the five compact bottom cards; share geometry for drawing and focus.
- No file/network/subprocess I/O on the render thread during interaction; bounded, opt-in service work only.
- Respect public/private Luna boundaries and size/deadline limits; no shell command construction, no arbitrary endpoints, never log tokens.
- Keep desktop samples, real host counters, queried TV data and unavailable values distinguishable; no browser screenshots as native evidence.
- Real SDK headers for ARM/webOS builds; never disable ARM/header gates to claim an IPK.
- No inline TV/HDMI video or physical 120 Hz claims without measurement.
- Do not redistribute fonts; cite artwork provenance.
- Strict warnings + sanitizer gates, then native UI checks; update `docs/PARITY.md` with measured evidence and distinguish host from TV results.
- `.ai/` is intentionally git-ignored (evidence/scratch/handoffs).
