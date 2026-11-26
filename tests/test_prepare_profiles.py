import copy
import base64
import struct
import zlib
import importlib.util
import io
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("prepare_profiles", ROOT / "tools" / "prepare_profiles.py")
profiles = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(profiles)


class Response(io.BytesIO):
    def __init__(self, status, body=b"", headers=None):
        super().__init__(body)
        self.status = status
        self.headers = headers or {}


class FakeOpener:
    def __init__(self, *responses):
        self.responses = list(responses)
        self.requests = []

    def open(self, request, timeout):
        self.requests.append((request.full_url, request.get_method(), timeout))
        return self.responses.pop(0)


class ProfilePreparationTests(unittest.TestCase):
    def field(self, value, source="imported"):
        return {"value": value, "source": source, "observed_at": "2026-11-04T10:00:00Z"}

    def known_good(self):
        document = profiles.empty_document()
        xbox = document["profiles"]["xbox"]
        xbox["state"] = "imported"
        xbox["fields"] = {"handle": self.field("PlayerOne"), "gamerscore": self.field(42),
                          "recent_games": self.field(["Game One"])}
        return document

    def test_handles_and_traversal(self):
        for value in ("../../admin", "//evil.example", "name@", "player?x=y", "name%2fetc", "..", "name\nother"):
            self.assertFalse(profiles.valid_handle("xbox", value), value)
            with self.assertRaises(ValueError):
                profiles.profile_url("xbox", value)
        self.assertFalse(profiles.valid_handle("psn", "a/b"))
        self.assertTrue(profiles.valid_handle("xbox", "Player One#1234"))
        self.assertTrue(profiles.valid_handle("psn", "Player_1"))

    def test_redirect_allowlist_rejects_ssrf_and_identity_change(self):
        good = profiles.profile_url("xbox", "PlayerOne")
        self.assertTrue(profiles.allowed_url("xbox", "PlayerOne", good))
        for bad in ("http://www.xbox.com/en-US/play/user?gamertag=PlayerOne",
                    "https://www.xbox.com.evil.test/en-US/play/user?gamertag=PlayerOne",
                    "https://" + "user" + chr(64) + "www.xbox.com/en-US/play/user?gamertag=PlayerOne",
                    "https://www.xbox.com:444/en-US/play/user?gamertag=PlayerOne",
                    "https://www.xbox.com/en-US/../play/user?gamertag=PlayerOne",
                    "https://www.xbox.com/en-US/play/user?gamertag=Other",
                    "https://www.xbox.com/en-US/play/user?gamertag=PlayerOne&next=http://127.0.0.1",
                    "https://www.xbox.com/en-US/play/user?gamertag=PlayerOne#fragment"):
            self.assertFalse(profiles.allowed_url("xbox", "PlayerOne", bad), bad)
        self.assertFalse(profiles.allowed_url("psn", "PlayerOne", "https://profile.playstation.com/%2e%2e/PlayerOne"))

    def test_cross_origin_redirect_is_not_fetched(self):
        opener = FakeOpener(Response(302, headers={"Location": "https://127.0.0.1/private"}))
        result = profiles.fetch_public("xbox", "PlayerOne", opener)
        self.assertEqual(result["result"], "unavailable")
        self.assertEqual(len(opener.requests), 1)
        self.assertNotIn("127.0.0.1", json.dumps(result))

    def test_allowed_redirect_and_non_profile_page(self):
        opener = FakeOpener(Response(307, headers={"Location": "/en-CA/play/user?gamertag=PlayerOne"}),
                            Response(200, b"<html><title>View profile</title>PlayerOne</html>", {"Content-Type": "text/html"}))
        result = profiles.fetch_public("xbox", "PlayerOne", opener)
        self.assertEqual(result["result"], "unavailable")
        self.assertEqual(len(opener.requests), 2)
        self.assertTrue(all(request[1] == "GET" for request in opener.requests))

    def test_only_matching_synthetic_jsonld_profile_is_observed(self):
        # Synthetic contract fixture: not evidence of any real provider response.
        body = b'<script type="application/ld+json">{"@type":"Person","alternateName":"PlayerOne","name":"Example Player"}</script>'
        self.assertEqual(profiles.parse_public_html(body, "PlayerOne"), {"display_name": "Example Player"})
        self.assertIsNone(profiles.parse_public_html(body, "SomeoneElse"))
        self.assertIsNone(profiles.parse_public_html(b'<script type="application/ld+json">{broken</script>', "PlayerOne"))
        opener = FakeOpener(Response(200, body, {"Content-Type": "text/html; charset=utf-8"}))
        self.assertEqual(profiles.fetch_public("xbox", "PlayerOne", opener)["result"], "public_observed")

    def test_sanitized_observed_xbox_title_is_not_profile_data(self):
        # The title below is the only copied excerpt from the actual anonymous
        # 2026-11-05 UTC HTML response; this is not the full provider response.
        body = "<title>View profile | Xbox Cloud Gaming on\u200a\u2009Xbox.com</title>".encode("utf-8")
        self.assertIsNone(profiles.parse_public_html(body, "pikldspratmn"))

    def test_http_and_size_statuses(self):
        self.assertEqual(profiles.fetch_public("xbox", "PlayerOne", FakeOpener(Response(401)))["result"], "auth_needed")
        self.assertEqual(profiles.fetch_public("xbox", "PlayerOne", FakeOpener(Response(403)))["result"], "unavailable")
        self.assertEqual(profiles.fetch_public("xbox", "PlayerOne", FakeOpener(Response(404)))["result"], "unavailable")
        oversized = Response(200, b"x" * (profiles.MAX_HTML + 1), {"Content-Type": "text/html"})
        self.assertEqual(profiles.fetch_public("xbox", "PlayerOne", FakeOpener(oversized))["result"], "error")

    def test_worker_timeout_is_an_error(self):
        with mock.patch.object(profiles.subprocess, "run", side_effect=subprocess.TimeoutExpired("worker", 10)):
            self.assertEqual(profiles.bounded_fetch("xbox", "PlayerOne")["result"], "error")

    def test_failed_refresh_preserves_values_and_provenance(self):
        document = self.known_good()
        before = copy.deepcopy(document["profiles"]["xbox"]["fields"])
        result = profiles.apply_result(document, "xbox", "PlayerOne", {"result": "error", "detail": "Request failed."}, "2026-11-04T12:00:00Z")
        self.assertEqual(result["profiles"]["xbox"]["state"], "stale")
        self.assertEqual(result["profiles"]["xbox"]["fields"]["gamerscore"], before["gamerscore"])
        self.assertEqual(result["profiles"]["xbox"]["fields"]["recent_games"], before["recent_games"])
        self.assertEqual(result["profiles"]["xbox"]["fields"]["handle"], before["handle"])
        self.assertEqual(result["profiles"]["xbox"]["last_attempt"]["result"], "error")

    def test_failed_refresh_preserves_public_identity_even_if_it_is_the_only_observation(self):
        document = profiles.empty_document()
        handle = self.field("PlayerOne", "public_observed")
        document["profiles"]["xbox"] = {"state": "public_observed", "fields": {"handle": handle}, "last_attempt": None}
        result = profiles.apply_result(document, "xbox", "PlayerOne", {"result": "unavailable", "detail": "Unavailable."})
        self.assertEqual(result["profiles"]["xbox"]["state"], "stale")
        self.assertEqual(result["profiles"]["xbox"]["fields"]["handle"], handle)

    def test_changing_identity_drops_old_data(self):
        result = profiles.apply_result(self.known_good(), "xbox", "Another", {"result": "unavailable", "detail": "Unavailable."})
        self.assertEqual(set(result["profiles"]["xbox"]["fields"]), {"handle"})

    def test_malformed_import_and_unknown_keys(self):
        document = self.known_good()
        document["unexpected"] = "discard"
        document["profiles"]["xbox"]["fields"]["unknown"] = self.field("discard")
        normalized = profiles.normalize_document(document)
        self.assertNotIn("unexpected", normalized)
        self.assertNotIn("unknown", normalized["profiles"]["xbox"]["fields"])
        document["profiles"]["xbox"]["fields"]["gamerscore"]["value"] = True
        with self.assertRaises(ValueError):
            profiles.normalize_document(document)
        with self.assertRaises(ValueError):
            profiles.normalize_document({"schema": "wrong", "profiles": {}})

    def test_timestamp_validation_rejects_invalid_dates(self):
        for value in ("2026-02-30T12:00:00Z", "2026-11-04Z", "2026-11-04T12:00:00", "2026-11-04T12:00:00+24:00"):
            self.assertFalse(profiles.valid_timestamp(value), value)
        self.assertTrue(profiles.valid_timestamp("2026-11-04T12:00:00.123Z"))
        self.assertTrue(profiles.valid_timestamp("2026-11-04T12:00:00-05:00"))

    def test_html_injection_is_data_and_remote_or_svg_avatar_rejected(self):
        document = self.known_good()
        injection = '<img src=x onerror=alert(1)>'
        document["profiles"]["xbox"]["fields"]["display_name"] = self.field(injection)
        self.assertEqual(profiles.normalize_document(document)["profiles"]["xbox"]["fields"]["display_name"]["value"], injection)
        for avatar in ("https://attacker.test/a.png", "data:image/svg+xml;base64,PHN2Zz4=", "data:image/png;base64,PHN2Zz4="):
            document["profiles"]["xbox"]["fields"]["avatar"] = self.field(avatar)
            with self.assertRaises(ValueError):
                profiles.normalize_document(document)

    def test_avatar_pixel_budget_precedes_browser_decode(self):
        def png(width, height):
            def chunk(kind, data):
                return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
            compressor = zlib.compressobj()
            row = b'\0' + b'\x20\x40\x60\xff' * width
            payload = b''.join(compressor.compress(row) for _ in range(height)) + compressor.flush()
            return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0)) + chunk(b'IDAT', payload) + chunk(b'IEND', b'')
        small = 'data:image/png;base64,' + base64.b64encode(png(128, 128)).decode()
        large = 'data:image/png;base64,' + base64.b64encode(png(4096, 4096)).decode()
        self.assertLess(len(base64.b64decode(large.split(',')[1])), profiles.MAX_AVATAR)
        self.assertTrue(profiles.valid_avatar(small))
        self.assertFalse(profiles.valid_avatar(large))

    def test_static_avatar_formats_and_animation_rejection(self):
        fixtures = json.loads((ROOT / 'tests' / 'fixtures' / 'profile-avatars.json').read_text())
        for kind in ('png', 'jpeg', 'webp'):
            self.assertTrue(profiles.valid_avatar(fixtures[kind]), kind)
        for kind in ('oversized_png', 'animated_webp'):
            self.assertFalse(profiles.valid_avatar(fixtures[kind]), kind)

    def test_atomic_round_trip_and_invalid_existing_file_is_untouched(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "snapshot.json"
            document = self.known_good()
            profiles.atomic_write(path, document)
            self.assertEqual(profiles.read_document(path)["profiles"]["xbox"]["fields"], document["profiles"]["xbox"]["fields"])
            path.write_text("{broken", encoding="utf-8")
            before = path.read_bytes()
            self.assertEqual(profiles.main(["--out", str(path)]), 2)
            self.assertEqual(path.read_bytes(), before)
            self.assertEqual(sorted(p.name for p in path.parent.iterdir()), ["snapshot.json"])

    def test_failed_replace_keeps_original(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "snapshot.json"
            profiles.atomic_write(path, self.known_good())
            original = path.read_bytes()
            with mock.patch.object(profiles.os, "replace", side_effect=OSError("simulated")):
                with self.assertRaises(OSError):
                    profiles.atomic_write(path, profiles.empty_document())
            self.assertEqual(path.read_bytes(), original)
            self.assertEqual(sorted(p.name for p in path.parent.iterdir()), ["snapshot.json"])


if __name__ == "__main__":
    unittest.main()
