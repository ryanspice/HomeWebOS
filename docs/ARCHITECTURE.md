# Architecture

## Layers

`src/core.cpp` owns pure layout geometry, directional navigation, high-DPI transforms,
preferences, input validation, frame statistics and the preview state machine. No SDL,
GL or TV service is required to test it.

`src/services.cpp` owns a bounded worker queue (8 requests / 16 completions). The render
thread submits requests and drains results. Public Luna operations are allowlisted;
private reads need an explicit webOS-only opt-in. A subprocess uses posix_spawn argv,
not a shell; stdout is drained nonblockingly, limited to 128 KiB and terminated at a
4-second deadline. Oversized and stalled subprocess groups are killed and reaped.
No arbitrary command/URI can be supplied through app settings.

`src/runtime.cpp` loads SDL2 and FreeType and checks required symbols. `src/main.cpp`
owns SDL window/context/controller lifetimes, input translation, focus lifecycle,
VSync selection, fallback pacing and native diagnostic capture. SDL_SetMainReady is
called explicitly for the Windows SDL_MAIN_HANDLED path. webOS's initial native JSON
argument is validated against this app ID and accepted without interpreting it as an option.

`src/render.cpp` directly issues GL shader/buffer/texture calls. It is not SDL_Renderer.
Rounded rectangles, strokes, gradients, image crossfades and glyph textures share a
quad path. The original small artwork set is preloaded; text textures use drawable
pixel density and a 24 MiB / 384-entry cache. Estimate counters are RGBA texture storage,
not driver process memory. There is no continuous particle/background animation loop.

`src/ui.cpp` is the native screen/state controller. It shares hit rectangles with
D-pad navigation. Service responses change data; drawing does not block on service IPC.
Panel motion uses opacity/translation; focus moves by time-based smoothing; artwork
crossfades use elapsed time. All interactive targets have explicit focus nodes.

## DPI and frame pacing

Window coordinates and physical GL drawable dimensions are distinct. The design space
is 1920×1080, centered with letterboxing. A pointer is transformed once from SDL window
coordinates into physical drawable space and then into design space. It is not passed
through SDL_RenderSetLogicalSize, since no SDL_Renderer exists here.

A 3840×2160 drawable therefore draws vector shapes and font textures at 2× density,
not a single stretched 1080p screenshot. Original background JPEG decodes are only
1600×900 and are naturally upscaled; source art is not made 4K by changing the renderer.

Adaptive VSync is requested, then ordinary VSync. A reported failure gets an explicit
software render ceiling. Observed frame intervals are diagnostic measurements, not a
panel refresh probe. Foreground idle work is sparse; hidden/unfocused frames are not drawn.

## Why a desktop ABI fallback exists

This environment had working SDL2/FreeType runtime libraries but no corresponding
SDK headers, and dependency downloads failed. `compat/native_abi.hpp` contains only
narrow public declarations needed for this bootstrap. The host implementation was
compiled, executed and tested against those actual runtimes. This is not a substitute
for complete SDL or FreeType development packages.

With both SDK header sets available CMake uses them instead. **SPICE_WEBOS=ON refuses
the declaration fallback and refuses a host compiler.** No ARM ABI compatibility was
inferred from the x86_64 run. Do not remove those gates to manufacture an IPK.

## Boundaries and follow-on work

The single UI source is intentionally a small prototype screen router, not a new GUI
framework. Split screens only once device measurements and interaction requirements
justify it. First hardware work: compile with real SDK headers, package audited target
runtime libraries, validate launch identity and remote scancodes, then measure drawable
size/frame pacing. Next rendering work should measure draw-call batching and glyph-atlas
benefits rather than assuming the language itself makes 120 fps possible.

Inline native tuner/HDMI video needs a separate media/compositor backend with verified
ownership/lifecycle. Another app's framebuffer or DRM video must not be assumed available.
