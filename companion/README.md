# Optional RetroPie companion (read-only by default)

This is a standalone Python standard-library program for a Raspberry Pi running RetroPie. It is **not installed or launched on the LG TV**. It exposes `/v1/status` on a private LAN only. A long random bearer token is required. Remote EmulationStation startup remains disabled unless you explicitly configure a user service and `RETROPIE_ALLOW_LAUNCH=1`; the webOS launcher does not invoke that action.

On the Pi, arrange for Python 3 and a trusted private home network, then configure the token and binding locally:

```bash
export RETROPIE_TOKEN="$(python3 -c 'import secrets;print(secrets.token_urlsafe(32))')"
export RETROPIE_BIND=192.168.1.50   # replace with your Pi's trusted LAN IPv4 address
export RETROPIE_PORT=8767
python3 ./retropie_bridge.py
```

Keep the token out of the settings file. For this C++ edition, set the session
`SPICE_RETROPIE_TOKEN` environment variable before launching the native app;
configure `retropie_host`, `retropie_port` and `retropie_hdmi` in the separate
C++ settings.json. Open RetroPie Link and press Refresh. The C++ edition has
no token-entry dialog and never writes that environment variable to disk.

For compatibility with the original webOS web-app `file://` origin, the companion responds to CORS preflight requests **only from `Origin: null`**. Never open the service to the public internet; any local-file web page has a null origin, so the **random bearer token** and LAN isolation are the security boundary. HTTP credentials are not encrypted on the wire. Prefer an isolated home LAN, and never use an untrusted Wi-Fi network or port-forwarding. This code is provided only for an opted-in home system.

The dashboard only checks status (hostname, emulator/system folders, CPU/RAM approximate metrics, EmulationStation running) and separately launches a chosen TV HDMI input. It cannot power on or stream the Pi's screen without additional supported tools.
