# YouTube upload — ytps5

Everything here goes with `youtube/ytps5-70s.mp4` (68 s, 1276×822, natural
audio, no music). The narration track is not recorded yet; `narration.srt` is
the script timed to the cut.

## Title

**Block YouTube Ads on PS5 — One Payload, No App Install (ps5yt)**

Alternates, if you want to A/B:

- `PS5 YouTube Ad Blocker in 2 Minutes — No App Install (FW 8.20)`
- `Level Up Your PS5: YouTube Without Ads, One ELF (ps5yt)`

Why the first one: it leads with the exact words people search — *block YouTube
ads PS5* — and the differentiator is in the second half. "Level up your PS5" is
generic and buries the keyword.

## Description

```
YouTube on PS5 shows an ad before almost every video. This removes them —
without installing a second app, and without touching the official app package.

It runs as a payload on a jailbroken console, attaches to the YouTube app you
already own, and filters the ads in memory. Sponsor segments are skipped through
the public SponsorBlock API. Nothing is written to the app, the save data, or
your autoload chain unless you put it there.

TESTED ON
PS5 firmware 8.20 · YouTube 01.000.003 (PPSA0165x)
The payload patches whichever build you name in config.txt — not just one.

WHAT YOU NEED
- A jailbroken PS5 on firmware 8.20
- Payload Manager (or any ELF loader)
- The official YouTube app (PPSA01652 by default)

INSTALL
1. Download ytps5.elf: https://github.com/MasterShivnath007/ytps5-sponsorblock/releases/latest/download/ytps5.elf
2. Payload Manager → local payloads → add ytps5.elf
3. Run it and wait for "ytps5: waiting for PPSA01652"
4. Open YouTube — it freezes for a second, then shows "ytps5: hook installed"
5. Settings → ytps5 Settings

CHOOSING WHICH APP GETS PATCHED
ytps5 writes /data/ytps5-sponsorblock/config.txt on first run:

    patch_title=PPSA01652   # PPSA01650 | PPSA01651 | PPSA01652 | AUTO
    allow_y2jb=0            # 1 = allow patching the Y2JB daily drivers

PPSA01650 and PPSA01651 are Y2JB daily drivers. They work, but they are refused
unless you set allow_y2jb=1, so the payload cannot break your autoload chain by
accident.

WHAT IT DOES NOT DO
No app packages, no console keys, no DRM tools, no HDCP bypass, no decryption.
It does not modify the installed app; the patch lives in memory and disappears
when the app or console restarts.

Links
Repository: https://github.com/MasterShivnath007/ytps5-sponsorblock
Site: https://mastershivnath007.github.io/ytps5-sponsorblock/
Support: https://github.com/sponsors/MasterShivnath007

Unofficial, research and educational project. Not affiliated with Sony, Google,
YouTube, or SponsorBlock. Use at your own risk.

#PS5 #Jailbreak #YouTube #Homebrew #PS5Homebrew
```

## Tags

```
ps5 youtube ads, block youtube ads ps5, ps5 ad blocker, youtube ad blocker ps5,
ps5 jailbreak, ps5 homebrew, ps5 payload, payload manager ps5, ps5 8.20,
firmware 8.20, ps5 sponsorblock, sponsorblock ps5, ytps5, y2jb, etahen,
ps5 youtube no ads, youtube without ads ps5, ps5 mod, ps5 tutorial
```

Under 500 characters, which is YouTube's limit. The first three carry the
search intent; the rest catch the jailbreak crowd.

## Hashtags

`#PS5 #Jailbreak #YouTube` — YouTube shows at most three above the title, so
more than that is wasted. The full set goes at the bottom of the description.

## Pinned comment

```
Two notes that the video does not cover:

• This is not the native app project with a similar name. It patches the
  official YouTube app you already own — nothing is replaced, nothing is
  installed as a package.

• The config file decides which YouTube build gets patched. If 01652 is not the
  one on your console, set patch_title to yours, or AUTO.

Questions, or it broke something? Open an issue on the repo, not here — I do not
get notifications for YouTube comments.
```

## Chapters (only if you also upload the full 2:40 version)

```
00:00 What this does
00:10 Firmware and app version
00:30 Installing the payload
01:18 Opening YouTube
01:25 Videos with no ads
02:08 The settings panel
02:20 Choosing which app to patch
```

Chapters need three or more entries, each at least 10 seconds, starting at
`00:00` — the 68-second cut is too short to bother, the full version is not.

## Thumbnail

`docs/media/thumbnail.png` — 1280×720, safe for the 16:9 crop, readable at
320 px wide. No YouTube logo, no Sony logo, no play button: the word "YouTube"
appears once as plain text, which is descriptive use.

## Upload checklist

- [ ] Title pasted
- [ ] Description pasted (check the links resolve)
- [ ] Tags pasted
- [ ] Thumbnail uploaded
- [ ] "Not made for kids"
- [ ] Category: Science & Technology
- [ ] Record narration over `narration.srt`, or upload it as captions
- [ ] End screen: link the site
- [ ] Add to a playlist: "PS5 homebrew"
