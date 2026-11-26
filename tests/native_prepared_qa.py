#!/usr/bin/env python3
"""Exercise local prepared data in the actual desktop binary; no TV connection."""
import argparse
import hashlib
import json
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
import prepare_tv_data as prep

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", type=pathlib.Path, required=True)
    parser.add_argument("--assets", type=pathlib.Path, required=True)
    parser.add_argument("--bundle", type=pathlib.Path, required=True)
    parser.add_argument("--out", type=pathlib.Path, required=True)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    data, images, _, _ = prep.verify_bundle(args.bundle)
    expected_pointer = hashlib.sha256((args.bundle / "current.json").read_bytes()).hexdigest()
    records = []
    env = dict(os.environ, SPICE_CPP_PRIVATE_LUNA="0", SPICE_RETROPIE_TOKEN="")
    # Keep artifact-bearing fixtures and private TV metadata in this explicit output.
    fixture = args.out / "fixtures"
    fixture.mkdir(exist_ok=False)

    def run(name, view="home", flags=(), bundle=None, assets=None, config=None, actions=(), width=3840, height=2160, expect_loaded=True, explicit_apps=False):
        report = args.out / (name + ".json")
        script = fixture / (name + ".input.json")
        script.write_text(json.dumps(list(actions)), encoding="utf-8")
        command = [str(args.binary.resolve()), "--assets", str((assets or args.assets).resolve()), "--config", str(config or fixture / (name + "-settings.json")),
                   "--no-vsync", "--fps", "240", "--width", str(width), "--height", str(height), "--frames", "48", "--step-ms", str(1000 / 120),
                   "--view", view, "--script", str(script), "--report", str(report), "--screenshot", str(args.out / (name + ".png")), *flags]
        if bundle is not None:
            command += ["--prepared-data", str(bundle.resolve())]
        result = subprocess.run(command, env=env, capture_output=True, text=True, timeout=90)
        (args.out / (name + ".log")).write_text(result.stdout + result.stderr, encoding="utf-8")
        assert result.returncode == 0, name + ": native process failed"
        value = json.loads(report.read_text())
        assert value["prepared_data"]["loaded"] is expect_loaded
        assert not value["tv_verified"] and not value["automatic_live_metadata_refresh"]
        assert value["drawable_width"] == width and value["drawable_height"] == height
        assert all(value["service_requests_posted"][key] == 0 for key in ("device", "inputs", "settings", "retropie", "probe", "launch")), name + ": implicit metadata/service request"
        assert value["service_requests_posted"]["apps"] == (1 if explicit_apps else 0), name + ": app request boundary"
        if expect_loaded:
            assert value["prepared_data"]["apps"] == len(data["apps"]) and value["prepared_data"]["graphics"] == len(images)
            assert len(value["prepared_apps"]) == len(data["apps"])
            assert value["prepared_data"]["source_kind"] == data["source"]["kind"]
            assert value["tv_information_source"] == ("prepared-snapshot" if data["source"]["kind"] == "tv" else "unavailable")
        records.append(dict(name=name, passed=True, texture_bytes=value["texture_bytes"], service_requests_posted=value["service_requests_posted"]))
        print("PASS " + name, flush=True)
        return value

    for batch in (False, True):
        flags = ["--batch-renderer"] if batch else []
        mode = "batch" if batch else "default"
        for view in ("home", "library", "diagnostics"):
            run(mode + "-" + view + "-2160", view=view, flags=flags, bundle=args.bundle)
    run("pointer-browse-720", bundle=args.bundle, width=1280, height=720,
        actions=[dict(frame=2, action="hover", x=260, y=135), dict(frame=8, action="right"), dict(frame=16, action="left")])
    run("explicit-refresh-request", view="library", bundle=args.bundle, explicit_apps=True,
        actions=[dict(frame=36, action="click", x=2200, y=1730)])
    local = fixture / "config-source"
    local.mkdir()
    prep.stage_bundle(args.bundle, local / "prepared")
    run("auto-config-bundle", config=local / "settings.json")
    package = fixture / "packaged-assets"
    shutil.copytree(args.assets, package)
    prep.stage_bundle(args.bundle, package / "prepared")
    run("auto-packaged-bundle", assets=package)
    bad = fixture / "invalid-source"
    bad.mkdir()
    _, _, _, pointer = prep.stage_bundle(args.bundle, bad / "prepared")
    (bad / "prepared" / pointer["manifest"]).with_name("data.json").write_bytes(b"corrupt metadata")
    invalid = run("invalid-auto-fallback", config=bad / "settings.json", expect_loaded=False)
    assert invalid["prepared_data"]["error"]
    result = subprocess.run([str(args.binary.resolve()), "--assets", str(args.assets.resolve()), "--prepared-data", str((bad / "prepared").resolve()), "--frames", "1"],
                            env=env, capture_output=True, text=True, timeout=15)
    assert result.returncode != 0 and "hash mismatch" in result.stderr
    records.append(dict(name="invalid-explicit-fails-before-interaction", passed=True))
    assert hashlib.sha256((args.bundle / "current.json").read_bytes()).hexdigest() == expected_pointer, "Runtime changed the prepared pointer"
    output = dict(scope="Native desktop prepared-data QA using locally collected TV metadata; no TV app execution or GPU/physical-presentation proof",
                  passed=len(records), failed=0, tests=records, bundle_pointer_sha256=expected_pointer, runtime_changed_bundle=False,
                  tv_data_source=data["source"]["kind"], tv_app_installed=False)
    (args.out / "prepared-native-qa.json").write_text(json.dumps(output, indent=2) + "\n", encoding="utf-8")
    print("RESULT " + str(len(records)) + " passed / 0 failed", flush=True)

if __name__ == "__main__":
    main()
