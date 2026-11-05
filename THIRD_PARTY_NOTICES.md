# Third-party notices

This project is licensed under **GPL-3.0**. It includes or links the following
third-party components. Their licenses apply to their respective files.

See **[CREDITS.md](CREDITS.md)** for complete credits and prior art, and
**[RESEARCH.md](RESEARCH.md)** for the public-project review. The settings
modal/QR renderer shapes are adapted from TizenTube's GPL-3.0 UI helpers; this
project is also GPL-3.0.

## etaHEN / etaHEN-Plugins

- Vendored: `third_party/etahen/include`, `third_party/etahen/libhijacker/source`,
  `third_party/etahen/libhijacker/libhijacker.a`.
- Upstream: <https://github.com/etaHEN/etaHEN-Plugins>
- Pinned upstream commit: `6339554e4e3c92c0a655a37194c1152d6b82ad88`
- License: GPL-3.0 (see `third_party/etahen/LICENSE`).

The `libhijacker.a` archive is built from the vendored `libhijacker/source`
with the PS5 payload SDK. It is included for reproducible builds; you can rebuild
it from that source or point `ETAHEN_DIR` at an external checkout.

**Local patch:** `libhijacker/source/offsets.cpp` and the bundled
`libhijacker.a` carry one addition over the pinned upstream commit: kernel
offsets for firmware 11.00, 11.20, 11.40 and 11.60. Values come from the
PS5 payload SDK's own `crt/kernel.c` table (`0x11400000` case) and
`EchoStretch/kstuff-lite` `prosper0gdb/offsets/11_40.h`. Upstream
etaHEN-Plugins stops at 10.60, which is why the unpatched build could not
attach on 11.40.

**Second local patch:** the process walk and root-vnode lookup now use the
payload SDK's runtime `KERNEL_ADDRESS_ALLPROC` / `KERNEL_ADDRESS_ROOTVNODE`
values (and `KERNEL_OFFSET_PROC_P_*` for the proc fields) instead of the
static table, so the payload follows whichever firmware the SDK supports.
The static table remains only as a diagnostic fallback and now covers
1.00 through 13.60.

## PS5 payload SDK

- Not vendored; install it separately.
- Upstream: <https://github.com/ps5-payload-dev/sdk>
- License: GPL-3.0+.

The build uses its compiler wrappers, headers, linker scripts, and stub
libraries.

## Hacker Disassembler Engine 64 (HDE64)

- Vendored: `module/hde64.cpp`, `module/hde64.h`, `module/table64.h`.
- Copyright (c) 2008-2009 Vyacheslav Patkov.
- BSD-style license; full text in `LICENSES/hde64.txt`. Used to calculate instruction boundaries.

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
linked into the released installer. Full license texts are in `LICENSES/`.

## Build tools

- **Terser** — <https://github.com/terser/terser> (BSD-2-Clause). Used only to
  minify `payload/inject.src.js` into `payload/inject.js`. It is not
  distributed.
