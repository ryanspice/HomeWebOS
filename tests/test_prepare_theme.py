import importlib.util
import json
import os
from pathlib import Path
import shutil
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("prepare_theme", ROOT / "tools" / "prepare_theme.py")
p = importlib.util.module_from_spec(spec)
spec.loader.exec_module(p)


class ThemePreparationTests(unittest.TestCase):
    def setUp(self):
        test_dir = os.environ.get("SPICE_THEME_TEST_TMP")
        self.tmp = tempfile.TemporaryDirectory(prefix="theme-test-", dir=test_dir)
        self.base = Path(self.tmp.name)
        self.source = self.base / "source"
        shutil.copytree(ROOT / "demos" / "themes" / "geometric", self.source)
        self.root = self.base / "data" / "themes"

    def tearDown(self):
        self.tmp.cleanup()

    def import_(self):
        return p.import_theme(self.root, self.source / "source.json")

    def source_data(self):
        return json.loads((self.source / "source.json").read_text(encoding="utf-8"))

    def put_source(self, source):
        (self.source / "source.json").write_bytes(p.encode(source))

    def rewrite_manifest(self, mutate):
        pointer, _ = p.pointer(self.root)
        path = self.root / pointer["selected"]["manifest"]
        manifest = json.loads(path.read_bytes())
        mutate(manifest)
        raw = p.encode(manifest)
        path.write_bytes(raw)
        pointer["selected"]["sha256"] = p.sha(raw)
        (self.root / "current.json").write_bytes(p.encode(pointer))

    def test_import_verify_deduplicate_and_fallback_metadata(self):
        source = self.source_data()
        source["icons"][1]["file"] = source["icons"][0]["file"]
        self.put_source(source)
        result = self.import_()
        self.assertEqual(p.verify(self.root), result)
        self.assertEqual(result["icons"][0]["image"], result["icons"][1]["image"])
        self.assertEqual(len(result["images"]), 8)
        self.assertLessEqual(result["rgba_bytes"], p.MAX_RGBA)
        self.assertEqual(result["background_policy"], "missing")
        self.assertEqual(result["apps"][0]["id"], "com.example.geometric")

    def test_corrupt_manifest_and_asset_fail(self):
        self.import_()
        pointer, _ = p.pointer(self.root)
        path = self.root / pointer["selected"]["manifest"]
        original = path.read_bytes()
        path.write_bytes(original + b" ")
        with self.assertRaisesRegex(p.ThemeError, "manifest hash"):
            p.verify(self.root)
        path.write_bytes(original)
        manifest = p.verify(self.root)
        image = path.parent / manifest["images"][0]["path"]
        image.write_bytes(image.read_bytes() + b"bad")
        with self.assertRaisesRegex(p.ThemeError, "hash/length"):
            p.verify(self.root)

    def test_crc_and_incomplete_decode_even_when_hash_is_updated(self):
        self.import_()
        pointer, _ = p.pointer(self.root)
        path = self.root / pointer["selected"]["manifest"]
        manifest = p.verify(self.root)
        image = path.parent / manifest["images"][0]["path"]
        bad = bytearray(image.read_bytes())
        bad[-1] ^= 1
        with self.assertRaisesRegex(p.ThemeError, "CRC"):
            p.verify_png(bad, manifest["images"][0]["kind"] == "background")
        # Truncating the compressed data but recomputing the PNG CRC still fails decode.
        blob = image.read_bytes()
        start = blob.index(b"IDAT") - 4
        length = int.from_bytes(blob[start:start+4], "big")
        body = blob[start+8:start+8+length][:max(1, length//2)]
        chunk = len(body).to_bytes(4, "big") + b"IDAT" + body + (p.zlib.crc32(b"IDAT" + body) & 0xffffffff).to_bytes(4, "big")
        truncated = blob[:start] + chunk + blob[start+12+length:]
        with self.assertRaises(p.ThemeError):
            p.verify_png(truncated, manifest["images"][0]["kind"] == "background")

    def test_duplicate_keys_in_source_and_pointer_rejected(self):
        path = self.source / "source.json"
        path.write_bytes(path.read_bytes().replace(b'"schema":', b'"schema":"evil", "\\u0073chema":', 1))
        with self.assertRaisesRegex(p.ThemeError, "Duplicate"):
            self.import_()
        shutil.copyfile(ROOT / "demos" / "themes" / "geometric" / "source.json", path)
        self.import_()
        current = self.root / "current.json"
        current.write_bytes(current.read_bytes().replace(b'"schema":', b'"schema":"evil", "\\u0073chema":', 1))
        with self.assertRaisesRegex(p.ThemeError, "Duplicate"):
            p.verify(self.root)

    def test_traversal_links_and_hardlinks_denied(self):
        source = self.source_data()
        for bad in ("../outside.png", "/etc/passwd", "https://example.test/icon.png", "x/../icon.png", "x\\icon.png"):
            source["background"] = bad
            self.put_source(source)
            with self.subTest(path=bad), self.assertRaises(p.ThemeError):
                self.import_()
        # Symlinks can require Windows developer permissions; unsupported creation is skipped.
        try:
            (self.source / "linked.png").symlink_to(self.source / "background.png")
        except (OSError, NotImplementedError):
            pass
        else:
            source["background"] = "linked.png"
            self.put_source(source)
            with self.assertRaisesRegex(p.ThemeError, "links"):
                self.import_()
        try:
            os.link(self.source / "background.png", self.source / "hard.png")
        except (OSError, NotImplementedError):
            pass
        else:
            source["background"] = "hard.png"
            self.put_source(source)
            with self.assertRaisesRegex(p.ThemeError, "unlinked"):
                self.import_()

    def test_unsupported_layers_source_formats_and_duplicate_slots(self):
        source = self.source_data()
        source["layers"] = [{"format": "zstd-dds"}]
        self.put_source(source)
        with self.assertRaisesRegex(p.ThemeError, "unsupported"):
            self.import_()
        source.pop("layers")
        source["icons"][1]["slot"] = source["icons"][0]["slot"]
        self.put_source(source)
        with self.assertRaisesRegex(p.ThemeError, "duplicate"):
            self.import_()
        with self.assertRaisesRegex(p.ThemeError, "p5theme"):
            p.import_theme(self.root, self.source / "unsupported.p5theme")

    def test_unsupported_manifest_fields_and_duplicate_bindings(self):
        self.import_()
        self.rewrite_manifest(lambda m: m.update({"layers": ["svg"]}))
        with self.assertRaisesRegex(p.ThemeError, "unsupported"):
            p.verify(self.root)
        # A fresh fixture is used because corrupt generations are immutable by contract.
        self.root = self.base / "data2" / "themes"
        self.import_()
        self.rewrite_manifest(lambda m: m["apps"].append(dict(m["apps"][0])))
        with self.assertRaisesRegex(p.ThemeError, "duplicate"):
            p.verify(self.root)

    def test_invalid_dimensions_counts_and_bound_paths_fail(self):
        source = self.source_data()
        from PIL import Image
        Image.new("RGB", (8193, 1), "red").save(self.source / "too-wide.png")
        source["background"] = "too-wide.png"
        self.put_source(source)
        with self.assertRaisesRegex(p.ThemeError, "dimensions"):
            self.import_()
        source = self.source_data()
        source["background"] = "background.png"
        source["apps"] = [dict(id="com.example.app%s" % i, hero="hero.png") for i in range(65)]
        self.put_source(source)
        with self.assertRaisesRegex(p.ThemeError, "count"):
            self.import_()
        self.put_source(json.loads((ROOT / "demos" / "themes" / "geometric" / "source.json").read_bytes()))
        self.import_()
        self.rewrite_manifest(lambda m: m["images"][0].update(path="../outside.png"))
        with self.assertRaisesRegex(p.ThemeError, "content-addressed"):
            p.verify(self.root)

    def test_budget_rejects_before_publication(self):
        self.import_()
        before = (self.root / "current.json").read_bytes()
        source = self.source_data()
        from PIL import Image
        for i in range(4):
            name = "extra-%s.png" % i
            Image.new("RGB", (8, 8), (i * 40, 2, 3)).save(self.source / name)
            source["apps"].append(dict(id="com.example.extra%s" % i, hero=name))
        self.put_source(source)
        with self.assertRaisesRegex(p.ThemeError, "budget"):
            self.import_()
        self.assertEqual((self.root / "current.json").read_bytes(), before)
        self.assertEqual(p.verify(self.root)["name"], "Geometric Blue")

    def test_atomic_replace_failure_keeps_current_and_previous(self):
        self.import_()
        before = (self.root / "current.json").read_bytes()
        source = self.source_data()
        source["name"] = "Second"
        self.put_source(source)
        with mock.patch.object(p.os, "replace", side_effect=OSError("injected interruption")):
            with self.assertRaisesRegex(OSError, "interruption"):
                self.import_()
        self.assertEqual((self.root / "current.json").read_bytes(), before)
        self.assertEqual(p.verify(self.root)["name"], "Geometric Blue")
        self.import_()
        self.assertEqual(p.verify(self.root)["name"], "Second")
        p.select_previous(self.root)
        self.assertEqual(p.verify(self.root)["name"], "Geometric Blue")
        p.select_previous(self.root)
        self.assertEqual(p.verify(self.root)["name"], "Second")

    def test_default_restore_and_corrupt_previous_refusal(self):
        self.import_()
        p.select_default(self.root)
        self.assertIsNone(p.verify(self.root))
        p.select_previous(self.root)
        self.assertEqual(p.verify(self.root)["name"], "Geometric Blue")
        source = self.source_data()
        source["name"] = "Second"
        self.put_source(source)
        self.import_()
        current, raw = p.pointer(self.root)
        prior = self.root / current["previous"]["manifest"]
        prior.write_bytes(b"corrupt")
        with self.assertRaisesRegex(p.ThemeError, "manifest hash"):
            p.select_previous(self.root)
        self.assertEqual((self.root / "current.json").read_bytes(), raw)
        self.assertEqual(p.verify(self.root)["name"], "Second")

    def test_default_recovers_malformed_pointer_and_preserves_preimage(self):
        self.import_()
        corrupt = b'{"schema": "broken", bad json'
        (self.root / "current.json").write_bytes(corrupt)
        p.select_default(self.root)
        self.assertIsNone(p.verify(self.root))
        backup = self.root / "recovery" / ("invalid-pointer-" + p.sha(corrupt) + ".json")
        self.assertEqual(backup.read_bytes(), corrupt)

    def test_nul_in_json_key_rejected(self):
        self.import_()
        current = self.root / "current.json"
        current.write_bytes(current.read_bytes().replace(b'"schema":', b'"schema\\u0000ignored":', 1))
        with self.assertRaisesRegex(p.ThemeError, "NUL"):
            p.verify(self.root)

    def test_lock_prevents_competing_writes_and_is_not_removed(self):
        self.import_()
        existing = (self.root / "current.json").read_bytes()
        (self.root / ".theme-lock").write_bytes(b"other-owner")
        with self.assertRaisesRegex(p.ThemeError, "locked"):
            p.select_default(self.root)
        self.assertEqual((self.root / ".theme-lock").read_bytes(), b"other-owner")
        self.assertEqual((self.root / "current.json").read_bytes(), existing)


if __name__ == "__main__":
    unittest.main(verbosity=2)
