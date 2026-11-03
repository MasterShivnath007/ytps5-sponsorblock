# Prior-art research

Research date: 2026-11-03. Method: GitHub repository and code search, plus
manual review of the projects listed below. General web search was unavailable
from the build environment, so GitHub search was used as the primary index.

## Finding

No public project was found that patches the **official PS5 app at runtime**
with an ELF payload in the way this project does. The closest public work falls
into four groups.

## 1. Same app, different console — PS4

- **[earthonion/yt_adskip_ps4](https://github.com/earthonion/yt_adskip_ps4)**
  (80★, no license file). A GoldHEN PRX that hooks
  `HTMLScriptExecute` in the PS4 Cobalt app and runs a local SponsorBlock proxy.
  This is the closest conceptual prior art. **No code was copied**; the project
  is unlicensed, so only the published behavior/idea was studied.

## 2. Same Cobalt runtime, different platform

- **[reisxd/TizenTube](https://github.com/reisxd/TizenTube)** (2.4k★, GPL-3.0).
  Tizen TV module with adblock, SponsorBlock, settings and a native UI. The
  settings modal/QR renderer shapes in this project are adapted from its
  GPL-3.0 UI helpers.
- **[reisxd/TizenTubeCobalt](https://github.com/reisxd/TizenTubeCobalt)**
  (3.5k★, BSD-3-Clause). Cobalt build for non-Tizen devices; reference for
  packaging and settings behavior. No source files copied.
- **[Exaphis/mutube](https://github.com/Exaphis/mutube)** (594★, no license
  file). Apple TV wrapper that loads a TizenTube Cobalt userscript. Studied for
  packaging behavior; **no code copied**.
- Android TV ecosystem: `TobiPeterG/tizentube-bridge`,
  `ysamjo/morphe-patches`, `EdddySh/TizenTube-Script-Changer`,
  `xD33m/tizentube-rebrander`. Related packaging/rebranding work; no code used.

## 3. Native PS5 homebrew client

- **[MrDerekib/Tube5-PS5](https://github.com/MrDerekib/Tube5-PS5)** (GPL-3.0).
  An independent native PS5 media client with planned SponsorBlock support. It
  is a different architecture (a separate homebrew client, not a runtime patch
  of the official app). No code used; listed as related work.

## 4. PS5 injection foundations

- **[etaHEN-Plugins](https://github.com/etaHEN/etaHEN-Plugins)** (GPL-3.0) —
  libhijacker process attachment, memory access, ELF helpers.
- **[ps5-payload-dev/sdk](https://github.com/ps5-payload-dev/sdk)** (GPL-3.0+) —
  payload compiler and stub libraries.
- **HDE64** (BSD-style) — instruction-length decoding.

## What this means

- The project targets the official PS5 app in memory, rather than shipping a
  modified app or a separate client.
- It does not contain app packages, keys, DRM tools, or copyrighted app files.
- Prior art is credited in `CREDITS.md`. If earlier work exists, open an issue
  so it can be added.
