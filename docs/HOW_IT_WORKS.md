# How it works

This is a runtime patch for the official PS5 video app. It does not modify the
installed app package, does not use console keys, and does not distribute any
app files.

## Components

- `ytps5.elf` — the single-file installer. It embeds the proxy and injector and
  hands both to the console's own `elfldr` on `127.0.0.1:9021`.
- Proxy (`injector/dump_server.c`) — a small local HTTP service on port `4040`.
  It fetches SponsorBlock data (and `version.json`) over HTTPS.
- Injector (`injector/main.cpp`) — reads `config.txt`, attaches to the selected
  app process, finds `HTMLScriptExecute`, installs a trampoline and hook, and
  stays resident.
- Hook (`injector/direct_hook.c`) — a tiny position-independent blob that
  prepends the JavaScript payload to each script.
- Payload (`payload/inject.src.js`, minified to `payload/inject.js`) — ad
  filtering, SponsorBlock skipping, the settings panel, and update checks.

## Flow

1. The installer sends the proxy and injector to `elfldr`.
2. The injector reads `/data/ytps5-sponsorblock/config.txt` and waits for the
   selected title (default `PPSA01652`; any `PPSA0165x` build works). `PPSA01650`/`PPSA01651` are refused
   unless `allow_y2jb=1` is set.
3. It attaches with libhijacker, finds `HTMLScriptExecute`, and makes the target
   page writable from the kernel side.
4. It installs a `scePadRead` GOT trigger. On the next controller poll, the
   trigger applies the `HTMLScriptExecute` prologue patch from inside the app's
   own input loop, avoiding a race with a live script call.
5. The `HTMLScriptExecute` hook calls a trampoline containing the original
   prologue, then calls the original function with a combined string:

   ```
   <our JavaScript payload>
   <the original script>
   ```

6. The JavaScript runs in the page context and is idempotent. It filters ad
   objects from API responses, watches the video element, fetches SponsorBlock
   segments through the local proxy, and skips matching segments.
7. The injector stays resident. When the app closes, it waits for the next
   launch and patches the new process.

## App selection

On first run the injector writes `/data/ytps5-sponsorblock/config.txt`:

```ini
patch_title=PPSA01652   # PPSA01650 | PPSA01651 | PPSA01652 | AUTO
allow_y2jb=0            # 1 = allow patching PPSA01650/PPSA01651
```

`patch_title` is matched exactly against the running app's title ID. `AUTO`
matches any `PPSA0165x`. The Y2JB daily drivers are refused unless
`allow_y2jb=1`; the default is the clean player. The file is read once at
injector startup, so restart `ytps5.elf` after editing it.

## Settings

The app's settings response is patched lazily, only when the settings screen is
opened. The “Premium” row becomes “ytps5 Settings”; the purchases/memberships
row is removed. The native modal exposes Ad Block, SponsorBlock, category
toggles, and the support QR.

Settings are currently per-session. A native persisted config is planned.

## Update check

The proxy serves `/update` by fetching `version.json` from this repository. The
payload compares it with `CURRENT_VERSION` and shows a toast when a newer
version exists.

## Limits

- Tested on firmware 8.20 and 11.40 with YouTube `01.000.003` (`PPSA01652`). Any `PPSA0165x` build of that version can be patched. The kernel layer follows the payload SDK from 1.00 to 13.60.
- The `HTMLScriptExecute` signature is matched against the tested app build.
  A store update can change it.
- The patch is memory-only and is cleared by a console reboot or app exit. The
  resident injector re-patches app launches; after a reboot, run the installer
  again or use autoload.
