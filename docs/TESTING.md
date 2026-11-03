# Testing

Test target: **PS5 firmware 8.20**, Y2JB + Relapse, etaHEN with FTP on `2121`
and `elfldr` on `9021`, clean app **`PPSA01652`**.

Do **not** test on `PPSA01650`/`PPSA01651`; those are Y2JB daily drivers and the
injector deliberately refuses them.

## Build

```bash
export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk
export LLVM_CONFIG=/opt/homebrew/opt/llvm@18/bin/llvm-config
make minify
make installer
```

`build/ytps5-installer.elf` is the single file to upload.

## Install

1. Upload `ytps5-installer.elf` in Payload Manager (or place it in the local
   payloads folder) and run it.
2. Confirm the toast **“ytps5: waiting for the supported app”**.
3. Open `PPSA01652`.
4. Expect a short freeze, then **“ytps5: hook installed”** and
   **“N script(s) injected”**.

## Checklist

- [ ] App launches to the home screen.
- [ ] No in-player ads play.
- [ ] Feed/hero sponsored cards are gone.
- [ ] Settings → **ytps5 Settings** opens.
- [ ] Ad Block / SponsorBlock / category toggles work.
- [ ] **Support master_shivnath** opens the QR panel.
- [ ] SponsorBlock finds and skips a known segment.
- [ ] Close and reopen the app; it is patched automatically without rerunning
      the installer.
- [ ] Run the installer on a clean boot; it starts the proxy and injector.

## Logs

```bash
python3 tools/ytps5.py logs --host <PS5_IP> --save
```

Or read from the console:

- `/data/ytps5-sponsorblock/relay.log`
- `/data/ytps5-sponsorblock/injector.log`
- `/data/ytps5-sponsorblock/proxy_server.log`

Expected injector stages include `app suspended`, `kernel_mprotect`, `installed
target`, `resumed`, and `calls=... injected=...`.

## Failure notes

- **Stuck launch screen:** reboot and rerun the installer. Verify the app is the
  tested build and that you did not try to patch a Y2JB app.
- **No SponsorBlock:** check that the proxy is listening (`curl
  http://<PS5_IP>:4040/ping` from the same network should return `[]`).
- **No update toast:** confirm `version.json` exists on `main`.
