# ytps5-sponsorblock

Ad blocking and SponsorBlock for the **official YouTube app on a jailbroken PS5**.

This is a runtime mod. It does **not** contain, modify, or redistribute a YouTube
PKG. It attaches to the YouTube app you already have installed and injects a
JavaScript payload into Cobalt, the browser engine YouTube uses on PS5.

> **Status:** first hardware test pending. The code builds, but the Cobalt hook
> signature may need adjustment for your exact YouTube version. If the app does
> not get the mod, run the log command below and open an issue with the two log
> files.

## What you get

- Removes pre-roll, mid-roll and other ad objects from YouTube API responses
  before the UI renders them.
- Fetches SponsorBlock segments and skips sponsor, intro, outro, interaction,
  self-promotion, preview, filler and non-music segments.
- Shows YouTube TV-style toast notifications when segments are found/skipped.
- Keeps the official YouTube UI, remote, login and subscriptions.
- No modified `PPSA01650/51/52` PKG, no Sony keys, no copyrighted app files.

## Why not a patched YouTube PKG?

The retail YouTube PKGs are AES-XTS encrypted and Sony-signed. Extracting or
rebuilding one requires console keys, and the app loads its main UI from
YouTube at runtime. A "merge a JS file into the PKG" tool cannot work on the
retail package. The working equivalent is a runtime hook, which is what this
project does.

## Requirements

- A jailbroken PS5 with **etaHEN** (or another environment that provides FTP and
  an ELF loader).
- The official YouTube app installed (`PPSA01650`, `PPSA01651` or `PPSA01652`).
- A PC on the same network with Python 3.9+.
- FTP on port `2121` and elfldr on port `9021` (etaHEN defaults).

## Which YouTube app?

Supported: the **native PS5** apps `PPSA01650` (US), `PPSA01651` (EU) and
`PPSA01652` (JP). All three are Cobalt builds that load the UI from
`youtube.com/tv`, which is what the hook targets.

Not supported: `CUSA01116`. That is the **PS4** YouTube app running through
PS4 backwards compatibility. It is a different binary with a different
renderer and module layout, so the injector ignores it (it only matches
`PPSA0165*`). Use one of the native PS5 apps instead.

The payload only reads and patches the running process in memory. It never
writes to `/user/download/PPSA0165*`, so a Y2JB `download0.dat` setup is not
modified.

## Keep YouTube from updating

The hook signatures are matched against YouTube `01.000.003`. A store update
can change them and break injection until the signatures are refreshed. If you
use Y2JB, consider
[y2jb-update-blocker](https://github.com/Epinorr/y2jb-update-blocker) to keep
the installed YouTube build pinned. This is optional; the payload does not
depend on it.

## Quick start (prebuilt)

Download the latest release ZIP. It contains:

```
ytps5-injector.elf
sponsorblock.so
inject.js
tools/ytps5.py
```

Then, with YouTube installed:

1. Start the PS5 and let etaHEN finish loading.
2. Run:

   ```bash
   python3 tools/ytps5.py send --host <PS5_IP> --wait 30
   ```

   The helper uploads the module and JS to
   `/data/ytps5-sponsorblock/`, then sends the ELF to elfldr.
3. Launch YouTube if it is not already open. The injector waits for
   `PPSA0165*`, attaches, and loads the module.
4. You should see a "Ad Block + SponsorBlock Enabled!" toast. Open a video with
   sponsor segments to test skipping.

If the injector was sent while YouTube was already open, it may suspend the app
for a few seconds while it patches it. That is expected.

## Logs

```bash
python3 tools/ytps5.py logs --host <PS5_IP> --save
```

This reads:

- `/data/ytps5-sponsorblock/injector.log` — attach/patch status
- `/data/ytps5-sponsorblock/ytps5.log` — module/Cobalt hook status

The logs contain the title ID, addresses and hook results; no account data.
Please attach both files to an issue if the mod does not appear. Use
`clean-logs` before a new test.

The payload never writes to an app's save data. In particular it does not
touch `/user/download/PPSA01650` or `/user/download/PPSA01651`, so a Y2JB
`download0.dat` setup is left alone.

## Build from source

You need:

- [PS5 payload SDK](https://github.com/ps5-payload-dev/sdk) (or the
  `ps5-payload-dev.tar.gz` from
  [pacbrew-repo](https://github.com/ps5-payload-dev/pacbrew-repo/releases)).
- LLVM 18 (`llvm-config` must be on `PATH`; on macOS:
  `brew install llvm@18`).
- The etaHEN headers and `libhijacker.a` are vendored under `third_party/etahen`
  (GPL-3.0, see `THIRD_PARTY_NOTICES.md`).

```bash
export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk
export LLVM_CONFIG=/opt/homebrew/opt/llvm@18/bin/llvm-config

make
```

Outputs:

```
build/ytps5-injector.elf   # standalone payload
build/sponsorblock.so      # in-process module
```

Then use the helper:

```bash
python3 tools/ytps5.py send --host <PS5_IP> --wait 30
```

### Build with a different etaHEN checkout

```bash
make ETAHEN_DIR=/path/to/etaHEN-Plugins
```

## How it works

```
ytps5-injector.elf
  ├─ waits for PPSA0165*
  ├─ attaches with libhijacker
  ├─ points the app's scePadReadState GOT slot at a 98-byte raw hook
  └─ the raw hook dlopen()s sponsorblock.so inside the app

sponsorblock.so
  ├─ finds HTMLScriptExecute
  ├─ hooks it with a 14-byte absolute jump
  └─ prepends inject.js to the first script of each Cobalt context

inject.js
  ├─ prunes ad objects in JSON.parse
  ├─ fetches SponsorBlock segments
  └─ skips them via the <video> element
```

The ELF is only a loader. It does not stay resident after it returns; the GOT
patch and the loaded `.so` remain in the YouTube process.

## Limitations

- Cobalt internals change between YouTube versions. If the hook signature stops
  matching, the logs will show `HTMLScriptExecute not found`. Send the log and
  the installed YouTube version; the signature can be updated.
- The first version tries a direct `fetch()` to `sponsor.ajay.app`. If
  YouTube's CSP blocks it, the JS falls back to a local proxy on port 4040; the
  native proxy is not implemented in this first test build. The log will show
  `direct SponsorBlock fetch failed` / `proxy unavailable`.
- The mod is loaded after YouTube starts. If you restart YouTube, run the
  injector again (or add the ELF to your autoloader).
- Only the official YouTube app is supported. Netflix, Disney+, etc. are not.
- No account or DRM bypass is involved.

## Legal

This is an independent, unofficial project. It is not affiliated with Google,
YouTube, Sony or PlayStation. "YouTube", "PlayStation" and "PS5" are trademarks
of their respective owners.

The repository contains no YouTube code, no Sony keys and no PKGs. You must
install YouTube yourself through normal means. Modifying a running app may
violate YouTube's terms of service; use it on your own console at your own risk.
The project is licensed under GPL-3.0.

## Credits

- [earthonion/yt_adskip_ps4](https://github.com/earthonion/yt_adskip_ps4) — the
  Cobalt `HTMLScriptExecute` injection and SponsorBlock approach (MIT).
- [TizenTube](https://github.com/reisxd/TizenTube) — SponsorBlock/adblock
  behavior and UI inspiration (GPL-3.0).
- [SponsorBlock](https://sponsor.ajay.app) — public segment API.
- [etaHEN / etaHEN-Plugins](https://github.com/etaHEN/etaHEN-Plugins) —
  libhijacker and plugin tooling (GPL-3.0).
- [ps5-payload-dev/sdk](https://github.com/ps5-payload-dev/sdk) — PS5 payload
  SDK (GPL-3.0).
- [Hacker Disassembler Engine](https://github.com/zyantific/hde64) — x86-64
  prologue length calculation (BSD-style).
