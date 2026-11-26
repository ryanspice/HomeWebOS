## Summary

<!-- What changed and why? Keep it focused. Mention any behavior/UX impact. -->

## Verification

<!-- Exact commands run and pass/fail counts. Check all that apply. -->

- [ ] Strict Release build (`cmake -S . -B build/desktop -G Ninja -DCMAKE_BUILD_TYPE=Release -DSPICE_STRICT=ON` && build)
- [ ] CTest (`ctest --test-dir build/desktop --output-on-failure`)
- [ ] Core ASan/UBSan (`cmake --preset core-asan` + build + `ctest --preset core-asan`)
- [ ] UI ASan/UBSan renderer checks (`ui-asan` preset)
- [ ] Python suites (`unittest discover -s tests` and companion suite)
- [ ] Native UI/visual QA (`tests/native_qa.py` under Xvfb / software GL)
- [ ] Not run (explain below)

Commands / results:

```

```

## Evidence — host vs TV

**Host / desktop evidence:** <!-- native captures, logs, counters, environment (OS, compiler, Mesa/llvmpipe, Xvfb) -->

**TV / device evidence:** <!-- C3 checks, if any; write "none — not tested on device" if not -->

Desktop captures and Mesa `llvmpipe` timings are **not** proof of target-GPU
performance, physical 120 Hz presentation or a successful ARM launch. Do not
present an `--fps` flag, a 4K framebuffer capture or an IPK as device evidence.

## Checklist

- [ ] Right-leaning 4/3/2 staircase and five compact bottom cards preserved; shared geometry used for drawing and focus.
- [ ] No file/network/subprocess I/O on the render thread during interaction; service work remains bounded and opt-in.
- [ ] Public/private Luna boundaries and size/deadline limits respected; no shell command construction, arbitrary endpoints or token logging.
- [ ] Desktop samples, real host counters, queried TV data and unavailable values remain distinguishable; no browser screenshots used as native evidence.
- [ ] Real SDK headers used for any ARM/webOS build; ARM/header gates not weakened.
- [ ] No inline TV/HDMI video or physical 120 Hz claim without measurement.
- [ ] No fonts redistributed; artwork provenance cited.
- [ ] `docs/PARITY.md` updated with measured evidence if behavior/status changed.
- [ ] No secrets, tokens, private settings or `.ai/` scratch included.
- [ ] Branch was created from `main`; PR targets `main`.
