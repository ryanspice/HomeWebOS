#!/usr/bin/env python3
"""Prepare immutable local TV metadata/artwork snapshots; never install or launch apps.

Only collect opens a device connection. Import/verify/stage are fully offline.
Pillow is required for image preparation, not verification, staging or the launcher.
"""
from __future__ import annotations
import argparse
import base64
import contextlib
import copy
import datetime as dt
import hashlib
import io
import json
import os
import pathlib
import re
import shutil
import struct
import subprocess
import sys
import uuid
import warnings

POINTER_SCHEMA = "spice-home.prepared-pointer.v1"
BUNDLE_SCHEMA = "spice-home.prepared.v1"
MAX_APPS, MAX_ASSETS = 200, 128
MAX_COMPRESSED, MAX_RGBA = 16 * 1024 * 1024, 48 * 1024 * 1024
APP_ROOTS = ("/usr/palm/applications/", "/media/cryptofs/apps/usr/palm/applications/", "/media/developer/apps/usr/palm/applications/")
APP_ID = re.compile(r"[A-Za-z0-9][A-Za-z0-9._-]{1,127}\Z")
SHA = re.compile(r"[0-9a-f]{64}\Z")

class PreparationError(ValueError):
    pass

def require(condition, message):
    if not condition:
        raise PreparationError(message)

def sha(data):
    return hashlib.sha256(data).hexdigest()

def encoded(value):
    return (json.dumps(value, ensure_ascii=False, sort_keys=True, indent=2, allow_nan=False) + "\n").encode("utf-8")

def text(value, limit=256):
    if not isinstance(value, str):
        return ""
    value = re.sub(r"<[^>]{0,256}>", "", value)
    value = "".join(c for c in value if ord(c) >= 32 and not 127 <= ord(c) <= 159)
    return value.encode("utf-8", errors="replace")[:limit].decode("utf-8", errors="ignore")

def read_bounded(path, limit):
    with pathlib.Path(path).open("rb") as stream:
        value = stream.read(limit + 1)
    require(len(value) <= limit, "Local file exceeds preparation limit")
    return value

def below(root, relative):
    require(isinstance(relative, str) and relative and "\\" not in relative and "\0" not in relative, "Invalid bundle path")
    part = pathlib.PurePosixPath(relative)
    require(not part.is_absolute() and all(p not in (".", "..") for p in part.parts) and ":" not in relative, "Bundle path escapes root")
    root = pathlib.Path(root).resolve(strict=True)
    result = (root / relative).resolve(strict=True)
    require(result.is_file() and result.is_relative_to(root), "Bundle path escapes root or is not a regular file")
    return result

def png_dimensions(data):
    require(len(data) >= 33 and data[:8] == b"\x89PNG\r\n\x1a\n" and data[12:16] == b"IHDR", "Prepared art must be PNG")
    w, h = struct.unpack(">II", data[16:24])
    require(data[24] == 8 and data[25] in (2, 6), "Prepared PNG must be 8-bit RGB/RGBA")
    require(0 < w <= 1920 and 0 < h <= 1080, "Prepared PNG dimensions exceed limit")
    return w, h

def verify_generation(folder, manifest_bytes):
    manifest = json.loads(manifest_bytes)
    require(isinstance(manifest, dict) and manifest.get("schema") == BUNDLE_SCHEMA, "Unsupported prepared schema")
    data_bytes = read_bounded(below(folder, "data.json"), 1024 * 1024)
    require(sha(data_bytes) == manifest.get("data_sha256"), "Metadata hash mismatch")
    data = json.loads(data_bytes)
    require(isinstance(data, dict) and data.get("schema") == BUNDLE_SCHEMA and isinstance(data.get("apps"), list), "Invalid prepared metadata")
    require(isinstance(data.get("source"), dict) and len(data["apps"]) <= MAX_APPS and data["source"].get("kind") in ("tv", "import"), "Metadata scope exceeds limit")
    require(bool(text(data.get("captured_at"), 40)), "Missing capture time")
    ids = set()
    keys = set()
    assets = {}
    total, rgba = 0, 0
    require(isinstance(manifest.get("assets"), list), "Prepared assets must be an array")
    for asset in manifest["assets"]:
        require(isinstance(asset, dict), "Prepared asset must be an object")
        require(len(keys) < MAX_ASSETS, "Too many prepared images")
        key, digest = asset.get("key", ""), asset.get("sha256", "")
        require(re.fullmatch(r"(?:covers|hero)/prepared/[A-Za-z0-9][A-Za-z0-9._-]{1,127}", key) and key not in keys, "Invalid/duplicate image key")
        require(SHA.fullmatch(digest) and asset.get("path") == "images/" + digest + ".png", "Invalid content-addressed image path")
        blob = read_bounded(below(folder, asset["path"]), 8 * 1024 * 1024)
        require(sha(blob) == digest and len(blob) == asset.get("bytes"), "Image hash/length mismatch")
        w, h = png_dimensions(blob)
        require((w, h) == (asset.get("width"), asset.get("height")), "Image header/manifest mismatch")
        require(not key.startswith("covers/") or (w <= 256 and h <= 256), "Icon exceeds prepared dimensions")
        total += len(blob)
        rgba += w * h * 4
        require(total <= MAX_COMPRESSED and rgba <= MAX_RGBA, "Prepared graphics budget exceeded")
        keys.add(key)
        assets[key] = blob
    for app in data["apps"]:
        require(isinstance(app, dict), "Prepared app must be an object")
        app_id = app.get("id", "")
        require(isinstance(app_id, str) and APP_ID.fullmatch(app_id) and app_id not in ids, "Invalid/duplicate app ID")
        ids.add(app_id)
        for field, limit in (("title", 96), ("description", 384), ("vendor", 96), ("version", 64), ("observed_at", 40), ("provenance", 80)):
            require(isinstance(app.get(field, ""), str) and text(app.get(field, ""), limit) == app.get(field, ""), "Invalid prepared text")
        for field, prefix in (("icon_key", "covers/"), ("hero_key", "hero/")):
            key = app.get(field, "")
            require(not key or key == prefix + "prepared/" + app_id and key in keys, "Unbound app image")
        require(isinstance(app.get("inventory_confirmed"), bool), "Missing inventory evidence state")
        require(data["source"]["kind"] == "tv" or not app["inventory_confirmed"], "Imported metadata cannot claim live TV inventory")
    require(isinstance(data.get("graphics"), dict) and data["graphics"].get("rgba_bytes") == rgba and data["graphics"].get("compressed_bytes") == total and data["graphics"].get("assets") == len(assets), "Prepared graphics accounting mismatch")
    return data, assets, manifest

def verify_bundle(root):
    root = pathlib.Path(root)
    pointer_bytes = read_bounded(below(root, "current.json"), 4096)
    pointer = json.loads(pointer_bytes)
    require(isinstance(pointer, dict) and pointer.get("schema") == POINTER_SCHEMA and isinstance(pointer.get("manifest"), str) and re.fullmatch(r"snapshots/[A-Za-z0-9_-]{1,80}/manifest\.json", pointer["manifest"]), "Invalid snapshot pointer")
    manifest_file = below(root, pointer["manifest"])
    manifest_bytes = read_bounded(manifest_file, 128 * 1024)
    require(sha(manifest_bytes) == pointer.get("sha256"), "Manifest hash mismatch")
    data, assets, manifest = verify_generation(manifest_file.parent, manifest_bytes)
    return data, assets, manifest, pointer

@contextlib.contextmanager
def bundle_lock(root):
    root = pathlib.Path(root)
    require(not root.is_symlink(), "Bundle root must not be a symbolic link")
    root.mkdir(parents=True, exist_ok=True)
    if any(root.iterdir()) and not (root / "current.json").exists():
        # An earlier failed new generation is retained; require a fresh output root.
        raise PreparationError("Nonempty output has no committed bundle; choose a new output root")
    lock = root / ".prepare.lock"
    try:
        fd = os.open(lock, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    except FileExistsError:
        raise PreparationError("Preparation lock exists; another preparation may own this bundle") from None
    token = str(uuid.uuid4()).encode()
    with os.fdopen(fd, "wb") as stream:
        stream.write(token)
    try:
        yield
    finally:
        if lock.exists() and lock.read_bytes() == token:
            lock.unlink()

def commit(root, data, assets, prior_pointer, protected_keys=()):
    root = pathlib.Path(root)
    current = root / "current.json"
    require((current.read_bytes() if current.exists() else None) == prior_pointer, "Current snapshot changed during preparation")
    generation = dt.datetime.now(dt.timezone.utc).strftime("%Y%m%dT%H%M%SZ_") + uuid.uuid4().hex[:12]
    folder = root / "snapshots" / generation
    folder.mkdir(parents=True, exist_ok=False)
    (folder / "images").mkdir()
    admitted, total, rgba, descriptors = {}, 0, 0, []
    # Prioritize icons, then backgrounds; never defer missing graphics to live APIs.
    for key in sorted(assets, key=lambda value: (value not in protected_keys, not value.startswith("covers/"), value)):
        blob = assets[key]
        w, h = png_dimensions(blob)
        if len(admitted) >= MAX_ASSETS or total + len(blob) > MAX_COMPRESSED or rgba + w * h * 4 > MAX_RGBA:
            continue
        digest = sha(blob)
        relative = "images/" + digest + ".png"
        destination = folder / relative
        if not destination.exists():
            destination.write_bytes(blob)
        descriptors.append(dict(key=key, path=relative, sha256=digest, bytes=len(blob), width=w, height=h))
        admitted[key] = blob
        total += len(blob)
        rgba += w * h * 4
    for app in data["apps"]:
        for field in ("icon_key", "hero_key"):
            if app.get(field) not in admitted:
                app[field] = ""
    data["graphics"] = dict(compressed_bytes=total, rgba_bytes=rgba, assets=len(admitted), omitted_assets=len(assets) - len(admitted), loading="startup-only-no-live-fallback")
    payload = encoded(data)
    (folder / "data.json").write_bytes(payload)
    manifest = encoded(dict(schema=BUNDLE_SCHEMA, data_sha256=sha(payload), assets=descriptors))
    (folder / "manifest.json").write_bytes(manifest)
    verify_generation(folder, manifest)
    pointer = encoded(dict(schema=POINTER_SCHEMA, manifest="snapshots/" + generation + "/manifest.json", sha256=sha(manifest)))
    require((current.read_bytes() if current.exists() else None) == prior_pointer, "Current snapshot changed before publication")
    temporary = root / (".current-" + uuid.uuid4().hex + ".tmp")
    with temporary.open("xb") as stream:
        stream.write(pointer)
        stream.flush()
        os.fsync(stream.fileno())
    os.replace(temporary, current)
    return verify_bundle(root)

def prepare_image(blob, hero=False):
    try:
        from PIL import Image, ImageOps
    except ImportError:
        raise PreparationError("Image preparation requires an existing Pillow installation") from None
    require(0 < len(blob) <= 8 * 1024 * 1024, "Source image exceeds preparation limit")
    with warnings.catch_warnings():
        warnings.simplefilter("error", Image.DecompressionBombWarning)
        Image.MAX_IMAGE_PIXELS = 16 * 1024 * 1024
        with Image.open(io.BytesIO(blob)) as source:
            require(source.format in ("PNG", "JPEG", "WEBP") and source.width * source.height <= 16 * 1024 * 1024, "Source image format/size denied")
            im = source.convert("RGBA")
        size = (1600, 900) if hero else (256, 256)
        im = ImageOps.contain(im, size, Image.Resampling.LANCZOS)
        canvas = Image.new("RGB" if hero else "RGBA", size, (15, 20, 30) if hero else (0, 0, 0, 0))
        position = ((size[0] - im.width) // 2, (size[1] - im.height) // 2)
        if hero:
            canvas.paste(im, position, im)
        else:
            canvas.alpha_composite(im, position)
        output = io.BytesIO()
        canvas.save(output, format="PNG", optimize=False)
        return output.getvalue()

class TvTransport:
    def __init__(self, device, private_reads=False, cli_root=None, luna_client="public"):
        node = shutil.which("node")
        require(node is not None, "Existing Node/webOS CLI installation is required")
        command = [node, str(pathlib.Path(__file__).parent / "agent/tv_data_transport.cjs"), "--device", device, "--luna-client", luna_client]
        if private_reads:
            command.append("--private-reads")
        if cli_root:
            command.extend(["--cli-root", cli_root])
        self.process = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                                        text=True, encoding="utf-8", creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        self.calls = []
        self.luna_client = luna_client
        ready = self._read()
        if not ready.get("ready"):
            self.close()
            raise PreparationError("TV transport unavailable: " + ready.get("fatal", "no-response"))
        self.config_sha = ready["device_configuration_sha256"]

    def _read(self):
        line = self.process.stdout.readline(12 * 1024 * 1024)
        require(line.endswith("\n"), "TV transport closed or exceeded output limit")
        return json.loads(line)

    def request(self, operation, **fields):
        request_id = "req-" + uuid.uuid4().hex
        self.process.stdin.write(json.dumps(dict(request_id=request_id, op=operation, **fields)) + "\n")
        self.process.stdin.flush()
        response = self._read()
        require(response.get("request_id") == request_id, "TV transport response identity mismatch")
        self.calls.append(dict(operation=operation, status="ok" if response.get("ok") else response.get("code", "unavailable")))
        return response.get("data") if response.get("ok") else None

    def read(self, path):
        response = self.request("file", path=path)
        return base64.b64decode(response["base64"], validate=True) if response else None

    def close(self):
        if self.process.poll() is None:
            try:
                self.process.stdin.write('{"op":"close"}\n')
                self.process.stdin.flush()
                response = self._read()
                require(response.get("configuration_unchanged") is True, "Shared device configuration changed during collection")
            finally:
                self.process.stdin.close()
                try:
                    self.process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    self.process.kill()
                    self.process.wait(timeout=3)

def app_metadata(row, now, confirmed, provenance):
    app_id = row.get("id", "")
    require(isinstance(app_id, str) and APP_ID.fullmatch(app_id), "Invalid app ID in source metadata")
    return dict(id=app_id, title=text(row.get("title", app_id), 96) or app_id, description=text(row.get("appDescription", row.get("description", "")), 384),
                vendor=text(row.get("vendor", ""), 96), version=text(row.get("version", ""), 64), observed_at=now,
                inventory_confirmed=bool(confirmed), provenance=provenance, icon_key="", hero_key="")

def local_art(base, value):
    if not isinstance(value, str) or not value:
        return None
    require(not re.match(r"^[a-zA-Z]+:", value), "Remote artwork is not fetched; supply an explicit local marketing export")
    return read_bounded(below(base, value), 8 * 1024 * 1024)

def add_art(app, row, reader, assets, unavailable):
    choices = (("icon_key", "covers/", row.get("largeIcon") or row.get("icon") or "icon.png", False),
               ("hero_key", "hero/", row.get("bgImage") or row.get("imageForRecents") or row.get("splashBackground"), True))
    for field, prefix, value, hero in choices:
        if not value:
            continue
        try:
            blob = reader(value)
            if blob:
                key = prefix + "prepared/" + app["id"]
                assets[key] = prepare_image(blob, hero)
                app[field] = key
            else:
                unavailable.append(dict(app_id=app["id"], field=field, status="not-readable"))
        except (ValueError, OSError) as error:
            unavailable.append(dict(app_id=app["id"], field=field, status="remote-url-not-fetched" if isinstance(value, str) and "://" in value else "invalid-or-unavailable-image"))

def import_data(source):
    source = pathlib.Path(source)
    raw = json.loads(read_bounded(source, 1024 * 1024))
    require(isinstance(raw.get("apps"), list) and len(raw["apps"]) <= MAX_APPS, "Import must contain at most 200 apps")
    now = dt.datetime.now(dt.timezone.utc).isoformat(timespec="seconds")
    apps, assets, unavailable = [], {}, []
    for row in raw["apps"]:
        app = app_metadata(row, now, False, "owner-import")
        add_art(app, row, lambda value: local_art(source.parent, value), assets, unavailable)
        apps.append(app)
    return dict(schema=BUNDLE_SCHEMA, captured_at=now, source=dict(kind="import", label="Owner-provided offline metadata"),
                device={}, launcher={}, apps=apps, unavailable=unavailable, refresh_policy="explicit-only"), assets

def remote_art_path(base, value):
    require(isinstance(value, str) and value and len(value) <= 1024, "Invalid artwork reference")
    if value.startswith("file://"):
        value = value[7:]
    require("://" not in value and "\0" not in value and "\\" not in value, "Remote artwork URL not fetched")
    part = pathlib.PurePosixPath(value)
    require(".." not in part.parts, "Artwork path traversal denied")
    return str(part if part.is_absolute() else pathlib.PurePosixPath(base) / part)

def collect_data(transport, device, existing=None, app_id=None, marketing=None):
    now = dt.datetime.now(dt.timezone.utc).isoformat(timespec="seconds")
    assets = dict(existing[1]) if existing else {}
    data = copy.deepcopy(existing[0]) if existing else dict(schema=BUNDLE_SCHEMA, device={}, launcher={}, apps=[])
    rows, confirmed, folders, unavailable, hidden = {}, set(), {}, [], []
    if app_id:
        require(APP_ID.fullmatch(app_id), "Invalid refresh app ID")
        require(existing is not None and existing[0]["source"]["kind"] == "tv", "Single-app refresh requires an existing TV snapshot")
        data.setdefault("device_captured_at", data["captured_at"])
        info = transport.request("appinfo", app_id=app_id) or {}
        status = transport.request("status", app_id=app_id) or {}
        row = info.get("appInfo", {})
        if isinstance(row, dict) and row.get("id") == app_id:
            rows[app_id] = row
        if status.get("exist") is True:
            confirmed.add(app_id)
    else:
        info = transport.request("device") or {}
        data["device_captured_at"] = now
        data["device"] = {key: text(str(info[key]), 96) for key in ("modelName", "firmwareVersion", "sdkVersion", "UHD") if key in info and isinstance(info[key], (str, bool, int, float))}
        launch = transport.request("launchpoints") or {}
        inventory = transport.request("apps") or {}
        data["source_availability"] = dict(tv_information=bool(data["device"]),
            launcher_inventory=isinstance(launch.get("launchPoints"), list),
            app_inventory=isinstance(inventory.get("apps"), list),
            inventory_scope=("private-full" if getattr(transport, "luna_client", "public") == "private" else "developer") if isinstance(inventory.get("apps"), list) else "package-files-only",
            inventory_records=len(inventory.get("apps", [])))
        for row in list(launch.get("launchPoints", []))[:400] + list(inventory.get("apps", []))[:400]:
            if not isinstance(row, dict):
                continue
            key = row.get("id", "")
            if not isinstance(key, str) or not APP_ID.fullmatch(key):
                continue
            confirmed.add(key)
            rows[key] = {**row, **rows.get(key, {})}
        found = transport.request("appdirs") or {}
        for filename in found.get("paths", []):
            key = pathlib.PurePosixPath(filename).parent.name
            if APP_ID.fullmatch(key):
                folders.setdefault(key, []).append(filename)
        for key in folders:
            rows.setdefault(key, dict(id=key))
    targets = [app_id] if app_id else list(rows)[:MAX_APPS]
    fresh = []
    for key in targets:
        row = dict(rows.get(key, dict(id=key)))
        filenames = folders.get(key, []) + [root + key + "/appinfo.json" for root in APP_ROOTS]
        folder, found = "", False
        for filename in dict.fromkeys(filenames):
            blob = transport.read(filename)
            if not blob:
                continue
            try:
                packaged = json.loads(blob)
            except (ValueError, UnicodeDecodeError):
                continue
            if not isinstance(packaged, dict) or packaged.get("id") != key:
                continue
            row = {**row, **packaged}
            folder, found = str(pathlib.PurePosixPath(filename).parent), True
            break
        if "home" in key and found:
            data["launcher"][key] = dict(id=key, title=text(row.get("title", key), 96), version=text(row.get("version", ""), 64), observed_at=now, provenance="tv-packaged-appinfo")
        if row.get("visible") is False:
            hidden.append(app_metadata(row, now, key in confirmed, "tv-launcher-inventory" if key in confirmed else "tv-packaged-appinfo"))
            continue
        if key == "com.spice.home.cpp":
            continue
        if not found and key not in confirmed:
            if app_id:
                raise PreparationError("Requested app was not observed; previous snapshot retained")
            continue
        if app_id:
            for old_key in ("covers/prepared/" + key, "hero/prepared/" + key):
                assets.pop(old_key, None)
        app = app_metadata(row, now, key in confirmed, "tv-launcher-inventory" if key in confirmed else "tv-packaged-appinfo")
        if folder:
            add_art(app, row, lambda value: transport.read(remote_art_path(folder, value)), assets, unavailable)
        fresh.append(app)
    if app_id:
        require(bool(fresh), "Requested app has no usable metadata; previous snapshot retained")
        old = data["apps"]
        data["apps"] = [fresh[0] if app["id"] == app_id else app for app in old]
        if not any(app["id"] == app_id for app in old):
            data["apps"].append(fresh[0])
    else:
        data["apps"] = fresh
        data["non_launchable_apps"] = hidden
    data.setdefault("source_availability", {})["lg_home_package"] = any(key in data.get("launcher", {}) for key in ("com.webos.app.home", "com.webos.app.seniorhome"))
    require(bool(data["apps"]), "No readable apps; previous snapshot retained")
    require(len(data["apps"]) <= MAX_APPS, "App count exceeds prepared limit")
    data.update(captured_at=now, source=dict(kind="tv", label="Prepared TV snapshot", device=device),
                unavailable=unavailable, refresh_policy="explicit-only", last_refresh=dict(scope="single-app" if app_id else "full", app_id=app_id))
    if marketing:
        overlay = json.loads(read_bounded(marketing, 1024 * 1024))
        by_id = {app["id"]: app for app in data["apps"]}
        for row in overlay.get("apps", [])[:MAX_APPS]:
            if row.get("id") in by_id and (not app_id or row["id"] == app_id):
                app = by_id[row["id"]]
                if row.get("appDescription"):
                    app["description"] = text(row["appDescription"], 384)
                add_art(app, row, lambda value: local_art(pathlib.Path(marketing).parent, value), assets, unavailable)
                app["marketing_provenance"] = "owner-provided-local-export"
    used = {app[field] for app in data["apps"] for field in ("icon_key", "hero_key") if app.get(field)}
    return data, {key: value for key, value in assets.items() if key in used}

def stage_bundle(root, output):
    data, assets, manifest, pointer = verify_bundle(root)
    output = pathlib.Path(output)
    require(not output.exists(), "Preinstall destination exists; choose a new stage")
    folder = output / pathlib.PurePosixPath(pointer["manifest"]).parent
    folder.mkdir(parents=True, exist_ok=False)
    (folder / "images").mkdir()
    source = below(root, pointer["manifest"]).parent
    for name in ("data.json", "manifest.json"):
        (folder / name).write_bytes(read_bounded(below(source, name), 1024 * 1024))
    for asset in manifest["assets"]:
        (folder / asset["path"]).write_bytes(assets[asset["key"]])
    (output / "current.json").write_bytes(encoded(pointer))
    return verify_bundle(output)

def summary(result):
    data, assets, _, pointer = result
    return dict(status="verified", source=data["source"], captured_at=data["captured_at"], apps=len(data["apps"]),
                non_launchable_apps=len(data.get("non_launchable_apps", [])), source_availability=data.get("source_availability", {}),
                graphics=len(assets), rgba_bytes=data["graphics"]["rgba_bytes"], omitted_assets=data["graphics"]["omitted_assets"],
                manifest=pointer["manifest"], automatic_live_refresh=False, tv_install_performed=False)

def inspect_connection(args):
    transport = TvTransport(args.device, args.private_reads, args.cli_root, args.luna_client)
    try:
        device = transport.request("device") or {}
        inventory = transport.request("apps") or {}
        launch = transport.request("launchpoints") or {}
        lg_home = any(transport.read(root + "com.webos.app.home/appinfo.json") for root in APP_ROOTS)
        result = dict(status="inspected", device=args.device, luna_client=args.luna_client,
                      method="Existing webOS CLI device / SSH; same configured route as ares-novacom",
                      model_name=text(device.get("modelName", ""), 96), firmware_version=text(device.get("firmwareVersion", ""), 96),
                      inventory_scope="developer" if args.luna_client == "public" else "private-full",
                      inventory_available=isinstance(inventory.get("apps"), list), inventory_records=len(inventory.get("apps", [])),
                      lg_launcher_inventory_available=isinstance(launch.get("launchPoints"), list), lg_home_package_available=lg_home,
                      request_status=transport.calls, remote_writes=False, configuration_sha256=transport.config_sha)
    finally:
        transport.close()
    result["configuration_unchanged"] = True
    return result

def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    for name in ("collect", "import", "verify", "stage", "inspect"):
        sub = commands.add_parser(name)
        if name != "inspect":
            sub.add_argument("--bundle", type=pathlib.Path, required=True)
        if name in ("collect", "inspect"):
            sub.add_argument("--device", required=True)
            sub.add_argument("--private-reads", action="store_true")
            sub.add_argument("--cli-root")
            sub.add_argument("--luna-client", choices=("public", "private"), default="public", help="Private client requires --private-reads; never changes the TV caller identity")
            if name == "collect":
                sub.add_argument("--app", help="Refresh only one app; preserves all other prepared metadata/art")
                sub.add_argument("--marketing", type=pathlib.Path, help="Explicit local app description/art export; no URL downloads")
        elif name == "import":
            sub.add_argument("--input", type=pathlib.Path, required=True)
        elif name == "stage":
            sub.add_argument("--out", type=pathlib.Path, required=True)
    args = parser.parse_args(argv)
    if args.command == "inspect":
        print(json.dumps(inspect_connection(args), indent=2))
        return
    if args.command == "verify":
        result = verify_bundle(args.bundle)
    elif args.command == "stage":
        result = stage_bundle(args.bundle, args.out)
    else:
        with bundle_lock(args.bundle):
            prior = read_bounded(args.bundle / "current.json", 4096) if (args.bundle / "current.json").exists() else None
            existing = verify_bundle(args.bundle) if prior else None
            if args.command == "import":
                data, assets = import_data(args.input)
            else:
                transport = TvTransport(args.device, args.private_reads, args.cli_root, args.luna_client)
                try:
                    data, assets = collect_data(transport, args.device, existing, args.app, args.marketing)
                    data["preparation"] = dict(transport="existing-webos-cli-ssh2", luna_client=args.luna_client, configuration_sha256=transport.config_sha,
                                               remote_writes=False, requests=transport.calls)
                finally:
                    transport.close()
            protected = set(existing[1]) - {"covers/prepared/" + args.app, "hero/prepared/" + args.app} if args.command == "collect" and args.app and existing else set()
            result = commit(args.bundle, data, assets, prior, protected)
    print(json.dumps(summary(result), ensure_ascii=False, indent=2))

if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, TypeError, subprocess.SubprocessError) as error:
        # Do not echo raw device replies, auth exception bodies, or credential paths.
        message = str(error) if isinstance(error, PreparationError) else "Preparation failed; previous committed snapshot retained"
        print(json.dumps(dict(status="failed", message=message)), file=sys.stderr)
        sys.exit(1)
