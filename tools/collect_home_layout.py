#!/usr/bin/env python3
"""Collect the stock LG Home screen as app metadata (read-only).

The stock Home on webOS 24 is a Flutter app whose layout is declarative XML.
This tool reads that layout contract, the ordered launch points and the
canonical strings, and writes one reviewable bundle:

    home-layout.json   pages, rows, widget template, strings, ordered apps
    art/<id>.png       PNG-validated artwork, hashed
    manifest.json      every source path, byte count and sha256

Nothing is written to the TV, nothing is installed, no URL is fetched.
`/usr/palm` is only reachable from the root namespace, so file reads go
through the on-device root shim. Launch points are read with a root
`luna-send`; the SDK device record is never edited.

Usage:
    python3 tools/collect_home_layout.py collect --device c3 --out DIR
    python3 tools/collect_home_layout.py verify  --bundle DIR
"""
import base64
import datetime as dt
import json
import pathlib
import re
import shutil
import struct
import subprocess
import sys
import threading
import xml.etree.ElementTree as ET

APP_ID = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]{0,159}$")
SCHEMA = "spice-home.home-layout.v1"
HOME_PACKAGE = "com.webos.app.home"
ASSETS = "/usr/palm/applications/" + HOME_PACKAGE + "/data/flutter_assets/assets/"
LAYOUT_FILES = ("home.xml", "home_lg.xml", "home_layoutShelfView.xml", "home_widget_template.xml")
LOCALES = ("en.json",)
MAX_APPS = 200
MAX_ROWS = 64
MAX_ART_BYTES = 1200000
MAX_BG_BYTES = 1500000
# The first 12 launch-point indices may use their higher-resolution largeIcon
# source. This is an array-order cutoff, not a measured Home visibility claim.
# The offline budget check below refuses to publish if art would be dropped.
ICON_UPGRADE_COUNT = 12
ROOT_READ_BATCH = 6


class CollectError(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise CollectError(message)


def sha(data):
    import hashlib
    return hashlib.sha256(data).hexdigest()


def text(value, limit=120):
    if not isinstance(value, str):
        return ""
    cleaned = "".join(ch for ch in value if ch.isprintable())
    return cleaned[:limit]


def posix_int(value, default=0):
    try:
        return max(0, int(value))
    except (TypeError, ValueError):
        return default


class Transport:
    """JSON-line driver for tools/agent/tv_data_transport.cjs."""

    def __init__(self, device):
        node = shutil.which("node")
        require(node is not None, "Node is required for the TV transport")
        script = pathlib.Path(__file__).parent / "agent/tv_data_transport.cjs"
        require(script.is_file(), "TV transport script missing")
        self.process = subprocess.Popen([node, str(script), "--device", device, "--luna-client", "public", "--private-reads"],
                                        stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                        stderr=subprocess.DEVNULL, text=True, encoding="utf-8",
                                        creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        try:
            ready = self._read()
            require(ready.get("ready") is True, "TV transport unavailable: " + ready.get("fatal", "no-response"))
            require("device_configuration_sha256" in ready, "TV transport missing configuration hash")
        except BaseException:
            self._kill()
            raise
        self.device_sha = ready["device_configuration_sha256"]
        self.count = 0

    def _kill(self):
        if self.process.poll() is None:
            self.process.kill()
            self.process.wait(timeout=5)

    def _read(self):
        # A stalled transport must not hang the collector or leave a child holding
        # the TV connection. The timer interrupts the blocking pipe read.
        timer = threading.Timer(60, self._kill)
        timer.daemon = True
        timer.start()
        try:
            line = self.process.stdout.readline(4 * 1024 * 1024)
        finally:
            timer.cancel()
        require(line.endswith("\n"), "TV transport closed or oversized")
        return json.loads(line)

    def request(self, operation, **fields):
        self.count += 1
        rid = "req-%d" % self.count
        self.process.stdin.write(json.dumps(dict(request_id=rid, op=operation, **fields)) + "\n")
        self.process.stdin.flush()
        response = self._read()
        require(response.get("request_id") == rid, "Transport response identity mismatch")
        return response

    def root_files(self, paths, limit=0):
        response = self.request("rootread", paths=list(paths), limit=limit)
        if not response.get("ok"):
            return {}
        return response["data"]["files"]

    def close(self):
        if self.process.poll() is not None:
            return
        self.process.stdin.write('{"op":"close"}\n')
        self.process.stdin.flush()
        try:
            self._read()
        finally:
            self.process.stdin.close()
            try:
                self.process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self._kill()


def parse_layout(name, blob):
    require(blob.startswith(b"<?xml"), "Layout asset is not XML: " + name)
    try:
        root = ET.fromstring(blob.decode("utf-8"))
    except ET.ParseError as error:
        raise CollectError("Unparseable layout %s: %s" % (name, error))
    require(root.tag == "home", "Unexpected layout root in " + name)
    version = text(root.get("version", ""), 8)
    layouts = []
    for layout in root.findall("layout"):
        pages = []
        # ShelfView layouts declare their first page as <firstPage>, not <page>;
        # including both keeps their geometry instead of emitting empty pages.
        for page in list(layout.findall("page")) + list(layout.findall("firstPage")):
            rows = []
            for item in page.findall("item"):
                rows.append(dict(
                    id=text(item.get("id", ""), 48),
                    width=posix_int(item.get("itemWidth")),
                    height=posix_int(item.get("itemHeight")),
                    focus=text(item.get("focusType", ""), 16),
                    autofocus=item.get("autoFocus") == "true",
                ))
            pages.append(dict(pageBodyType=text(page.get("pageBodyType", ""), 24), rows=rows[:MAX_ROWS]))
        layouts.append(dict(
            type=text(layout.get("type", ""), 24),
            vMargin=posix_int(layout.get("vMargin")),
            windowType=text(layout.get("windowType", ""), 24),
            pageType=text(layout.get("pageType", ""), 24),
            pageCount=posix_int(layout.get("pageCount")),
            defaultPage=posix_int(layout.get("defaultPage")),
            pages=pages,
        ))
    return dict(version=version, layouts=layouts)


def parse_strings(blob, layout_ids):
    try:
        data = json.loads(blob.decode("utf-8"))
    except (ValueError, UnicodeDecodeError):
        raise CollectError("Unparseable strings file")
    require(isinstance(data, dict), "Strings file is not an object")
    wanted = {key: text(value, 120) for key, value in data.items()
              if isinstance(value, str) and any(part in key.lower() for part in layout_ids)}
    return wanted


def art_from(row, transport, assets, missing):
    """Read the icon and background PNGs for one launch point, in one batch."""
    wanted = []
    for field, limit in (("icon", MAX_ART_BYTES), ("largeIcon", MAX_ART_BYTES), ("bgImage", MAX_BG_BYTES)):
        value = row.get(field)
        if isinstance(value, str) and value.startswith("/") and value.lower().endswith(".png"):
            wanted.append((field, value, limit))
    result = {}
    for field, path, limit in wanted:
        files = transport.root_files([path], limit)
        entry = files.get(path) or {}
        if entry.get("error") or not entry.get("base64"):
            missing.append(dict(field=field, path=path, reason=entry.get("error", "unreadable")))
            continue
        blob = base64.b64decode(entry["base64"])
        if len(blob) > limit or not blob.startswith(b"\x89PNG\r\n\x1a\n"):
            missing.append(dict(field=field, path=path, reason="size-or-format"))
            continue
        key = "art/%s-%s.png" % (text(row.get("id", "app"), 60), field)
        assets[key] = blob
        result[field + "_key"] = key
        result[field + "_bytes"] = len(blob)
    return result


def collect(transport, pairs):
    now = dt.datetime.now(dt.timezone.utc).isoformat(timespec="seconds")
    files = transport.root_files([ASSETS + name for name in LAYOUT_FILES])
    layout = {}
    for name in LAYOUT_FILES:
        entry = files.get(ASSETS + name) or {}
        require(entry.get("base64"), "Stock layout asset unreadable: " + name)
        layout[name.removesuffix(".xml")] = parse_layout(name, base64.b64decode(entry["base64"]))
    strings = {}
    for locale in LOCALES:
        entry = transport.root_files([ASSETS + "i18n/" + locale]).get(ASSETS + "i18n/" + locale) or {}
        if entry.get("base64"):
            strings[locale] = parse_strings(base64.b64decode(entry["base64"]), layout.keys())

    info = transport.request("rootluna", uri="luna://com.webos.service.tv.systemproperty/getSystemInfo",
                             body=dict(keys=["modelName", "firmwareVersion", "sdkVersion", "UHD"]))
    device = {}
    if info.get("ok"):
        source = info["data"]
        device = {k: text(str(source[k]), 96) for k in ("modelName", "firmwareVersion", "sdkVersion", "UHD") if k in source}

    points = transport.request("rootluna", uri="luna://com.webos.applicationManager/listLaunchPoints", body={})
    require(points.get("ok"), "launch points unavailable: " + str(points.get("code")))
    rows = points["data"].get("launchPoints")
    require(isinstance(rows, list), "launch point payload missing")

    assets, missing, apps, skipped = {}, [], [], []
    seen = set()
    for index, row in enumerate(rows[:MAX_APPS * 2]):
        if not isinstance(row, dict):
            continue
        app_id = row.get("id", "")
        if not isinstance(app_id, str) or not APP_ID.fullmatch(app_id) or app_id in seen:
            continue
        seen.add(app_id)
        if row.get("hidden") is True:
            skipped.append(dict(id=app_id, reason="hidden"))
            continue
        entry = dict(
            index=len(apps), order_index=index, id=app_id,
            title=text(row.get("title", app_id), 96),
            title_visible=row.get("titleVisible") is not False,
            launch_point=text(row.get("launchPointId", ""), 160),
            ltype=text(row.get("lptype", ""), 24),
            bookmark_target=text((row.get("params") or {}).get("target", ""), 512),
            tile_size=text(row.get("tileSize", "normal"), 16),
            icon_color=text(row.get("iconColor", ""), 12),
            system_app=row.get("systemApp") is True,
            removable=row.get("removable") is not False,
            unmovable=row.get("unmovable") is True,
            badges=[text(b, 24) for b in (row.get("badges") or [])][:6],
            install_time=posix_int(row.get("installTime")),
        )
        entry.update(art_from(row, transport, assets, missing))
        apps.append(entry)
        if len(apps) >= MAX_APPS:
            break

    require(apps, "No usable launch points")
    used = set()
    for app in apps:
        for field in ("icon_key", "largeIcon_key", "bgImage_key"):
            if app.get(field):
                used.add(app[field])
    layout_ids = sorted(layout.keys())
    data = dict(
        schema=SCHEMA,
        captured_at=now,
        device=device,
        source=dict(package=HOME_PACKAGE, asset_root=ASSETS, layout_assets=list(LAYOUT_FILES),
                    source_kind="tv-stock-home", read_only=True,
                    inventory_scope="root listLaunchPoints", order_rule="array index"),
        layout=layout,
        strings=strings,
        apps=apps,
        skipped=skipped,
        unavailable=missing,
        counts=dict(apps=len(apps), rows=sum(len(p["rows"]) for lg in layout.values() for lay in lg["layouts"] for p in lay["pages"]),
                    art=len(used), missing=len(missing)),
        layout_ids=layout_ids,
    )
    manifest = dict(schema=SCHEMA + ".manifest", captured_at=now,
                    files=[dict(path=key, bytes=len(blob), sha256=sha(blob)) for key, blob in sorted(assets.items())],
                    device_configuration_sha256=transport.device_sha,
                    transport_requests=transport.count)
    return data, assets, manifest


def commit(out, data, assets, manifest):
    out = pathlib.Path(out)
    require(not out.exists(), "Output exists; choose a new directory")
    (out / "art").mkdir(parents=True)
    for key, blob in assets.items():
        (out / key).write_bytes(blob)
    (out / "home-layout.json").write_text(json.dumps(data, indent=1, sort_keys=True) + "\n", encoding="utf-8")
    (out / "manifest.json").write_text(json.dumps(manifest, indent=1, sort_keys=True) + "\n", encoding="utf-8")


def png_size(blob):
    require(blob[:8] == b"\x89PNG\r\n\x1a\n" and blob[12:16] == b"IHDR", "Not a PNG")
    return struct.unpack(">II", blob[16:24])


def stage(bundle, out):
    """Convert a layout bundle into the app's prepared-data format.

    Reuses the project's own strict writer/verifier instead of inventing a
    second bundle format, so `prepare_tv_data.py verify` proves the result.
    """
    sys.path.insert(0, str(pathlib.Path(__file__).parent))
    import prepare_tv_data as prep  # noqa: E402

    verify(bundle)
    bundle = pathlib.Path(bundle)
    layout = json.loads((bundle / "home-layout.json").read_text(encoding="utf-8"))
    assets, apps = {}, []
    for entry in layout["apps"]:
        app = dict(
            id=entry["id"], title=entry["title"],
            description=entry.get("bookmark_target") or "",
            vendor="", version="", observed_at=layout["captured_at"],
            provenance="tv-stock-home-launchpoint", inventory_confirmed=True,
            icon_key="", hero_key="",
            home_index=entry["index"], ltype=entry.get("ltype", ""),
            tile_size=entry.get("tile_size", "normal"), icon_color=entry.get("icon_color", ""),
            unmovable=entry.get("unmovable", False), bookmark_target=entry.get("bookmark_target", ""),
        )
        large = bundle / entry.get("largeIcon_key", "")
        use_large = int(entry.get("index", ICON_UPGRADE_COUNT)) < ICON_UPGRADE_COUNT and large.is_file()
        icon_field = "largeIcon_key" if use_large else "icon_key"
        icon = bundle / entry.get(icon_field, "")
        if entry.get(icon_field) and icon.is_file():
            blob = icon.read_bytes()
            if use_large:
                # Always normalize the higher-resolution source so the admitted
                # asset stays inside the 256x256 icon bound.
                try:
                    blob = prep.prepare_image(blob)
                except Exception:
                    # The large source is only a quality upgrade: fall back to
                    # the already-collected icon instead of losing the tile.
                    use_large = False
                    icon_field = "icon_key"
                    fallback = bundle / entry.get("icon_key", "")
                    blob = fallback.read_bytes() if fallback.is_file() else None
                    if blob is None:
                        app["icon_skipped"] = "large-icon-unconvertible"
            width, height = png_size(blob) if blob else (0, 0)
            if blob and not (blob[24] == 8 and blob[25] in (2, 6) and width <= 256 and height <= 256):
                # Live launcher icons are sometimes palette, 16-bit or oversized;
                # let the project's own normalizer re-encode rather than dropping
                # them, so no tile loses its artwork.
                try:
                    blob = prep.prepare_image(blob)
                    width, height = png_size(blob)
                except Exception:
                    app["icon_skipped"] = "unconvertible"
            if blob and width <= 256 and height <= 256 and blob[24] == 8 and blob[25] in (2, 6):
                app["icon_key"] = "covers/prepared/" + entry["id"]
                assets[app["icon_key"]] = blob
            else:
                app.setdefault("icon_skipped", "palette-or-depth")
        hero = bundle / entry.get("bgImage_key", "")
        if entry.get("bgImage_key") and hero.is_file():
            blob = hero.read_bytes()
            width, height = png_size(blob)
            if width <= 1920 and height <= 1080 and blob[24] == 8 and blob[25] == 2:
                app["hero_key"] = "hero/prepared/" + entry["id"]
                assets[app["hero_key"]] = blob
        apps.append(app)

    data = dict(
        schema=prep.BUNDLE_SCHEMA, captured_at=layout["captured_at"],
        source=dict(kind="tv", label="Stock LG Home layout", device=layout["device"]),
        device=layout["device"], apps=apps,
        home=dict(order_rule=layout["source"]["order_rule"], layout=layout["layout"],
                  strings=layout["strings"], layout_ids=layout["layout_ids"]),
        unavailable=layout["unavailable"],
    )
    # prepare_tv_data.commit silently drops assets when the budget is exceeded;
    # fail here instead of publishing a bundle that lost artwork.
    compressed=sum(len(blob) for blob in assets.values())
    rgba=sum(png_size(blob)[0]*png_size(blob)[1]*4 for blob in assets.values())
    require(compressed<=prep.MAX_COMPRESSED and rgba<=prep.MAX_RGBA,
            "Prepared graphics budget exceeded; reduce upgraded icons or heroes")
    out = pathlib.Path(out)
    with prep.bundle_lock(out):
        current = out / "current.json"
        prior = current.read_bytes() if current.exists() else None
        prep.commit(out, data, assets, prior)
    print("Staged %d apps, %d graphics -> %s" % (len(apps), len(assets), out))


def verify(bundle):
    bundle = pathlib.Path(bundle)
    manifest = json.loads((bundle / "manifest.json").read_text(encoding="utf-8"))
    for entry in manifest["files"]:
        blob = (bundle / entry["path"]).read_bytes()
        require(len(blob) == entry["bytes"], "Byte count mismatch: " + entry["path"])
        require(sha(blob) == entry["sha256"], "Hash mismatch: " + entry["path"])
        require(blob.startswith(b"\x89PNG"), "Not a PNG: " + entry["path"])
    data = json.loads((bundle / "home-layout.json").read_text(encoding="utf-8"))
    require(data["schema"] == SCHEMA, "Unknown bundle schema")
    require(data["counts"]["apps"] == len(data["apps"]), "App count mismatch")
    require(data["layout_ids"], "No layout captured")
    print("Verified %d art files, %d apps, layouts: %s" % (len(manifest["files"]), len(data["apps"]), ", ".join(data["layout_ids"])))


def main(argv):
    args = list(argv[1:])
    if not args:
        print(__doc__)
        return 2
    command = args.pop(0)
    opts = {}
    while args:
        key = args.pop(0)
        require(key.startswith("--"), "Unexpected argument: " + key)
        opts[key[2:]] = args.pop(0) if args else ""
    if command == "verify":
        verify(opts.get("bundle") or "")
        return 0
    if command == "stage":
        stage(opts.get("bundle") or "", opts.get("out") or "")
        return 0
    require(command == "collect", "Expected collect, stage or verify")
    require(opts.get("device"), "--device is required")
    require(opts.get("out"), "--out is required")
    transport = Transport(opts["device"])
    try:
        data, assets, manifest = collect(transport, None)
        commit(opts["out"], data, assets, manifest)
        print("Captured %d apps, %d art files, %d unavailable -> %s" %
              (data["counts"]["apps"], data["counts"]["art"], data["counts"]["missing"], opts["out"]))
    finally:
        transport.close()
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv))
    except CollectError as error:
        print("collect_home_layout: " + str(error), file=sys.stderr)
        sys.exit(1)
