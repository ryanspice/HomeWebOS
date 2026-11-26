# webOS ARM build and deployment

The current working tree passed the real SDK/header/ELF gates and is installed
on the C3. See [the November 5 deployment and recovery](TV-INSTALL-2026-11-05.md).
The task-local scripts and exact SDK/compiler logs live under `.ai/evidence/20261105-tv-fixes/`
and the retained `.ai/runs/20261105-tv-install/`. The shared SDK was not modified.
Native permission and payload-byte checks are required: the first Windows package
lost its executable bit. Final packaging uses Linux staging and verified IPK modes.
The text below retains the original archive's build contract and historical limits.

The original archive was a native Linux desktop build plus a TV source candidate.
It had no ARM ELF, webOS runtime bundle or IPK. Do not install the Linux binary
on the TV or overwrite the existing Rust launcher.

## Required input

Use a verified webOS ARM toolchain and target sysroot from the Homebrew native toolchain
or the specific SDK already used by your working native TV implementation. The C++20
compiler, libc/float ABI, libstdc++, SDL-webOS, FreeType, PNG, JSON-C and optional curl
must all match that target. An arbitrary Linux ARM toolchain is not equivalent.

The project accepts the SDK's CMake toolchain file rather than inventing another ABI:

```bash
export SPICE_WEBOS_TOOLCHAIN=/your/verified/sdk/share/buildroot/toolchainfile.cmake
./scripts/build-webos.sh
```

The script configures SPICE_WEBOS=ON, requires target SDL2/FreeType headers, cross-builds,
checks ELF32/ARM and stages into a **new** `dist/webos-stage`. No upload, install, Home
binding or privileged call happens. An existing staging folder causes an explicit stop.

For an owner-specific prepared data bundle, set `SPICE_PREPARED_DATA` to its local
root before running the build. The helper verifies that bundle, then copies only
its committed generation into the new stage's `assets/prepared`. This does not
contact the TV or run collection during the build. See
[offline preparation](OFFLINE-PREPARATION-2026-11-04.md). The ARM/header/runtime
gates above still apply; data preparation does not produce or install an IPK.

## Before packaging

Inspect `build/webos/elf-dependencies.txt` and every target shared library. SDL and
FreeType are dynamically loaded and will **not** appear in DT_NEEDED, so also explicitly
check the SDL-webOS and FreeType SONAMEs used in `src/runtime.cpp`.

Resolve transitive dependencies against the verified device runtime. Put only vetted,
compatible redistributable target libraries in `dist/webos-stage/lib` as necessary.
The executable has `$ORIGIN/lib` runtime search support. Never copy host x86_64 libraries,
glibc from another distribution, font files from this environment, or unrelated binaries.
Preserve all applicable dependency licenses. This repository deliberately does not guess
which libraries the user's C3 jail already provides.

A readable, installed font is needed. Test `--font` with an appropriate font available
on your device or one you are entitled to use in your own local build. No fonts are shipped.

Only **after the runtime audit and a real ARM build** may the normal webOS CLI package it:

```bash
ares-package -n -o dist dist/webos-stage
```

That command is not run by this release. An IPK is not proof of successful launch.
App ID must remain `com.spice.home.cpp` for this experiment.

## First device acceptance

1. Launch as a separate application. Preserve the current Home binding and recovery route.
2. Capture Diagnostics: GL renderer/version, actual window and drawable pixels, accepted
   swap interval, frame-interval sample, runtime library versions and build identity.
3. Verify D-pad/OK/Back, pointer at all corners, wheel, focus loss and restoration.
   Record real SDL scancode names. Configure only observed mappings.
4. Test a selected app launch and read-only settings query. ACG declarations are not
   permission proof. Validate luna-send-pub availability and the caller identity in the jail.
5. Leave private discovery off initially. If explicitly approved on the rooted device,
   enable SPICE_CPP_PRIVATE_LUNA=1 and record actual failures/success without bypassing ACLs.
6. Observe idle/sleep/OLED protection and app switching. This prototype has no verified
   native screensaver integration; do not leave it unattended on an OLED for long periods.
7. Measure physical 120 Hz presentation only with appropriate device evidence. A 120 Hz
   panel, a --fps flag or a desktop 4K screenshot proves none of that.

No root shell, system-app replacement, binary injection or automated Homebrew updates
are part of this port. Direct LS2 transport and a native media/video-plane backend are
separate future work; the current bounded CLI bridge and poster/handoff mode must be
labelled accurately.
