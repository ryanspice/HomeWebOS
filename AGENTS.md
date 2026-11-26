# HomeWebOS — canonical agent entry point

`HomeWebOS` is the formal repository/project name (GitHub `ryanspice/HomeWebOS`);
this checkout is `B:\Dev\HomeWebOS`. It is a native C++20 / SDL2 / raw OpenGL ES 2
desktop + TV application for the LG C3 (webOS), app ID `com.spice.home.cpp`.
It is a parallel app, not an upgrade to `com.spice.home` or
`com.ryanspice.ps5home`; the existing Rust/web launchers are not replaced.

Read `README.md`, `docs/ARCHITECTURE.md`, `docs/PARITY.md`,
`docs/VERIFICATION.md` and `docs/WEBOS-BUILD.md` before claiming deployment
readiness. `docs/PARITY.md` holds the authoritative supported / partial /
unverified distinctions. The `.ai/` directory is intentionally git-ignored:
evidence, captures, handoffs and scratch live there and are not committed.

## Scope of an authorized task

A scoped code task authorizes changes in this project only. It does not authorize
modifying the owner's other checkout, resetting local edits, editing governance
files, installing root services, Home takeover, network credentials or TV/app
updates. Keep render-thread work free of network, subprocess and file I/O during
interaction; service work is bounded (size/deadline limits) and starts only on
explicit requests.

## Non-negotiable boundaries

- Preserve the approved right-leaning 4/3/2 staircase aligned beneath the fourth
  rail column and the five compact bottom cards. Use the shared geometry for both
  drawing and focus/hit testing.
- Keep public/private Luna boundaries and opt-ins. Private installed-app and HDMI
  queries require `SPICE_CPP_PRIVATE_LUNA=1`; permission failure remains normal.
  No shell command construction, no arbitrary service endpoint settings, no ACL
  grants or privileged calls, and never log tokens.
- Keep desktop sample tiles, real host counters, queried TV data and unavailable
  values distinguishable. Do not substitute browser screenshots for native
  evidence. Never label a sample app as installed on the TV.
- Use actual SDK headers for ARM/webOS builds. `SPICE_WEBOS=ON` rejects the
  desktop ABI fallback and host compilers. Do not disable ARM/header/ELF gates to
  manufacture an IPK; an IPK is not proof of a successful launch.
- Keep inline TV/HDMI video and physical 120 Hz output unverified until actual
  device measurements. `--fps` is only a software pacing ceiling; desktop 4K
  captures and Mesa `llvmpipe` timings do not prove GPU time or panel refresh.
- Do not distribute fonts. Use installed/authorized fonts locally (`--font`) and
  cite artwork provenance. No Proprietary console assets are claimed.
- Run strict-warning and sanitizer gates first, then native UI checks. Update
  `docs/PARITY.md` with measured evidence rather than marker-match guesses.
  Distinguish host evidence from device evidence in every report.

## Build and test

Desktop dependencies: CMake, Ninja, a C++20 compiler, `libsdl2-dev`,
`libfreetype-dev`, `libpng-dev`, `libjson-c-dev`, `libcurl4-openssl-dev`; native
UI tests also use Xvfb (Python tests are stdlib-only). Linux helpers:
`./scripts/build-desktop.sh`, `./scripts/run-linux.sh`.

```bash
cmake -S . -B build/desktop -G Ninja -DCMAKE_BUILD_TYPE=Release -DSPICE_STRICT=ON
cmake --build build/desktop --parallel 4
ctest --test-dir build/desktop --output-on-failure
./build/desktop/spice_core_tests

cmake --preset core-asan          # SPICE_SANITIZE=ON, UI off
cmake --build --preset core-asan
ctest --preset core-asan
# cmake --preset ui-asan ... for the native renderer checks under ASan/UBSan

python3 -m unittest discover -s tests -p 'test_*.py' -v
(cd companion && python3 -m unittest -v test_retropie_bridge.py)

LIBGL_ALWAYS_SOFTWARE=1 MESA_GLES_VERSION_OVERRIDE=2.0 \
xvfb-run -a -s '-screen 0 3840x2160x24' \
python3 tests/native_qa.py --binary build/desktop/spice-home-cpp \
  --assets assets --out build/native-qa
```

UI sanitizer runs explicitly disable leak detection (a known libdbus startup
allocation); do not report them as a clean full UI LSAN pass.

Windows: Visual Studio 2022 Developer PowerShell with C++ tools, CMake, Ninja and
an existing vcpkg installation — `.\scripts\Build-Desktop.ps1 -VcpkgRoot 'C:\tools\vcpkg' -Run`.

Target build: export `SPICE_WEBOS_TOOLCHAIN` to the SDK toolchain file and run
`./scripts/build-webos.sh`. It requires a real ARM cross toolchain plus target
SDL2/FreeType headers, stages only into a new `dist/webos-stage`, and performs no
upload/install/Home binding.

## Read-only helper

`python tools/agent/adapt.py inspect --target PATH --out NEW_EXTERNAL_REPORT_DIR`

Settings migration always writes a new file, drops credentials and disables
handoff. No tool in this package blindly overlays files into the target checkout.

See `CONTRIBUTING.md` and `.github/PULL_REQUEST_TEMPLATE.md` for the PR workflow
and evidence requirements.
