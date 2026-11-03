# Credits and prior art

`ytps5` is maintained by **master_shivnath**. It is a runtime mod for the
official PS5 video app. It does not contain the app, app packages, console keys,
DRM tools, or copyrighted app files.

## Runtime and build foundations

| Project | Author | License | What we use |
| --- | --- | --- | --- |
| [etaHEN-Plugins](https://github.com/etaHEN/etaHEN-Plugins) / libhijacker | etaHEN contributors | GPL-3.0 | Process attachment, memory access, and the vendored headers/archive/source under `third_party/etahen`. Pinned commit `6339554e4e3c92c0a655a37194c1152d6b82ad88`. |
| [PS5 payload SDK](https://github.com/ps5-payload-dev/sdk) | John Törnblom and contributors | GPL-3.0+ | Compiler wrappers, headers, linker scripts, stub libraries. |
| [Hacker Disassembler Engine 64](https://github.com/zyantific/hde64) | Vyacheslav Patkov | BSD-style | Instruction-boundary decoding for the `HTMLScriptExecute` prologue. |

## Prior art and inspiration

These projects were studied for behavior, UX, and platform details. Where a
project has **no license**, no code was copied; it is cited as a conceptual
reference only.

| Project | Author | License | Relationship |
| --- | --- | --- | --- |
| [yt_adskip_ps4](https://github.com/earthonion/yt_adskip_ps4) | earthonion | No license file | PS4 Cobalt ad-skip plugin. The `HTMLScriptExecute` hook and local SponsorBlock-proxy ideas are prior art. **No code copied.** |
| [TizenTube](https://github.com/reisxd/TizenTube) | reisxd and contributors | GPL-3.0 | Tizen TV adblock/SponsorBlock and native settings UX. The settings modal/QR renderer shapes in `payload/inject.src.js` are adapted from TizenTube's GPL-3.0 UI helpers; this project is also GPL-3.0. |
| [TizenTubeCobalt](https://github.com/reisxd/TizenTubeCobalt) | reisxd and contributors | BSD-3-Clause | Cobalt-based adblock/SponsorBlock packaging and settings behavior. Reference only; no source files copied. |
| [MuTube](https://github.com/Exaphis/mutube) | Exaphis | No license file | Apple TV wrapper that loads the TizenTube Cobalt userscript. Studied for packaging behavior. **No code copied.** |
| [Tube5-PS5](https://github.com/MrDerekib/Tube5-PS5) | MrDerekib | GPL-3.0 | Independent native PS5 homebrew client with planned SponsorBlock support. No code used; listed as related prior art. |
| [tizentube-bridge](https://github.com/TobiPeterG/tizentube-bridge) | TobiPeterG | See upstream | Android TV package bridge in the same ecosystem. No code used. |
| [morphe-patches](https://github.com/ysamjo/morphe-patches) | ysamjo | See upstream | Android TV patch-set in the same ecosystem. No code used. |
| [TizenTube-Script-Changer](https://github.com/EdddySh/TizenTube-Script-Changer) | EdddySh | See upstream | APK modification automation in the same ecosystem. No code used. |
| [tizentube-rebrander](https://github.com/xD33m/tizentube-rebrander) | xD33m | See upstream | Android TV client rebranding in the same ecosystem. No code used. |

## PS5 jailbreak ecosystem (not used as code)

These projects are part of the Y2JB / PS5 payload ecosystem. They are credited
for context; no code from them is included.

- [Gezine/Y2JB](https://github.com/Gezine/Y2JB)
- [ayassins/autoY2JB](https://github.com/ayassins/autoY2JB)
- [matem6/P2JB-Y2JB-Porting](https://github.com/matem6/P2JB-Y2JB-Porting)
- [Epinorr/y2jb-update-blocker](https://github.com/Epinorr/y2jb-update-blocker)
- [itsPLK/ps5-y2jb-autoloader](https://github.com/itsPLK/ps5-y2jb-autoloader)
- [itsPLK/ps5-payload-manager](https://github.com/itsPLK/ps5-payload-manager)

## Services and libraries

- **SponsorBlock** — <https://sponsor.ajay.app>. The public segment API is used
  at runtime. SponsorBlock is an independent community service; this project is
  not affiliated with it.
- **curl**, **OpenSSL**, **libpsl**, **zlib**, **zstd** — statically linked into
  the local proxy and installer. Full license texts are in `LICENSES/`.
- **Terser** — used only to minify the JavaScript payload at build time. Not
  distributed.

## Research note

At the time of the 1.0.1 release, no public project was found that patches the
**official PS5 app at runtime** with an ELF payload. The closest public work is
the PS4 Cobalt plugin, the Tizen/Android TV TizenTube family, and the Apple TV
MuTube wrapper. If you know of earlier work, please open an issue so it can be
credited.

## Corrections

If you believe your project or code is credited incorrectly, or that code was
used without permission, open an issue with the details. We will correct the
credit or remove the material promptly.
