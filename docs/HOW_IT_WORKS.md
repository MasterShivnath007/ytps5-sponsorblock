# How it works

## The short version

The PS5 YouTube app is a Cobalt application. Cobalt is a Chromium-derived
browser that runs YouTube's TV web app. The app's executable is a normal
x86-64 module; its UI is JavaScript running inside Cobalt.

We cannot patch the retail PKG because it is encrypted and signed. We can,
however, patch the running process:

1. `ytps5-injector.elf` attaches to the YouTube process with libhijacker.
2. It finds the app's `scePadReadState` import in the executable's PLT/GOT and
   points that GOT slot at a small code blob.
3. The next time the app polls the controller, the blob runs. It calls
   `dlopen("/data/ytps5-sponsorblock/sponsorblock.so")`, resolves
   `ytps5_install`, and calls it.
4. `sponsorblock.so` hooks Cobalt's `HTMLScriptExecute`.
5. The first script in each Cobalt context is replaced with:

   ```
   <our JavaScript payload>
   <the original script>
   ```

   so the app still runs normally.
6. The payload prunes ad objects from `JSON.parse` results and skips
   SponsorBlock segments through the page's `<video>` element.

## Why the GOT route?

`scePadReadState` is imported by the YouTube executable and is called
regularly. Patching its GOT slot is safer than overwriting the function's
prologue:

- The GOT entry is writable data, so no instruction-boundary disassembly is
  needed in the injector.
- The original function is still available at its real address, and our blob
  calls it normally.
- libhijacker's `Game_Plugin_Loader` uses the same technique for its shellcode.

The blob is compiled from `injector/hook.c` with no external references, then
copied into the executable's text allocator. A copy of the structure it needs
is written to the target's data allocator.

## Why the HTMLScriptExecute route?

The PS4 project (`earthonion/yt_adskip_ps4`) showed that Cobalt exposes a
function that receives a script object and an "external" flag. The function is
found by pattern-scanning the executable's text for a sequence from the Cobalt
build:

```
55 48 89 E5 41 57 41 56 41 55 41 54 53 48 81 EC ...
```

The PS5 app may compile Cobalt differently. The injector tries the exact PS4
signature first, then a version with wildcards, then a broader prologue. If none
match, the module's fallback scans `/proc/curproc/map`. The logs record every
candidate and the module it was found in.

## Script object layout

Cobalt passes something like a `std::string`-like object. The module probes the
object for a `{pointer, size, capacity}` triple, learns the field offsets, and
then builds a fresh copy of the header with our combined script. This is
deliberately defensive: it does not assume a fixed layout until it has read a
real object.

## SponsorBlock data

The JS payload calls:

```
https://sponsor.ajay.app/api/skipSegments?videoID=...&categories=[...]
```

The API returns segments. If Cobalt's CSP blocks direct `fetch`, the payload
tries `http://127.0.0.1:4040/<videoID>` (the PS4 plugin's local proxy
convention). The native proxy is not part of this first test build; it can be
added to `sponsorblock.so` later if the direct request is blocked.

## What this project does not do

- It does not decrypt or modify `PPSA01650.pkg`, `PPSA01651.pkg` or
  `PPSA01652.pkg`.
- It does not include or download Sony/Google code, keys or licenses.
- It does not bypass account checks, DRM or PSN.
- It does not modify the installed app on disk; a reboot restores the original
  process state.
