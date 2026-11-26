# Notices

Spice Home C++ is an independent experiment, not affiliated with Sony, LG, Valve,
Microsoft, SDL, FreeType or RetroPie. Names in the interface identify integration targets.
No Sony or LG firmware code, system assets, account credentials or DRM workarounds are
included. Artwork provenance is recorded in assets/PROVENANCE.md.

The original Spice Home project's MIT license is retained. The optional RetroPie
companion is carried forward from that source. The new C++ implementation uses public
APIs and external libraries; no SDL/FreeType/GL/libcurl implementations are embedded.

`compat/native_abi.hpp` is a narrow desktop bootstrap declaration layer written against
SDL2's public event/video/controller types and FreeType's published face/glyph-slot
record prefixes. It is not a complete SDK, has only been exercised on the stated Linux
runtime, and is prohibited for webOS builds. Prefer the actual SDK headers.

Dependency libraries keep their own licenses (SDL zlib license; FreeType FTL/GPL option;
libpng, JSON-C and curl licenses). Runtime DLLs/shared objects are not shipped in this
archive. Preserve upstream notices when constructing your own runtime bundle.
No font files are included or available as part of this release.

Primary implementation references, checked November 4, 2026:

- https://wiki.libsdl.org/SDL2/SDL_GL_GetDrawableSize
- https://wiki.libsdl.org/SDL2/SDL_GL_SetSwapInterval
- https://freetype.org/freetype2/docs/reference/ft2-face_creation.html
- https://freetype.org/freetype2/docs/reference/ft2-glyph_retrieval.html
- https://www.webosbrew.org/pages/native-development
- https://www.webosbrew.org/develop/guides/native-app/
- https://webostv.developer.lge.com/develop/references/application-manager
- https://webostv.developer.lge.com/develop/references/tv-device-information
- https://webostv.developer.lge.com/develop/guides/acg-guide

References inform the adapter contract; they are not proof that a native C3 process
has the corresponding permissions or that any undocumented media API works.
