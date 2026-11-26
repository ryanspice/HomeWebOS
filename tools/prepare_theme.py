#!/usr/bin/env python3
"""Offline, app-owned theme preparation. No downloads or device operations.

Every write is confined to DATA_DIR/themes and generated content-addressed names.
An immutable snapshot is verified before one atomic current.json replacement.
"""
from __future__ import annotations

import argparse
import contextlib
import hashlib
import io
import json
import os
import pathlib
import re
import stat
import struct
import sys
import uuid
import warnings
import zlib

POINTER = "spice-home.theme-pointer.v1"
SCHEMA = "spice-home.theme.v1"
SOURCE = "spice-home.theme-source.v1"
MAX_RGBA = 24 * 1024 * 1024
RESERVE_RGBA = 1600 * 900 * 4  # one additional crossfade texture
MAX_COMPRESSED = 16 * 1024 * 1024
MAX_IMAGE = 8 * 1024 * 1024
MAX_SOURCE_TOTAL = 32 * 1024 * 1024
MAX_IMAGES = 128
APP_ID = re.compile(r"[A-Za-z0-9][A-Za-z0-9._-]{1,127}\Z")
HASH = re.compile(r"[0-9a-f]{64}\Z")
GEN = re.compile(r"[a-f0-9]{32}\Z")
ACCENT = re.compile(r"#[0-9a-fA-F]{6}\Z")
SLOTS = frozenset(("navigation.search", "navigation.home", "navigation.control-center",
                   "actions.refresh", "actions.settings", "library.collection"))


class ThemeError(ValueError):
    pass


def require(ok, message):
    if not ok:
        raise ThemeError(message)


def sha(blob):
    return hashlib.sha256(blob).hexdigest()


def encode(value):
    return (json.dumps(value, ensure_ascii=False, sort_keys=True, indent=2,
                       allow_nan=False) + "\n").encode("utf-8")


def _pairs(pairs):
    result = {}
    for key, value in pairs:
        require("\0" not in key, "NUL in JSON key")
        require(key not in result, "Duplicate JSON key: " + key)
        result[key] = value
    return result


def parse(blob):
    try:
        return json.loads(blob.decode("utf-8"), object_pairs_hook=_pairs,
                          parse_constant=lambda _: (_ for _ in ()).throw(ThemeError("Non-finite JSON number")))
    except (UnicodeDecodeError, json.JSONDecodeError, RecursionError) as error:
        raise ThemeError("Invalid UTF-8 JSON") from error


def fields(value, required, optional=()):
    require(isinstance(value, dict), "Expected JSON object")
    require(set(required) <= value.keys() and value.keys() <= set(required) | set(optional),
            "Missing or unsupported theme fields/layers")


def text(value, limit):
    require(isinstance(value, str) and value and
            all(ord(c) >= 32 and not 127 <= ord(c) <= 159 and not 0xD800 <= ord(c) <= 0xDFFF for c in value) and
            len(value.encode("utf-8")) <= limit,
            "Invalid theme text")
    return value


def no_links(path, *, exists=True):
    """Reject links/reparse points in every existing component, including the root."""
    path = pathlib.Path(os.path.abspath(path))
    for component in reversed((path, *path.parents)):
        try:
            info = component.lstat()
        except FileNotFoundError:
            if exists:
                raise ThemeError("Missing theme path") from None
            continue
        reparse = getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0)
        require(not stat.S_ISLNK(info.st_mode) and not (getattr(info, "st_file_attributes", 0) & reparse),
                "Theme paths cannot contain links or reparse points")
    return path


def file_at(root, relative):
    require(isinstance(relative, str) and relative and "\\" not in relative and
            "\0" not in relative and ":" not in relative, "Invalid relative theme path")
    parts = relative.split("/")
    require(all(p not in ("", ".", "..") for p in parts), "Theme path traversal denied")
    base = no_links(root)
    path = no_links(base.joinpath(*parts))
    info = path.stat()
    require(stat.S_ISREG(info.st_mode) and info.st_nlink == 1, "Theme file must be an unlinked regular file")
    return path


def read(path, limit):
    with path.open("rb") as stream:
        blob = stream.read(limit + 1)
    require(len(blob) <= limit, "Theme file exceeds size limit")
    return blob


def _pillow():
    try:
        from PIL import Image, ImageOps
        return Image, ImageOps
    except ImportError:
        raise ThemeError("Theme preparation and verification require installed Pillow") from None


def normalize(blob, background):
    Image, ImageOps = _pillow()
    require(0 < len(blob) <= MAX_IMAGE, "Source image exceeds size limit")
    try:
        with warnings.catch_warnings():
            warnings.simplefilter("error", Image.DecompressionBombWarning)
            with Image.open(io.BytesIO(blob)) as source:
                require(source.format in ("PNG", "JPEG", "WEBP"), "Unsupported source format; use PNG, JPEG or WebP")
                require(0 < source.width <= 8192 and 0 < source.height <= 8192 and
                        source.width * source.height <= 16 * 1024 * 1024 and
                        getattr(source, "n_frames", 1) == 1, "Source image dimensions/frames exceed limit")
                image = ImageOps.exif_transpose(source).convert("RGBA")
            size = (1600, 900) if background else (128, 128)
            image = ImageOps.contain(image, size, Image.Resampling.LANCZOS)
            position = ((size[0] - image.width) // 2, (size[1] - image.height) // 2)
            canvas = Image.new("RGB" if background else "RGBA", size,
                               (15, 20, 30) if background else (0, 0, 0, 0))
            if background:
                canvas.paste(image, position, image)
            else:
                canvas.alpha_composite(image, position)
            output = io.BytesIO()
            canvas.save(output, format="PNG")
            return output.getvalue()
    except ThemeError:
        raise
    except (OSError, ValueError, SyntaxError, Image.DecompressionBombError, Image.DecompressionBombWarning) as error:
        raise ThemeError("Cannot decode supported source image") from error


def verify_png(blob, background):
    """Admit only our simple RGB/RGBA PNG profile, with CRC and full decode."""
    require(len(blob) >= 57 and blob[:8] == b"\x89PNG\r\n\x1a\n", "Invalid PNG signature")
    pos, kinds, idat_end = 8, [], False
    while pos + 12 <= len(blob):
        size = struct.unpack_from(">I", blob, pos)[0]
        require(size <= MAX_IMAGE and pos + 12 + size <= len(blob), "Invalid PNG chunk size")
        kind = blob[pos + 4:pos + 8]
        body = blob[pos + 8:pos + 8 + size]
        crc = struct.unpack_from(">I", blob, pos + 8 + size)[0]
        require((zlib.crc32(kind + body) & 0xffffffff) == crc, "PNG chunk CRC mismatch")
        require(kind in (b"IHDR", b"IDAT", b"IEND"), "Unsupported PNG chunk")
        if kind == b"IDAT":
            require(not idat_end, "Noncontiguous PNG data")
        elif b"IDAT" in kinds:
            idat_end = True
        kinds.append(kind)
        pos += 12 + size
        if kind == b"IEND":
            require(size == 0 and pos == len(blob), "Invalid PNG end")
            break
    require(kinds and kinds[0] == b"IHDR" and kinds.count(b"IHDR") == 1 and
            b"IDAT" in kinds and kinds[-1] == b"IEND" and kinds.count(b"IEND") == 1 and
            struct.unpack_from(">I", blob, 8)[0] == 13, "Invalid PNG chunks")
    width, height = struct.unpack_from(">II", blob, 16)
    expected = (1600, 900, 8, 2) if background else (128, 128, 8, 6)
    require((width, height, blob[24], blob[25]) == expected and blob[26:29] == b"\0\0\0",
            "PNG dimensions or RGB/RGBA profile mismatch")
    Image, _ = _pillow()
    try:
        with Image.open(io.BytesIO(blob)) as image:
            image.load()
            require(image.size == (width, height) and image.mode == ("RGB" if background else "RGBA"),
                    "PNG decode mismatch")
    except ThemeError:
        raise
    except (OSError, ValueError, SyntaxError) as error:
        raise ThemeError("PNG decode failed") from error
    return width, height


def _reference(ref):
    if ref is None:
        return
    fields(ref, ("manifest", "sha256"))
    path = ref["manifest"]
    require(isinstance(path, str) and re.fullmatch(r"snapshots/[a-f0-9]{32}/manifest\.json", path) and
            isinstance(ref["sha256"], str) and HASH.fullmatch(ref["sha256"]), "Invalid theme pointer reference")


def current_bytes(root):
    root = pathlib.Path(root)
    if root.exists() or root.is_symlink():
        no_links(root)
    path = root / "current.json"
    if not path.exists() and not path.is_symlink():
        return None
    return read(file_at(root, "current.json"), 4096)


def pointer(root):
    raw = current_bytes(root)
    if raw is None:
        return None, None
    data = parse(raw)
    fields(data, ("schema", "selected", "previous"))
    require(data["schema"] == POINTER, "Unsupported theme pointer")
    _reference(data["selected"])
    _reference(data["previous"])
    return data, raw


def verify_ref(root, ref):
    _reference(ref)
    require(ref is not None, "Built-in default has no theme generation")
    manifest_file = file_at(root, ref["manifest"])
    raw = read(manifest_file, 131072)
    require(sha(raw) == ref["sha256"], "Theme manifest hash mismatch")
    data = parse(raw)
    fields(data, ("schema", "name", "version", "provenance", "background_policy", "background",
                  "icons", "apps", "images", "rgba_bytes", "compressed_bytes"), ("accent",))
    require(data["schema"] == SCHEMA, "Unsupported theme schema or p5theme layers")
    for key, limit in (("name", 96), ("version", 64), ("provenance", 384)):
        text(data[key], limit)
    require(data["background_policy"] in ("all", "missing"), "Invalid background policy")
    require("accent" not in data or isinstance(data["accent"], str) and ACCENT.fullmatch(data["accent"]), "Invalid accent")
    require(isinstance(data["images"], list) and 1 <= len(data["images"]) <= MAX_IMAGES, "Invalid image count")
    images, rgba, compressed = {}, 0, 0
    for item in data["images"]:
        fields(item, ("sha256", "path", "bytes", "width", "height", "kind"))
        digest = item["sha256"]
        require(isinstance(digest, str) and HASH.fullmatch(digest) and digest not in images and
                item["path"] == "images/" + digest + ".png" and item["kind"] in ("background", "icon"),
                "Invalid or duplicate content-addressed theme image")
        blob = read(file_at(manifest_file.parent, item["path"]), MAX_IMAGE)
        require(type(item["bytes"]) is int and item["bytes"] == len(blob) and sha(blob) == digest,
                "Theme image hash/length mismatch")
        w, h = verify_png(blob, item["kind"] == "background")
        require(type(item["width"]) is int and type(item["height"]) is int and
                (item["width"], item["height"]) == (w, h), "Theme image dimensions mismatch")
        rgba += w * h * 4
        compressed += len(blob)
        require(rgba + RESERVE_RGBA <= MAX_RGBA and compressed <= MAX_COMPRESSED, "Theme graphics budget exceeded")
        images[digest] = (item["kind"], blob)
    require(type(data["rgba_bytes"]) is int and data["rgba_bytes"] == rgba + RESERVE_RGBA and
            type(data["compressed_bytes"]) is int and data["compressed_bytes"] == compressed, "Theme budget accounting mismatch")
    used = set()

    def bind(digest, kind):
        require(isinstance(digest, str) and digest in images and images[digest][0] == kind, "Unbound theme image")
        used.add(digest)

    bind(data["background"], "background")
    require(isinstance(data["icons"], list) and len(data["icons"]) <= len(SLOTS), "Invalid semantic icons")
    seen = set()
    for entry in data["icons"]:
        fields(entry, ("slot", "image"))
        require(isinstance(entry["slot"], str) and entry["slot"] in SLOTS and entry["slot"] not in seen, "Invalid or duplicate semantic slot")
        seen.add(entry["slot"])
        bind(entry["image"], "icon")
    require(isinstance(data["apps"], list) and len(data["apps"]) <= 64, "Invalid app overrides")
    seen.clear()
    for entry in data["apps"]:
        fields(entry, ("id",), ("icon", "hero"))
        app = entry["id"]
        require(isinstance(app, str) and APP_ID.fullmatch(app) and app not in seen and
                ("icon" in entry or "hero" in entry), "Invalid or duplicate app override")
        seen.add(app)
        if "icon" in entry:
            bind(entry["icon"], "icon")
        if "hero" in entry:
            bind(entry["hero"], "background")
    require(used == set(images), "Unreferenced theme image")
    return data, images


def verify(root):
    data, _ = pointer(root)
    if data is None or data["selected"] is None:
        return None
    return verify_ref(root, data["selected"])[0]


@contextlib.contextmanager
def lock(root):
    root = no_links(root, exists=False)
    root.mkdir(parents=True, exist_ok=True)
    no_links(root)
    path = root / ".theme-lock"
    try:
        fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    except FileExistsError:
        raise ThemeError("Theme store is locked; do not remove an active lock") from None
    token = uuid.uuid4().hex.encode()
    try:
        with os.fdopen(fd, "wb") as stream:
            stream.write(token)
            stream.flush()
            os.fsync(stream.fileno())
        yield root
    finally:
        try:
            if read(file_at(root, ".theme-lock"), 128) == token:
                path.unlink()
        except (OSError, ThemeError):
            pass


def write_new(path, blob):
    with path.open("xb") as stream:
        stream.write(blob)
        stream.flush()
        os.fsync(stream.fileno())


def publish(root, before, selected, previous):
    _reference(selected)
    _reference(previous)
    require(current_bytes(root) == before, "Current theme changed during preparation")
    raw = encode(dict(schema=POINTER, selected=selected, previous=previous))
    tmp = root / (".current-" + uuid.uuid4().hex + ".tmp")
    write_new(tmp, raw)
    # A failed replace leaves the previous pointer and the uniquely named temporary file intact.
    require(current_bytes(root) == before, "Current theme changed before publication")
    os.replace(tmp, root / "current.json")


def import_theme(root, source_path):
    require(pathlib.Path(source_path).suffix.lower() == ".json", "Unsupported p5theme/archive input; use a local theme source JSON")
    source_path = no_links(source_path)
    blob = read(file_at(source_path.parent, source_path.name), 131072)
    source = parse(blob)
    fields(source, ("schema", "name", "version", "provenance", "background", "icons", "apps"),
           ("accent", "background_policy"))
    require(source["schema"] == SOURCE, "Unsupported source schema/p5theme layers")
    for key, limit in (("name", 96), ("version", 64), ("provenance", 384)):
        text(source[key], limit)
    policy = source.get("background_policy", "missing")
    require(policy in ("all", "missing"), "Invalid background policy")
    require("accent" not in source or isinstance(source["accent"], str) and ACCENT.fullmatch(source["accent"]), "Invalid accent")
    require(isinstance(source["icons"], list) and len(source["icons"]) <= len(SLOTS) and
            isinstance(source["apps"], list) and len(source["apps"]) <= 64, "Theme mapping count exceeds limit")
    blobs, descriptions = {}, {}
    input_bytes = 0

    def convert(path, background):
        nonlocal input_bytes
        original = read(file_at(source_path.parent, path), MAX_IMAGE)
        input_bytes += len(original)
        require(input_bytes <= MAX_SOURCE_TOTAL, "Source image total exceeds limit")
        png = normalize(original, background)
        require(len(png) <= MAX_IMAGE, "Normalized image exceeds limit")
        digest = sha(png)
        kind = "background" if background else "icon"
        if digest not in blobs:
            blobs[digest] = png
            descriptions[digest] = dict(sha256=digest, path="images/" + digest + ".png", bytes=len(png),
                                        width=1600 if background else 128, height=900 if background else 128, kind=kind)
        return digest

    background = convert(source["background"], True)
    icons, apps, seen = [], [], set()
    for item in source["icons"]:
        fields(item, ("slot", "file"))
        slot = item["slot"]
        require(isinstance(slot, str) and slot in SLOTS and slot not in seen, "Invalid or duplicate semantic slot")
        seen.add(slot)
        icons.append(dict(slot=slot, image=convert(item["file"], False)))
    seen.clear()
    for item in source["apps"]:
        fields(item, ("id",), ("icon", "hero"))
        app = item["id"]
        require(isinstance(app, str) and APP_ID.fullmatch(app) and app not in seen and
                ("icon" in item or "hero" in item), "Invalid or duplicate app override")
        seen.add(app)
        result = dict(id=app)
        for key in ("icon", "hero"):
            if key in item:
                result[key] = convert(item[key], key == "hero")
        apps.append(result)
    require(len(blobs) <= MAX_IMAGES, "Too many theme images")
    rgba = RESERVE_RGBA + sum(d["width"] * d["height"] * 4 for d in descriptions.values())
    compressed = sum(len(b) for b in blobs.values())
    require(rgba <= MAX_RGBA and compressed <= MAX_COMPRESSED, "Theme graphics budget exceeded")
    manifest = dict(schema=SCHEMA, name=source["name"], version=source["version"], provenance=source["provenance"],
                    background_policy=policy, background=background, icons=icons, apps=apps,
                    images=[descriptions[d] for d in sorted(descriptions)], rgba_bytes=rgba, compressed_bytes=compressed)
    if "accent" in source:
        manifest["accent"] = source["accent"]
    with lock(root) as root:
        before, raw = pointer(root)
        if before and before["selected"]:
            verify_ref(root, before["selected"])
        snapshots = root / "snapshots"
        snapshots.mkdir(exist_ok=True)
        no_links(snapshots)
        generation = uuid.uuid4().hex
        folder = snapshots / generation
        folder.mkdir()
        (folder / "images").mkdir()
        for digest, data in blobs.items():
            write_new(folder / "images" / (digest + ".png"), data)
        manifest_bytes = encode(manifest)
        write_new(folder / "manifest.json", manifest_bytes)
        ref = dict(manifest="snapshots/" + generation + "/manifest.json", sha256=sha(manifest_bytes))
        verify_ref(root, ref)
        publish(root, raw, ref, before["selected"] if before else None)
    return manifest


def select_default(root):
    with lock(root) as root:
        raw = current_bytes(root)
        try:
            before, _ = pointer(root)
        except ThemeError:
            # Preserve malformed pointer bytes before recovery; never follow links or
            # overwrite a recovery file. The raw preimage is checked again on publish.
            require(raw is not None, "Theme pointer changed during recovery")
            recovery = root / "recovery"
            recovery.mkdir(exist_ok=True)
            no_links(recovery)
            name = "invalid-pointer-" + sha(raw) + ".json"
            backup = recovery / name
            if backup.exists() or backup.is_symlink():
                require(read(file_at(recovery, name), 4096) == raw, "Recovery preimage mismatch")
            else:
                write_new(backup, raw)
            publish(root, raw, None, None)
            return
        if before and before["selected"] is not None:
            publish(root, raw, None, before["selected"])
        elif before is None:
            publish(root, raw, None, None)


def select_previous(root):
    with lock(root) as root:
        before, raw = pointer(root)
        require(before is not None, "No previous theme is recorded")
        # A null reference is the built-in default; a selected default with a null
        # previous has no different state to restore.
        require(before["selected"] is not None or before["previous"] is not None, "No previous theme is recorded")
        if before["previous"] is not None:
            verify_ref(root, before["previous"])
        publish(root, raw, before["previous"], before["selected"])


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--data-dir", type=pathlib.Path, required=True,
                        help="app-owned data directory; writes only beneath its themes/ child")
    sub = parser.add_subparsers(dest="command", required=True)
    imp = sub.add_parser("import", help="normalize an explicit local source JSON")
    imp.add_argument("source", type=pathlib.Path)
    sub.add_parser("verify")
    sub.add_parser("default")
    sub.add_parser("previous")
    args = parser.parse_args(argv)
    root = args.data_dir / "themes"
    try:
        if args.command == "import":
            result = import_theme(root, args.source)
            print("Prepared and selected: " + result["name"])
        elif args.command == "verify":
            result = verify(root)
            print("Verified: " + (result["name"] if result else "built-in default"))
        elif args.command == "default":
            select_default(root)
            print("Built-in default selected; previous retained")
        else:
            select_previous(root)
            print("Previous selection restored")
        return 0
    except (ThemeError, OSError) as error:
        print("Theme error: " + str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
