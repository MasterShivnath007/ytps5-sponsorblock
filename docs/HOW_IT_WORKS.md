# How it works

This is a runtime patch for the official PS5 video app. It does not modify the
installed app package, does not use console keys, and does not distribute any
app files.

## Components

- `ytps5.elf` — the single-file installer. It embeds the proxy and injector and
  hands both to the console's own `elfldr` on `127.0.0.1:9021`.
- Proxy (`injector/dump_server.c`) — a small local HTTP service on port `4040`.
  It fetches SponsorBlock data (and `version.json`) over HTTPS.
- Injector (`injector/main.cpp`) — attaches to the clean app process, finds
  `HTMLScriptExecute`, installs a trampoline and hook, and stays resident.
- Hook (`injector/direct_hook.c`) — a tiny position-independent blob that
  prepends the JavaScript payload to each script.
- Payload (`payload/inject.src.js`, minified to `payload/inject.js`) — ad
  filtering, SponsorBlock skipping, the settings panel, and update checks.

## Flow

1. The installer sends the proxy and injector to `elfldr`.
2. The injector waits for a `PPSA0165x` app. It refuses `PPSA01650`/`PPSA01651`
   (Y2JB daily drivers) and waits for a clean title.
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

- Tested only on firmware 8.20 with `PPSA01652`.
- The `HTMLScriptExecute` signature is matched against the tested app build.
  A store update can change it.
- The patch is memory-only and is cleared by a console reboot or app exit. The
  resident injector re-patches app launches; after a reboot, run the installer
  again or use autoload.
