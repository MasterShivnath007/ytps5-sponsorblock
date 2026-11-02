# Third-party notices

This project is licensed under GPL-3.0. It includes or links the following
third-party components. Their licenses apply to their respective files.

## etaHEN / etaHEN-Plugins

- Vendored: `third_party/etahen/include`, `third_party/etahen/libhijacker/libhijacker.a`,
  `third_party/etahen/lib/libNidResolver.a`.
- Upstream: https://github.com/etaHEN/etaHEN-Plugins
- Pinned upstream commit: `6339554e4e3c92c0a655a37194c1152d6b82ad88`
- License: GPL-3.0 (see `third_party/etahen/LICENSE`).

These are used to attach to the YouTube process, read/write its memory, and
resolve the target's imports.

## PS5 payload SDK

- Not vendored; install it separately.
- Upstream: https://github.com/ps5-payload-dev/sdk
- License: GPL-3.0+.

The build uses its compiler wrappers, headers, linker scripts and stub
libraries.

## Hacker Disassembler Engine 64 (HDE64)

- Vendored: `module/hde64.cpp`, `module/hde64.h`, `module/table64.h`.
- Copyright (c) 2008-2009 Vyacheslav Patkov.
- BSD-style license. Used to calculate safe instruction boundaries when
  installing the `HTMLScriptExecute` hook.

## earthonion/yt_adskip_ps4

- Upstream: https://github.com/earthonion/yt_adskip_ps4
- License: MIT.
- Used as the reference for the Cobalt `HTMLScriptExecute` injection and the
  local SponsorBlock proxy approach. This repository does not copy its
  prebuilt binaries.

## TizenTube

- Upstream: https://github.com/reisxd/TizenTube
- License: GPL-3.0.
- Used as a reference for the adblock and SponsorBlock user-script behavior.

## SponsorBlock

- API: https://sponsor.ajay.app
- The SponsorBlock service and database are operated by the SponsorBlock
  project. This repository only calls the public API.

## OpenOrbis PS4 toolchain / libhijacker notes

`libhijacker.a` was built by the etaHEN project and is redistributed here as a
convenience for reproducible builds. If you prefer, remove the vendored binary
and build etaHEN-Plugins yourself; the Makefile's `ETAHEN_DIR` variable accepts
an external checkout.
