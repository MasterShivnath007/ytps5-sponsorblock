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
- App title ID and version (`PPSA01652` recommended; `PPSA01650`/`PPSA01651` are Y2JB apps and are not supported for patching):
- Release version (`v1.0.0`, etc.):

**What happened**
<!-- toast missing, app crash, no ad blocking, no SponsorBlock, settings missing, etc. -->

**Logs**

Attach the three files from the console:

- `/data/ytps5-sponsorblock/relay.log`
- `/data/ytps5-sponsorblock/injector.log`
- `/data/ytps5-sponsorblock/proxy_server.log`

If you use the helper script, you can collect them with:

```bash
python3 tools/ytps5.py logs --host <PS5_IP> --save
```

**Checklist**

- [ ] I used the clean app `PPSA01652`, not `PPSA01650`/`PPSA01651`.
- [ ] I ran `ytps5.elf` after the last reboot/jailbreak.
- [ ] The installer showed “ytps5: waiting for the supported app”.
- [ ] I opened the app after the installer was waiting.

**Was the app running before you ran the installer?**
<!-- yes / no -->
