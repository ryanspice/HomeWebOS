"""Offline preparation boundaries; no TV, service call or provider request."""
import copy
import io
import json
import pathlib
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
import prepare_tv_data as p
from PIL import Image

class FakeTv:
    def __init__(self):
        self.calls = []
        self.rows = {
            "com.example.one": dict(id="com.example.one", title="One", version="1", appDescription="Packaged app description", icon="icon.png"),
            "com.example.two": dict(id="com.example.two", title="Two", version="2", icon="icon.png"),
        }
        image = io.BytesIO()
        Image.new("RGBA", (16, 16), (20, 40, 60, 128)).save(image, format="PNG")
        self.image = image.getvalue()

    def request(self, op, **fields):
        self.calls.append((op, fields))
        if op == "device": return dict(modelName="fixture only", serialNumber="must not be collected")
        if op == "launchpoints": return dict(launchPoints=list(self.rows.values()))
        if op == "apps": return dict(apps=list(self.rows.values()))
        if op == "appdirs": return dict(paths=[p.APP_ROOTS[0] + key + "/appinfo.json" for key in self.rows])
        if op == "appinfo": return dict(appInfo=self.rows.get(fields["app_id"], {}))
        if op == "status": return dict(exist=fields["app_id"] in self.rows)
        raise AssertionError("unexpected operation")

    def read(self, path):
        self.calls.append(("file", dict(path=path)))
        for key, row in self.rows.items():
            if path == p.APP_ROOTS[0] + key + "/appinfo.json": return p.encoded(row)
            if path == p.APP_ROOTS[0] + key + "/icon.png": return self.image
        return None

class PreparedDataTests(unittest.TestCase):
    def setUp(self):
        self.base = pathlib.Path(__file__).resolve().parents[1] / "build" / "preparation-test-fixtures"
        self.base.mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(prefix="case-", dir=self.base)
        self.root = pathlib.Path(self.temp.name)
        self.assertTrue(self.root.resolve().is_relative_to(self.base.resolve()))

    def tearDown(self):
        # Validate the resolved absolute path immediately before recursive cleanup.
        self.assertTrue(self.root.resolve().is_relative_to(self.base.resolve()))
        self.temp.cleanup()

    def bundle(self):
        root = self.root / "bundle"
        with p.bundle_lock(root):
            data, assets = p.collect_data(FakeTv(), "fixture-tv")
            return root, p.commit(root, data, assets, None)

    def test_prepared_images_preserve_icon_alpha_and_flatten_background(self):
        blob = FakeTv().image
        with Image.open(io.BytesIO(p.prepare_image(blob))) as icon:
            self.assertEqual(icon.size, (256, 256))
            self.assertEqual(icon.getpixel((128, 128))[3], 128)
        with Image.open(io.BytesIO(p.prepare_image(blob, True))) as hero:
            self.assertEqual(hero.mode, "RGB")
            self.assertEqual(hero.size, (1600, 900))

    def test_round_trip_and_no_unique_device_identifiers(self):
        root, result = self.bundle()
        self.assertEqual(p.verify_bundle(root)[0], result[0])
        self.assertEqual(result[0]["device"], {"modelName": "fixture only"})
        self.assertEqual(len(result[0]["apps"]), 2)
        self.assertTrue(all(app["inventory_confirmed"] for app in result[0]["apps"]))

    def test_single_app_refresh_preserves_other_app_and_avoids_full_queries(self):
        root, prior = self.bundle()
        original_pointer = (root / "current.json").read_bytes()
        old_two = copy.deepcopy(prior[0]["apps"][1])
        old_art = prior[1]["covers/prepared/com.example.two"]
        tv = FakeTv()
        tv.rows["com.example.one"]["version"] = "3"
        with p.bundle_lock(root):
            data, assets = p.collect_data(tv, "fixture-tv", prior, "com.example.one")
            result = p.commit(root, data, assets, original_pointer, {"covers/prepared/com.example.two"})
        self.assertEqual(result[0]["apps"][1], old_two)
        self.assertEqual(result[1]["covers/prepared/com.example.two"], old_art)
        self.assertEqual(result[0]["apps"][0]["version"], "3")
        self.assertFalse({op for op, _ in tv.calls} & {"device", "apps", "launchpoints", "appdirs"})
        self.assertEqual(len(list((root / "snapshots").iterdir())), 2)

    def test_missing_single_app_retains_pointer(self):
        root, prior = self.bundle()
        pointer = (root / "current.json").read_bytes()
        with self.assertRaises(p.PreparationError):
            p.collect_data(FakeTv(), "fixture-tv", prior, "com.example.missing")
        self.assertEqual((root / "current.json").read_bytes(), pointer)

    def test_corrupt_asset_fails_hash_verification(self):
        root, result = self.bundle()
        folder = (root / result[3]["manifest"]).parent
        (folder / result[2]["assets"][0]["path"]).write_bytes(b"corrupt")
        with self.assertRaises(p.PreparationError): p.verify_bundle(root)

    def test_corrupt_metadata_fails_hash_verification(self):
        root, result = self.bundle()
        (root / result[3]["manifest"]).with_name("data.json").write_bytes(b"{}")
        with self.assertRaises(p.PreparationError): p.verify_bundle(root)

    def test_pointer_traversal_fails(self):
        root, _ = self.bundle()
        (root / "current.json").write_bytes(p.encoded(dict(schema=p.POINTER_SCHEMA, manifest="../outside/manifest.json", sha256="0" * 64)))
        with self.assertRaises(p.PreparationError): p.verify_bundle(root)

    def test_imported_inventory_is_never_claimed_observed(self):
        source = self.root / "import.json"
        source.write_bytes(p.encoded(dict(apps=[dict(id="com.example.import", title="Owner import", inventory_confirmed=True)])))
        data, assets = p.import_data(source)
        self.assertFalse(data["apps"][0]["inventory_confirmed"])
        self.assertEqual(data["source"]["kind"], "import")
        self.assertEqual(assets, {})

    def test_local_import_cannot_escape_or_download_a_url(self):
        with self.assertRaises(p.PreparationError): p.local_art(self.root, "../outside.png")
        with self.assertRaises(p.PreparationError): p.local_art(self.root, "https://example.com/icon.png")
        with self.assertRaises(p.PreparationError): p.remote_art_path(p.APP_ROOTS[0], "../secrets.json")

    def test_duplicate_id_cannot_replace_last_good_pointer(self):
        root, prior = self.bundle()
        pointer = (root / "current.json").read_bytes()
        data = copy.deepcopy(prior[0])
        data["apps"].append(data["apps"][0])
        with p.bundle_lock(root):
            with self.assertRaises(p.PreparationError): p.commit(root, data, prior[1], pointer)
        self.assertEqual((root / "current.json").read_bytes(), pointer)

    def test_concurrent_pointer_change_blocks_publication(self):
        root, prior = self.bundle()
        with self.assertRaises(p.PreparationError): p.commit(root, prior[0], prior[1], b"stale pointer")
        self.assertEqual(p.verify_bundle(root)[3], prior[3])

    def test_preinstall_stage_is_verified_and_refuses_overlay(self):
        root, prior = self.bundle()
        output = self.root / "new-stage"
        with mock.patch.object(p.subprocess, "Popen", side_effect=AssertionError("offline stage opened a process")):
            staged = p.stage_bundle(root, output)
        self.assertEqual(staged[0], prior[0])
        with self.assertRaises(p.PreparationError): p.stage_bundle(root, output)

    def test_initial_lock_is_exclusive_and_retained_data_not_deleted(self):
        root = self.root / "lock-test"
        with p.bundle_lock(root):
            with self.assertRaises(p.PreparationError):
                with p.bundle_lock(root): pass

    def test_existing_graphics_survive_single_app_admission_at_capacity(self):
        root = self.root / "capacity-bundle"
        blob = FakeTv().image
        now = "2026-11-05T00:00:00Z"
        apps = [p.app_metadata(dict(id="com.example.old" + str(n), title="Existing"), now, False, "fixture") for n in range(p.MAX_ASSETS)]
        assets = {}
        for app in apps:
            app["icon_key"] = "covers/prepared/" + app["id"]
            assets[app["icon_key"]] = blob
        data = dict(schema=p.BUNDLE_SCHEMA, captured_at=now, source=dict(kind="tv"), device={}, apps=apps)
        with p.bundle_lock(root):
            prior = p.commit(root, data, assets, None)
        pointer = (root / "current.json").read_bytes()
        new = p.app_metadata(dict(id="com.aaa.new", title="New"), now, False, "fixture")
        new["icon_key"] = "covers/prepared/com.aaa.new"
        updated = copy.deepcopy(prior[0])
        updated["apps"].append(new)
        with p.bundle_lock(root):
            result = p.commit(root, updated, {**assets, new["icon_key"]: blob}, pointer, set(assets))
        self.assertEqual(set(result[1]), set(assets))
        self.assertEqual(result[0]["apps"][:-1], prior[0]["apps"])
        self.assertEqual(result[0]["apps"][-1]["icon_key"], "")

if __name__ == "__main__":
    unittest.main()
