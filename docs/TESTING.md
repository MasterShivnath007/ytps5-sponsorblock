# Testing on the console

Tested target: PS5 firmware 8.20, Y2JB + Relapse, etaHEN with FTP on 2121 and
elfldr on 9021. The injector itself is firmware-independent; it uses libhijacker.

## 1. Prepare the console

1. Boot the PS5 and let the jailbreak + etaHEN finish.
2. Confirm FTP works:

   ```bash
   python3 - <<'PY'
   from ftplib import FTP
   ftp = FTP(); ftp.connect("<PS5_IP>", 2121, timeout=10)
   ftp.login("anonymous", "anonymous")
   print(ftp.pwd())
   ftp.quit()
   PY
   ```

3. Confirm the official YouTube app is installed. The injector accepts
   `PPSA01650`, `PPSA01651` and `PPSA01652`.

## 2. Build (or use the release ZIP)

```bash
export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk
export LLVM_CONFIG=/opt/homebrew/opt/llvm@18/bin/llvm-config
make
```

You should get `build/ytps5-injector.elf` and `build/sponsorblock.so`.

## 3. Send it

```bash
python3 tools/ytps5.py send --host <PS5_IP> --wait 30
```

Expected helper output:

```
uploaded sponsorblock.so -> /data/ytps5-sponsorblock/sponsorblock.so (...)
uploaded inject.js -> /data/ytps5-sponsorblock/inject.js (...)
uploaded ytps5-injector.elf -> /data/ytps5-sponsorblock/ytps5-injector.elf (...)
sending ytps5-injector.elf (...) to <PS5_IP>:9021 ...
ELF sent
waiting 30s ...
```

Then launch YouTube. The injector waits for it, so you can launch it after
sending the ELF.

## 4. What you should see

- A toast: `Ad Block + SponsorBlock Enabled!`
- Open a video that has sponsor segments. The toast `SponsorBlock: N segment(s)
  found` should appear, then `Segment Skipped` when the segment starts.
- Ads should not play before/inside videos.

## 5. If nothing happens

Run:

```bash
python3 tools/ytps5.py clean-logs --host <PS5_IP>
python3 tools/ytps5.py send --host <PS5_IP> --wait 30
python3 tools/ytps5.py logs --host <PS5_IP> --save
```

`logs/` will contain:

- `data_ytps5-sponsorblock_injector.log`
- `data_ytps5-sponsorblock_ytps5.log`
- `download0_ytps5.log` (if the module used the fallback path)

Open an issue and attach those files plus:

- PS5 firmware
- YouTube version (the app's version, e.g. `01.000.003`)
- Whether the ELF was sent before or after launching YouTube
- What you saw (toast, crash, black screen, no change)

### Common log lines

| Log line | Meaning |
|---|---|
| `found YouTube title=... pid=...` | The injector found the app. |
| `scePadReadState=... GOT=...` | The PLT/GOT patch target was found. |
| `hooked scePadReadState GOT ...` | The loader is installed. |
| `module install start` | `sponsorblock.so` was loaded into the app. |
| `HTMLScriptExecute candidate at ...` | The Cobalt hook target was found. |
| `[inject] context=...` | The JS payload was injected into a Cobalt context. |
| `HTMLScriptExecute not found` | The signature needs updating for this app version. |
| `kernel_mprotect(...) -> ...` | Whether the app's code page was made writable. |

## 6. Repeat tests

The GOT patch remains until YouTube is restarted. To re-test:

1. Close YouTube completely.
2. `clean-logs`.
3. `send` again.
4. Launch YouTube.

If the app is already patched, sending the injector again will see the GOT slot
already pointing at the old hook and may re-patch it. Closing YouTube is the
clean way.

## 7. Uninstall

Close YouTube and reboot the console. The patch only lives in the running
process; nothing is installed into the app. You can also delete
`/data/ytps5-sponsorblock/` over FTP.
