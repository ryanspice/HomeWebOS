# Contributing to HomeWebOS

`HomeWebOS` is the native C++20 / SDL2 / raw GLES2 desktop + LG C3 (webOS) app
with ID `com.spice.home.cpp`. It is a parallel app; the existing Rust/web
launchers are not replaced. Read [AGENTS.md](AGENTS.md) — the canonical agent
entry point — and [README.md](README.md) before starting. For supported /
partial / unverified status, use [docs/PARITY.md](docs/PARITY.md).

## Workflow

1. Fork [`ryanspice/HomeWebOS`](https://github.com/ryanspice/HomeWebOS) and clone your fork.
2. Create a topic branch from the current `main` (`git switch -c my-change main`).
3. Make a focused change with targeted edits; keep unrelated work, credentials,
   saves and profile identities intact. Do not include secrets or tokens.
4. Run the checks below and include their results in the PR description.
5. Open a PR against `main` using the [pull request template](.github/PULL_REQUEST_TEMPLATE.md).
   Be prepared to respond to review and keep the branch updated by rebasing or
   merging `main` as requested.

This is a public repository: do not commit TV credentials, tokens, private
settings, redistributed fonts, prebuilt binaries from other machines, or
`.ai/` scratch (it is intentionally git-ignored).

## Required checks before a PR

Install CMake, Ninja, a C++20 compiler, `libsdl2-dev`, `libfreetype-dev`,
`libpng-dev`, `libjson-c-dev` and `libcurl4-openssl-dev`; native UI checks also
need Xvfb. Python tests are stdlib-only. Linux helpers: `./scripts/build-desktop.sh`
and `./scripts/run-linux.sh`.

```bash
# Strict Release build + CTest
cmake -S . -B build/desktop -G Ninja -DCMAKE_BUILD_TYPE=Release -DSPICE_STRICT=ON
cmake --build build/desktop --parallel 4
ctest --test-dir build/desktop --output-on-failure
./build/desktop/spice_core_tests

# Sanitizer gates (core; UI renderer checks use the ui-asan preset)
cmake --preset core-asan
cmake --build --preset core-asan
ctest --preset core-asan

# Python suites
python3 -m unittest discover -s tests -p 'test_*.py' -v
(cd companion && python3 -m unittest -v test_retropie_bridge.py)

# Native UI / visual QA under a software GL context
LIBGL_ALWAYS_SOFTWARE=1 MESA_GLES_VERSION_OVERRIDE=2.0 \
xvfb-run -a -s '-screen 0 3840x2160x24' \
python3 tests/native_qa.py --binary build/desktop/spice-home-cpp \
  --assets assets --out build/native-qa
```

Windows: use a Visual Studio 2022 Developer PowerShell with C++ tools, CMake,
Ninja and an existing vcpkg installation:
`.\scripts\Build-Desktop.ps1 -VcpkgRoot 'C:\tools\vcpkg' -Run`.

ARM/webOS: export `SPICE_WEBOS_TOOLCHAIN` to a verified SDK toolchain file and
run `./scripts/build-webos.sh`. Real ARM toolchain and target SDL2/FreeType
headers are mandatory; never weaken those gates. Staging is local only — no
upload, install or Home binding from a build script.

UI sanitizer runs intentionally disable leak detection because of a known
libdbus startup allocation; report them accurately, not as a clean full UI LSAN
pass.

## Boundaries a PR must respect

- Preserve the approved right-leaning 4/3/2 staircase beneath the fourth rail column and the five compact bottom cards; share geometry for drawing and focus.
- No file/network/subprocess I/O on the render thread during interaction; bounded, opt-in service work only.
- Public/private Luna boundaries and size/deadline limits; no shell command construction, arbitrary endpoints or token logging.
- Desktop samples, real host counters, queried TV data and unavailable values must stay distinguishable.
- No inline TV/HDMI video or physical 120 Hz claims without measurement; no font redistribution; cite artwork provenance.
- Update `docs/PARITY.md` with measured evidence rather than marker-match guesses.

## Evidence to include in a PR

- Exact commands run and pass/fail counts (CTest, Python suites, native QA).
- Host evidence — native framebuffer captures from the executable
  (`glReadPixels`), build logs, measurable counters. State the environment
  (OS, compiler, Mesa/llvmpipe, Xvfb) and that results are **host/desktop only**.
- TV/device evidence — clearly separated and labelled. If you did not test on
  the C3, say so explicitly. Never present a desktop 4K capture, an `--fps`
  flag or a software frame interval as physical GPU timing, panel refresh or a
  successful ARM launch; an IPK alone proves nothing.
- Any `docs/PARITY.md` change with measured evidence and the source of the numbers.
- Known limitations or unresolved gates, plus reproduction steps for new UI behavior.
