# Player profiles: standalone demonstration

Open `index.html` directly from disk. It is self-contained: there is no server,
build step, external asset, sign-in, or network request on page load. The
starting Xbox handle is owner-supplied and unverified. PlayStation stays in
setup-needed until a confirmed online ID is entered. The separately marked
sample values are invented; they do not describe either account.

**Import local JSON** loads an exported snapshot; **Export snapshot** saves the
current view. Handle changes apply to this view until it is exported. Changing
a handle discards that platform's previous details so they cannot be attributed
to another person. The default snapshot in `profile-snapshot.json` records the
anonymous check performed on November 4, 2026 (Toronto; November 5 UTC).
Loaded-file provenance is reported as provided by the file.

To make a fresh anonymous public request, from the project directory:

```powershell
.\scripts\Prepare-Profiles.ps1 -Platform xbox -Handle 'pikldspratmn' -Refresh
```

Then import `demos/profiles/profile-snapshot.json` in the page. For PlayStation,
replace the platform and supply the confirmed online ID. The CLI does not read
credentials and follows only allowlisted profile URLs for that same handle. It
does not sign in or download avatars. It accepts a conservative subset of
possible handles; a rejection does not prove a platform considers one invalid.
The experimental HTML parser recognizes only a JSON-LD Person with an exact
matching `alternateName`; it does not inspect or run a site's client scripts.
An unrecognized public page, access denial, network error, or absent JSON-LD
profile is not proof that an account does not exist or that its privacy settings
have any particular value. The PSN connector has not been tested with an actual
online ID.

The optional Python interface also accepts `--import-json path` and `--out path`.
An invalid existing output is never overwritten. On an unsuccessful refresh,
last-good fields and their timestamps remain and the profile is marked stale;
the new attempt's result and time are recorded separately. Writes use a flushed
temporary file and atomic replacement in the output directory.

## Local JSON contract

The file is UTF-8 JSON, at most 512 KiB, with `schema: "spice.profiles/1"`,
`generated_at` (zoned ISO time with seconds and optional fractional seconds,
or null), and `profiles` containing
`xbox` and/or `psn`. Each profile has `state`, `fields`, and `last_attempt` (or
null). Supported states are `setup_needed`, `untested`, `unavailable`,
`auth_needed`, `error`, `stale`, `public_observed`, `imported`, and `sample`.

Each field is `{ "value": ..., "source": ..., "observed_at": ... }`.
Sources are `provided`, `public_observed`, `imported`, and `sample`;
`observed_at` is a zoned timestamp or null. Recognized fields are:

- `handle` and `display_name`: strings. Handles are validated per platform.
- `gamerscore`, `achievements`, and `trophy_level`: nonnegative integers.
- `trophies`: an object with any of `platinum`, `gold`, `silver`, `bronze` as
  nonnegative integers. A total with missing categories is labelled partial.
- `recent_games`: at most 20 game-title strings.
- `avatar`: an embedded still PNG/JPEG/WebP data URL, at most 96 KiB after
  base64 decoding, with each declared side at most 1024 pixels. Header/container
  admission occurs before the browser sees it; this is not full codec validation.
  PNG/WebP animations and compressed PNG metadata are rejected. A bounded image
  that the browser cannot decode falls back to platform initials. Remote image
  URLs and SVG are rejected; imported images are never fetched from a network.

Unknown keys are dropped during normalization. Invalid known fields reject the
whole import; imported text is rendered as text, not HTML. `last_attempt`, when
present, has `at`, `result`, and `detail`. Nothing derives trophies, games,
presence, or achievements from a handle.

```json
{
  "schema": "spice.profiles/1",
  "generated_at": null,
  "profiles": {
    "xbox": {
      "state": "sample",
      "fields": {
        "gamerscore": {"value": 18250, "source": "sample", "observed_at": null},
        "recent_games": {"value": ["Example racer"], "source": "sample", "observed_at": null}
      },
      "last_attempt": null
    }
  }
}
```

## Checks

```powershell
$env:PYTHONDONTWRITEBYTECODE = '1'
py -3 -m unittest discover -s tests -p 'test_prepare_profiles.py' -v
node demos/profiles/test-dashboard.cjs
```

The browser check requires an installed Microsoft Edge and uses an isolated
temporary browser profile. It loads a temporary copy of the standalone file;
it does not host the demo or use an account.
