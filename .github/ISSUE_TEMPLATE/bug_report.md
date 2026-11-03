---
name: Bug report
about: The mod did not load, did not skip ads, or crashed the app
title: ""
labels: bug
---

**Console**
- PS5 firmware:
- Jailbreak (Y2JB / Relapse / other):
- etaHEN version:
- App title ID and version (`PPSA01650` / `PPSA01651` / `PPSA01652`, e.g. `01.000.003`):

**What happened**
<!-- toast missing, app crash, no adblock, no SponsorBlock, etc. -->

**Logs**
Run these and drag the files here:

```bash
python3 tools/ytps5.py clean-logs --host <PS5_IP>
python3 tools/ytps5.py send --host <PS5_IP> --wait 30
python3 tools/ytps5.py logs --host <PS5_IP> --save
```

Attach:

- `logs/data_ytps5-sponsorblock_injector.log`
- `logs/data_ytps5-sponsorblock_ytps5.log`
- `logs/download0_ytps5.log` if present
- `logs/mnt_sandbox_..._yt.log` if present

**Was the app running before you sent the ELF?**
<!-- yes / no -->
