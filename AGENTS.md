# Spice Home C++ agent entry point

This is a separate C++20/SDL2/raw GLES2 prototype with app ID `com.spice.home.cpp`.
The existing Rust/web launchers are not replaced. Read README.md, docs/PARITY.md,
docs/VERIFICATION.md and docs/WEBOS-BUILD.md before claiming deployment readiness.

A scoped code task authorizes changes in this new project only. It does not authorize
modifying the user's other checkout, resetting local edits, editing governance files,
installing root services, Home takeover, network credentials or TV/app updates.

- Preserve the approved right-leaning staircase aligned beneath the fourth rail column
  and the five compact bottom cards. Use the shared geometry for drawing and focus.
- Keep render-thread work free of network/subprocess/file operations during interaction.
- Keep public/private Luna boundaries, size/deadline limits and opt-ins. No shell command
  construction and no arbitrary service endpoint settings. Never log tokens.
- Desktop sample tiles, real host counters, queried TV data and unavailable values must
  stay distinguishable. Do not substitute browser screenshots for native evidence.
- Use actual SDK headers for target builds. Do not disable ARM/header gates to claim an IPK.
- Keep inline TV/HDMI video and physical 120 Hz output unverified until actual measurements.
- Do not distribute fonts. Use installed/authorized fonts locally and cite artwork provenance.
- Run the core tests under strict warnings and sanitizers, then native UI checks. Update
  docs/PARITY.md with measured evidence rather than marker-match guesses.

Read-only helper:
`python tools/agent/adapt.py inspect --target PATH --out NEW_EXTERNAL_REPORT_DIR`

Settings migration always writes a new file, drops credentials, and disables handoff.
No tool in this package blindly overlays files into the target checkout.
