# Third-party notices

This project is licensed under **GPL-3.0**. It includes or links the following
third-party components. Their licenses apply to their respective files.

## etaHEN / etaHEN-Plugins

- Vendored: `third_party/etahen/include`, `third_party/etahen/libhijacker/source`,
  `third_party/etahen/libhijacker/libhijacker.a`.
- Upstream: <https://github.com/etaHEN/etaHEN-Plugins>
- Pinned upstream commit: `6339554e4e3c92c0a655a37194c1152d6b82ad88`
- License: GPL-3.0 (see `third_party/etahen/LICENSE`).

The `libhijacker.a` archive is built from the vendored `libhijacker/source`
with the PS5 payload SDK. It is included for reproducible builds; you can rebuild
it from that source or point `ETAHEN_DIR` at an external checkout.

## PS5 payload SDK

- Not vendored; install it separately.
- Upstream: <https://github.com/ps5-payload-dev/sdk>
- License: GPL-3.0+.

The build uses its compiler wrappers, headers, linker scripts, and stub
libraries.

## Hacker Disassembler Engine 64 (HDE64)

- Vendored: `module/hde64.cpp`, `module/hde64.h`, `module/table64.h`.
- Copyright (c) 2008-2009 Vyacheslav Patkov.
- BSD-style license. Used to calculate instruction boundaries.

## References and prior art

- `earthonion/yt_adskip_ps4` — <https://github.com/earthonion/yt_adskip_ps4>
  (MIT). Reference for runtime script injection and a local proxy approach. No
  prebuilt binaries are copied.
- `TizenTube` — <https://github.com/reisxd/TizenTube> (GPL-3.0). Reference for
  settings/adblock/SponsorBlock UI behavior. No source files are copied.
- `mutube` — <https://github.com/Exaphis/mutube> (see upstream license).
  Reference for packaging and app-side script loading. No files are copied.

## SponsorBlock

- API: <https://sponsor.ajay.app>
- The SponsorBlock service and database are operated by the SponsorBlock
  project. This repository only calls the public API and is not affiliated with
  it.

## Networking libraries in the proxy / installer

The local proxy is statically linked with:

- **curl** — <https://curl.se/docs/copyright.html> (curl license)
- **OpenSSL** — <https://www.openssl.org/source/license.html> (Apache-2.0)
- **libpsl** — <https://github.com/rockdaboot/libpsl> (MIT)
- **zlib** — <https://zlib.net/zlib_license.html> (zlib license)
- **zstd** — <https://github.com/facebook/zstd> (BSD-3-Clause)

These are build dependencies from the PS5 payload SDK's userland; they are
linked into the released installer.

## Build tools

- **Terser** — <https://github.com/terser/terser> (BSD-2-Clause). Used only to
  minify `payload/inject.src.js` into `payload/inject.js`. It is not
  distributed.
