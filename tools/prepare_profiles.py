#!/usr/bin/env python3
"""Prepare an explicitly requested, bounded, credential-free profile snapshot."""

from __future__ import annotations

import argparse
import base64
import binascii
import datetime as dt
import html.parser
import json
import os
import re
import ssl
import subprocess
import sys
import tempfile
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path

SCHEMA = "spice.profiles/1"
MAX_JSON = 512 * 1024
MAX_HTML = 768 * 1024
MAX_AVATAR = 96 * 1024
MAX_AVATAR_SIDE = 1024
TIMEOUT_SECONDS = 10
PLATFORMS = ("xbox", "psn")
SOURCES = {"provided", "public_observed", "imported", "sample"}
STATES = {"setup_needed", "untested", "unavailable", "auth_needed", "error", "stale", "public_observed", "imported", "sample"}
FIELDS = {"handle", "display_name", "avatar", "gamerscore", "achievements", "trophy_level", "trophies", "recent_games"}


def now():
    return dt.datetime.now(dt.timezone.utc).isoformat(timespec="seconds").replace("+00:00", "Z")


def valid_handle(platform, value):
    if not isinstance(value, str) or value != value.strip():
        return False
    if platform == "psn":
        return re.fullmatch(r"[A-Za-z0-9_-]{3,16}", value) is not None
    return (3 <= len(value) <= 32 and
            re.fullmatch(r"[\w .-]+(?:#[0-9]{1,6})?", value, re.UNICODE) is not None and
            not value.startswith((".", "-")) and ".." not in value)


def safe_text(value, limit=120):
    return isinstance(value, str) and 0 < len(value) <= limit and not any(ord(c) < 32 or ord(c) == 127 for c in value)


def valid_timestamp(value):
    if value is None:
        return True
    if not isinstance(value, str) or not re.fullmatch(r"\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(?:\.\d{1,6})?(?:Z|[+-]\d{2}:\d{2})", value):
        return False
    try:
        parsed = dt.datetime.fromisoformat(value.replace("Z", "+00:00"))
        return parsed.tzinfo is not None
    except ValueError:
        return False


def avatar_dimensions(data, kind):
    """Bound declared still-image canvases before any browser decoder sees them.

    Header admission only, not full codec validation. PNG/WebP container layouts:
    https://www.w3.org/TR/png-3/ and developers.google.com/speed/webp/docs/riff_container
    """
    def bounded(width, height):
        return (width, height) if 0 < width <= MAX_AVATAR_SIDE and 0 < height <= MAX_AVATAR_SIDE else None
    if kind == 'png' and len(data) >= 33 and data.startswith(b'\x89PNG\r\n\x1a\n') and data[8:16] == b'\0\0\0\rIHDR':
        dims = bounded(int.from_bytes(data[16:20], 'big'), int.from_bytes(data[20:24], 'big'))
        pos, seen_data = 8, False
        while dims and pos + 12 <= len(data):
            size, chunk = int.from_bytes(data[pos:pos + 4], 'big'), data[pos + 4:pos + 8]
            if size > len(data) - pos - 12 or chunk in (b'acTL', b'fcTL', b'fdAT', b'iCCP', b'zTXt', b'iTXt'):
                return None
            pos += size + 12
            seen_data |= chunk == b'IDAT'
            if chunk == b'IEND':
                return dims if size == 0 and seen_data and pos == len(data) else None
        return None
    if kind == 'jpeg' and data.startswith(b'\xff\xd8\xff'):
        pos = 2
        while pos + 4 <= len(data):
            if data[pos] != 255:
                return None
            while pos < len(data) and data[pos] == 255:
                pos += 1
            if pos + 3 > len(data):
                return None
            marker = data[pos];pos += 1
            if marker in (0, 0xD8, 0xD9, 0xDA):
                return None
            size = int.from_bytes(data[pos:pos + 2], 'big')
            if size < 2 or size > len(data) - pos:
                return None
            if marker in (0xC0, 0xC1, 0xC2):
                return bounded(int.from_bytes(data[pos + 5:pos + 7], 'big'), int.from_bytes(data[pos + 3:pos + 5], 'big')) if size >= 8 and data[pos + 2] == 8 else None
            pos += size
    if kind == 'webp' and len(data) >= 20 and data[:4] == b'RIFF' and data[8:12] == b'WEBP' and int.from_bytes(data[4:8], 'little') == len(data) - 8:
        pos, canvas, frame = 12, None, None
        while pos + 8 <= len(data):
            chunk, size = data[pos:pos + 4], int.from_bytes(data[pos + 4:pos + 8], 'little')
            if size > len(data) - pos - 8 or chunk in (b'ANIM', b'ANMF'):
                return None
            body = data[pos + 8:pos + 8 + size]
            if chunk == b'VP8X':
                if pos != 12 or size != 10 or body[0] & 2:
                    return None
                canvas = bounded(1 + int.from_bytes(body[4:7], 'little'), 1 + int.from_bytes(body[7:10], 'little'))
                if not canvas:
                    return None
            elif chunk in (b'VP8 ', b'VP8L'):
                if frame:
                    return None
                if chunk == b'VP8 ' and size >= 10 and body[3:6] == b'\x9d\x01\x2a' and not body[0] & 1:
                    frame = bounded(int.from_bytes(body[6:8], 'little') & 0x3fff, int.from_bytes(body[8:10], 'little') & 0x3fff)
                elif chunk == b'VP8L' and size >= 5 and body[0] == 0x2f:
                    bits = int.from_bytes(body[1:5], 'little')
                    if bits >> 29:
                        return None
                    frame = bounded(1 + (bits & 0x3fff), 1 + ((bits >> 14) & 0x3fff))
                if not frame:
                    return None
            elif pos == 12:
                return None
            pos += 8 + size + (size & 1)
        return frame if pos == len(data) and frame and (canvas is None or canvas == frame) else None
    return None


def valid_avatar(value):
    if not isinstance(value, str) or len(value) > MAX_AVATAR * 2:
        return False
    match = re.fullmatch(r"data:image/(png|jpeg|webp);base64,([A-Za-z0-9+/=]+)", value)
    if not match:
        return False
    try:
        data = base64.b64decode(match.group(2), validate=True)
    except (ValueError, binascii.Error):
        return False
    return 0 < len(data) <= MAX_AVATAR and avatar_dimensions(data, match.group(1)) is not None


def empty_profile():
    return {"state": "setup_needed", "fields": {}, "last_attempt": None}


def empty_document():
    return {"schema": SCHEMA, "generated_at": now(), "profiles": {p: empty_profile() for p in PLATFORMS}}


def normalize_document(raw):
    """Reject malformed known values; discard unknown keys instead of exporting them."""
    if not isinstance(raw, dict) or raw.get("schema") != SCHEMA or not isinstance(raw.get("profiles"), dict):
        raise ValueError("Expected a spice.profiles/1 document")
    generated = raw.get("generated_at")
    if not valid_timestamp(generated):
        raise ValueError("Invalid generated_at timestamp")
    out = {"schema": SCHEMA, "generated_at": generated, "profiles": {}}
    for platform in PLATFORMS:
        profile = raw["profiles"].get(platform, empty_profile())
        if not isinstance(profile, dict) or profile.get("state") not in STATES or not isinstance(profile.get("fields"), dict):
            raise ValueError("Invalid " + platform + " profile")
        clean = {"state": profile["state"], "fields": {}, "last_attempt": None}
        for key, item in profile["fields"].items():
            if key not in FIELDS:
                continue
            if not isinstance(item, dict) or item.get("source") not in SOURCES or not valid_timestamp(item.get("observed_at")):
                raise ValueError("Invalid provenance for " + key)
            value = item.get("value")
            valid = False
            if key == "handle":
                valid = valid_handle(platform, value)
            elif key == "display_name":
                valid = safe_text(value)
            elif key == "avatar":
                valid = valid_avatar(value)
            elif key in {"gamerscore", "achievements", "trophy_level"}:
                valid = type(value) is int and 0 <= value <= 1000000000
            elif key == "trophies":
                valid = (isinstance(value, dict) and set(value) <= {"platinum", "gold", "silver", "bronze"} and
                         bool(value) and all(type(v) is int and 0 <= v <= 1000000000 for v in value.values()))
                if valid:
                    value = {k: value[k] for k in ("platinum", "gold", "silver", "bronze") if k in value}
            elif key == "recent_games":
                valid = isinstance(value, list) and len(value) <= 20 and all(safe_text(v) for v in value)
            if not valid:
                raise ValueError("Invalid value for " + key)
            clean["fields"][key] = {"value": value, "source": item["source"], "observed_at": item.get("observed_at")}
        attempt = profile.get("last_attempt")
        if attempt is not None:
            if (not isinstance(attempt, dict) or not valid_timestamp(attempt.get("at")) or not attempt.get("at") or
                    attempt.get("result") not in STATES or not safe_text(attempt.get("detail"), 240)):
                raise ValueError("Invalid last_attempt")
            clean["last_attempt"] = {k: attempt[k] for k in ("at", "result", "detail")}
        out["profiles"][platform] = clean
    return out


def read_document(path):
    with open(path, "rb") as source:
        data = source.read(MAX_JSON + 1)
    if len(data) > MAX_JSON:
        raise ValueError("Snapshot exceeds size limit")
    return normalize_document(json.loads(data.decode("utf-8")))


def atomic_write(path, document):
    path = Path(path)
    payload = (json.dumps(normalize_document(document), indent=2, ensure_ascii=False) + "\n").encode("utf-8")
    if len(payload) > MAX_JSON:
        raise ValueError("Snapshot exceeds size limit")
    fd, name = tempfile.mkstemp(prefix=".profiles-", suffix=".tmp", dir=str(path.parent))
    try:
        with os.fdopen(fd, "wb") as output:
            output.write(payload)
            output.flush()
            os.fsync(output.fileno())
        os.replace(name, path)
    finally:
        if os.path.exists(name):
            os.unlink(name)


def profile_url(platform, handle):
    if platform not in PLATFORMS or not valid_handle(platform, handle):
        raise ValueError("Invalid profile handle")
    if platform == "xbox":
        return "https://www.xbox.com/en-US/play/user?" + urllib.parse.urlencode({"gamertag": handle})
    return "https://profile.playstation.com/" + urllib.parse.quote(handle, safe="")


def allowed_url(platform, handle, url):
    try:
        parts = urllib.parse.urlsplit(url)
        if parts.scheme != "https" or parts.username or parts.password or parts.port not in (None, 443) or parts.fragment:
            return False
        if platform == "xbox":
            return (parts.hostname == "www.xbox.com" and
                    re.fullmatch(r"/(?:[a-z]{2}-[A-Z]{2}/)?play/user", parts.path) is not None and
                    urllib.parse.parse_qsl(parts.query, keep_blank_values=True, strict_parsing=True) == [("gamertag", handle)])
        return (platform == "psn" and parts.hostname == "profile.playstation.com" and
                parts.path == "/" + urllib.parse.quote(handle, safe="") and not parts.query)
    except (ValueError, TypeError):
        return False


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, request, fp, code, message, headers, newurl):
        return None


class JsonLdParser(html.parser.HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.collect = False
        self.buffer = []
        self.documents = []

    def handle_starttag(self, tag, attrs):
        if tag.lower() == "script" and dict(attrs).get("type", "").lower() == "application/ld+json":
            self.collect, self.buffer = True, []

    def handle_data(self, data):
        if self.collect:
            self.buffer.append(data)

    def handle_endtag(self, tag):
        if tag.lower() == "script" and self.collect:
            self.collect = False
            try:
                self.documents.append(json.loads("".join(self.buffer)))
            except (ValueError, RecursionError):
                pass


def parse_public_html(body, handle):
    """Conservative experimental parser: a matching JSON-LD Person is required."""
    parser = JsonLdParser()
    parser.feed(body.decode("utf-8", "replace"))
    pending = list(parser.documents)
    visited = 0
    while pending and visited < 500:
        item = pending.pop()
        visited += 1
        if isinstance(item, list):
            pending.extend(item[:100])
        elif isinstance(item, dict):
            kind = item.get("@type")
            if (kind == "Person" or isinstance(kind, list) and "Person" in kind) and item.get("alternateName") == handle:
                name = item.get("name")
                return {"display_name": name} if safe_text(name) and "@" not in name else {}
            graph = item.get("@graph")
            if isinstance(graph, (list, dict)):
                pending.append(graph)
    return None


def fetch_public(platform, handle, opener=None):
    """One anonymous GET chain, restricted to the chosen identity and origin."""
    url = profile_url(platform, handle)
    opener = opener or urllib.request.build_opener(NoRedirect(), urllib.request.HTTPSHandler(context=ssl.create_default_context()))
    for _ in range(3):
        if not allowed_url(platform, handle, url):
            return {"result": "unavailable", "detail": "Redirect outside the allowlisted profile URL was blocked."}
        request = urllib.request.Request(url, headers={"Accept": "text/html", "User-Agent": "SpiceHomeProfiles/1.0"}, method="GET")
        try:
            response = opener.open(request, timeout=TIMEOUT_SECONDS)
        except urllib.error.HTTPError as exc:
            response = exc
        except (OSError, ValueError):
            return {"result": "error", "detail": "The public request failed or timed out."}
        with response:
            status = response.status
            if status in (301, 302, 303, 307, 308):
                location = response.headers.get("Location")
                if not location:
                    return {"result": "unavailable", "detail": "The public response had no redirect target."}
                url = urllib.parse.urljoin(url, location)
                continue
            if status == 401:
                return {"result": "auth_needed", "detail": "The public request returned HTTP " + str(status) + "."}
            if status == 403:
                return {"result": "unavailable", "detail": "The public request returned HTTP 403; the cause is unknown."}
            if status != 200:
                return {"result": "unavailable", "detail": "The public request returned HTTP " + str(status) + "."}
            if "text/html" not in response.headers.get("Content-Type", "").lower():
                return {"result": "unavailable", "detail": "The public response was not HTML."}
            try:
                body = response.read(MAX_HTML + 1)
            except OSError:
                return {"result": "error", "detail": "The public response could not be read."}
            if len(body) > MAX_HTML:
                return {"result": "error", "detail": "The public response exceeded the byte limit."}
            parsed = parse_public_html(body, handle)
            if parsed is None:
                return {"result": "unavailable", "detail": "No matching JSON-LD Person was observed in the fetched HTML; account availability remains unknown."}
            return {"result": "public_observed", "detail": "A matching structured public profile was observed.", "fields": parsed}
    return {"result": "unavailable", "detail": "The public profile exceeded the redirect limit."}


def bounded_fetch(platform, handle):
    """A separate process bounds DNS, redirects, and a slow streaming response."""
    try:
        completed = subprocess.run([sys.executable, str(Path(__file__).resolve()), "--worker", platform, handle],
                                   capture_output=True, timeout=TIMEOUT_SECONDS, check=False)
        if completed.returncode != 0 or len(completed.stdout) > 4096:
            return {"result": "error", "detail": "The public request worker failed."}
        result = json.loads(completed.stdout.decode("utf-8"))
        if not isinstance(result, dict) or result.get("result") not in {"public_observed", "unavailable", "auth_needed", "error"}:
            raise ValueError("Invalid worker result")
        return result
    except subprocess.TimeoutExpired:
        return {"result": "error", "detail": "The public request exceeded the time limit."}
    except (OSError, UnicodeError, ValueError):
        return {"result": "error", "detail": "The public request worker failed."}


def apply_result(document, platform, handle, result, at=None):
    at = at or now()
    profile = document["profiles"][platform]
    if profile["fields"].get("handle", {}).get("value") != handle:
        profile = document["profiles"][platform] = empty_profile()
        profile["fields"]["handle"] = {"value": handle, "source": "provided", "observed_at": None}
    profile["last_attempt"] = {"at": at, "result": result["result"], "detail": result["detail"]}
    if result["result"] == "public_observed":
        profile["state"] = "public_observed"
        profile["fields"]["handle"] = {"value": handle, "source": "public_observed", "observed_at": at}
        for key, value in result.get("fields", {}).items():
            if key == "display_name":
                profile["fields"][key] = {"value": value, "source": "public_observed", "observed_at": at}
    else:
        has_good = any(key != "handle" and value["source"] in {"public_observed", "imported"} for key, value in profile["fields"].items())
        has_good = has_good or any(value["source"] == "public_observed" for value in profile["fields"].values())
        profile["state"] = "stale" if has_good else result["result"]
    document["generated_at"] = at
    return normalize_document(document)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", required=True, help="Snapshot path; an existing valid snapshot is preserved on refresh failure")
    parser.add_argument("--import-json", help="Import an existing spice.profiles/1 JSON document")
    parser.add_argument("--platform", choices=PLATFORMS)
    parser.add_argument("--handle", help="Explicit PSN online ID or Xbox gamertag")
    parser.add_argument("--refresh", action="store_true", help="Make an explicit anonymous HTTPS public-profile request")
    args = parser.parse_args(argv)
    if (args.handle or args.refresh) and not args.platform:
        parser.error("--platform is required with --handle or --refresh")
    try:
        out = Path(args.out)
        if out.exists():
            document = read_document(out)  # Never overwrite an invalid existing file.
        else:
            document = empty_document()
        if args.import_json:
            document = read_document(args.import_json)
            for profile in document["profiles"].values():
                if profile["state"] != "sample" and profile["fields"]:
                    profile["state"] = "imported"
                    for field in profile["fields"].values():
                        if field["source"] != "sample":
                            field["source"] = "imported"
        if args.platform:
            profile = document["profiles"][args.platform]
            handle = args.handle or profile["fields"].get("handle", {}).get("value")
            if handle and not valid_handle(args.platform, handle):
                raise ValueError("Invalid handle")
            if args.handle and profile["fields"].get("handle", {}).get("value") != handle:
                profile = document["profiles"][args.platform] = empty_profile()
                profile["fields"]["handle"] = {"value": handle, "source": "provided", "observed_at": None}
                profile["state"] = "untested"
            if args.refresh:
                if not handle:
                    raise ValueError("A handle is required before refresh")
                document = apply_result(document, args.platform, handle, bounded_fetch(args.platform, handle))
        document["generated_at"] = now()
        atomic_write(out, document)
        print("Snapshot written: " + str(out))
        for platform in PLATFORMS:
            print(platform + ": " + document["profiles"][platform]["state"])
        return 0
    except (ValueError, OSError, UnicodeError, json.JSONDecodeError) as exc:
        print("Profile preparation failed: " + str(exc), file=sys.stderr)
        return 2


if __name__ == "__main__":
    if len(sys.argv) == 4 and sys.argv[1] == "--worker":
        try:
            print(json.dumps(fetch_public(sys.argv[2], sys.argv[3]), ensure_ascii=True))
        except Exception:
            print(json.dumps({"result": "error", "detail": "The public request worker failed."}))
        sys.exit(0)
    sys.exit(main())
