# ytps5

**A runtime quality-of-life mod for the official PS5 video app on jailbroken consoles.**

It adds ad filtering, SponsorBlock skipping, and a native settings panel with
support/QR and update notifications. It does **not** contain, modify, or
redistribute the app, app packages, console keys, DRM tools, or any copyrighted
app files. It attaches to the app you already own and patches the running
process in memory.

> **Research and educational project.** Not affiliated with Sony, Google,
> YouTube, SponsorBlock, or any app vendor. No app packages, keys, DRM bypass
> tools, or copyrighted content are distributed. You must already own the app.
> **Tested only on PS5 firmware 8.20 with the clean app `PPSA01652`.** Use at
> your own risk; no warranty. Do not use this with the Y2JB apps
> `PPSA01650`/`PPSA01651`.

## Compatibility

| App | Role | Status |
| --- | --- | --- |
| `PPSA01650` | Y2JB daily driver | 🔴 **Never patch.** The tool refuses. |
| `PPSA01651` | Y2JB daily driver | 🔴 **Never patch.** The tool refuses. |
| `PPSA01652` | Clean player | 🟢 **Recommended.** Tested on FW 8.20. |
| other `PPSA0165x` | Native PS5 app | 🟡 Untested. The tool may allow it, but signatures can differ. |
| `CUSA01116` | PS4-compatibility app | ⚪ Unsupported. |

The injector detects the title it is about to patch. For `PPSA01650`/`PPSA01651`
it shows **“Y2JB app — NOT patching”** and waits. For a clean title it shows
**“clean — patching”**. Nothing writes to app save data, `download0.dat`, or the
Y2JB autoloader.

## What you get

- In-player ad objects removed before the UI renders them.
- Feed and hero sponsored cards filtered out.
- SponsorBlock segments fetched from the public API and skipped, with toasts.
- Native **ytps5 Settings** panel:
  - Ad Block on/off
  - SponsorBlock on/off
  - SponsorBlock category toggles
  - **Support master_shivnath** with a QR code and GitHub Sponsors link
- Update check against `version.json`; a toast appears when a newer release
  exists.
- The payload stays resident and **re-patches every app launch** until the
  console is rebooted.

## Install (single file)

1. Download the latest `ytps5.elf` from the **Releases** page.
2. Copy it to the console or a USB drive.
3. On the console, open **Payload Manager** → local payloads → upload/add
   `ytps5.elf` → run it.
4. Wait for the **“ytps5: waiting for the supported app”** toast.
5. Open the clean app (`PPSA01652`). It will freeze for a moment while the hook
   is installed, then show **“ytps5: hook installed”**.

The single ELF embeds the onboarding proxy and the injector. It only uses
network loopback to hand the real payloads to the console's own `elfldr`, so it
also works when launched directly by Payload Manager.

### Automatic start (optional)

To start everything after each boot/jailbreak, add `ytps5.elf` to your
autoloader:

- Payload Manager: add `ytps5.elf` to `autoload.txt`.
- etaHEN: place it in the payload autoload list.

The installer does **not** modify the Y2JB app or its files.

## One-time run or every boot?

- **Every boot/jailbreak:** the installer must run once, because the patch is
  memory-only and a reboot clears it. With autoload enabled this is automatic.
- **Every app launch:** no. The resident injector waits for the app, patches it,
  waits for it to close, and patches the next launch automatically.
- **After a full console restart:** run `ytps5.elf` again, or rely on autoload.

## Settings and support

Open the app's **Settings** → **ytps5 Settings**. The native panel includes:

- Ad Block and SponsorBlock toggles.
- Individual SponsorBlock category toggles.
- **Support master_shivnath** → QR code and GitHub Sponsors.

Preferences are currently per-session. A native persisted config is on the
roadmap for the next update.

## Updates

`version.json` on the `main` branch is the update source:

```json
{
  "version": "1.0.0",
  "url": "https://github.com/MasterShivnath007/ytps5-sponsorblock/releases/latest",
  "notes": "Initial public release"
}
```

The local proxy fetches it, compares it with the payload's `CURRENT_VERSION`,
and shows a toast if a newer version is available. To publish a release, bump
`version.json` and attach the new `ytps5.elf` to a GitHub Release.

## Troubleshooting

Logs on the console:

- `/data/ytps5-sponsorblock/injector.log`
- `/data/ytps5-sponsorblock/relay.log`
- `/data/ytps5-sponsorblock/proxy_server.log`

Common issues:

- **No toast after running the installer:** confirm `elfldr` is listening on
  `127.0.0.1:9021`; the installer hands the payloads to it.
- **SponsorBlock not skipping:** the local proxy must be running on port `4040`.
  The single installer starts it automatically.
- **App stuck on the launch screen:** reboot, rerun `ytps5.elf`, and open the
  clean app. Do not try to patch a Y2JB app.
- **Update toast never appears:** `version.json` is fetched from `main`; make
  sure it is present and the console has network access.

When opening an issue, attach the three logs above and the exact app title ID
and version.

## Build from source

Requirements:

- PS5 payload SDK (John Törnblom)
- LLVM 18 for `llvm-config`
- Node.js + `npx` (only to minify the JavaScript payload)

```bash
export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk
export LLVM_CONFIG=/opt/homebrew/opt/llvm@18/bin/llvm-config

make minify      # payload/inject.src.js -> payload/inject.js
make installer   # build/ytps5-installer.elf (single-file release)
make dist        # dist/ytps5.elf + README/LICENSE/NOTICES/version.json
```

`payload/inject.src.js` is the readable source; `payload/inject.js` is the
minified file embedded in the injector. The `make` target regenerates the C
header automatically.

## How it works (short version)

1. The installer hands the proxy and injector to the console's `elfldr`.
2. The injector attaches to the clean app process with libhijacker.
3. It installs a 14-byte jump on `HTMLScriptExecute` using a trampoline, and a
   tiny position-independent hook that prepends the JavaScript payload to
   scripts as they execute.
4. The JavaScript filters ad objects from app API responses, manages the video
   element, and talks to the local SponsorBlock proxy.
5. A `scePadRead` GOT trigger is used only as a safe place to apply the
   prologue patch; it never runs library code from disk.

See `docs/HOW_IT_WORKS.md` for details.

## Legal and safety

- This repository contains **no** app packages, console keys, DRM bypass
  tooling, or copyrighted app files.
- It does not modify the installed app package. The patch exists only in the
  running process's memory and disappears when the app or console restarts.
- The project is intended for research, education, and personal use on hardware
  you own and control. Check the laws and terms that apply to you.
- The tool deliberately refuses `PPSA01650` and `PPSA01651` because those are
  commonly used as Y2JB daily drivers. Use a clean app for patching.
- SponsorBlock is an independent community service. This project only calls its
  public API.

## License

GPL-3.0. See `LICENSE` and `THIRD_PARTY_NOTICES.md`.
