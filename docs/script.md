# Daemon — console script and attack-view cues

This is the screenplay for the left-hand Git Bash window and the cues that
drive the right-hand attack view. It is set dressing for a movie scene: the
commands look like a hacker's terminal but every tool, flag, host, address and
company is invented. Nothing here would do anything on a real machine.

The story: the machine running the saver is the **SOURCE**. The script
builds a relay chain out into the world, then takes down one **TARGET** after
another. Every attack succeeds. Totals climb into the billions.

---

## 1. Format

The engine reads this script line by line.

| Line | Meaning |
|---|---|
| `$ text` | A typed command. Typed at human speed (30–70 ms per key, jittered, an occasional pause or corrected typo), then Enter. |
| plain text | Program output. Printed fast, a line every 8–25 ms, in bursts. |
| `@vis NAME key=val …` | Start an animation in the right-hand window. |
| `@wait` | Hold the console until the current animation reaches its next beat. |
| `@pace slow` / `normal` / `fast` / `flood` | Output speed from here on. `flood` dumps a full screen at once and scrolls. |
| `@pause S` | Hold for S seconds, cursor blinking. |
| `@prompt` | Print the two-line Git Bash prompt (green `user@HOST`, purple `MINGW64`, yellow path, then `$ `). |
| `@color #RRGGBB` … `@color off` | Colour for the output lines that follow; `off` (or empty) returns to the default grey. See §2. |
| `@target [name]` | No name: advance to the next random target. With a name: set it. See §3. |
| `# text` | Comment for us. Never shown. |

### Placeholders

Written as `{TOKEN}` and filled in at load time from the random seed, so every
run reads differently. They work in `$` commands, in output lines, and in
`@vis` parameter values.

| Token | Example | Meaning |
|---|---|---|
| `{TARGET}` | `Cindermark Crypto` | The current target company. **Sticky** — stays the same until `@target` changes it, so a whole chapter shares one victim. |
| `{HOST}` | `vault-13.internal` | A made-up host name (`prefix-NN.tld`). |
| `{IP}` | `91.63.48.247` | A made-up IPv4 address. |
| `{PORT}` | `6379` | A plausible TCP port. |
| `{USER}` | `s.petrov` | A made-up account name. |
| `{HASH}` | `9af31c0b7e` | A run of hex, 10–16 digits — a fake key / session id. |
| `{AMOUNT}` | `$7.4 billion` | A headline sum of money. |
| `{RECORDS}` | `3.1 billion records` | A headline count of stolen data. |
| `{CC}` | `SG` | A two-letter country code. |
| `{ASN}` | `AS39288` | A made-up autonomous-system number. |
| `{PCT}` | `73` | An integer 1–100 (progress, etc.). |
| `{N}` | `8384` | An integer 2–9999 (counts). |
| `{MS}` | `42` | An integer 3–900 (milliseconds). |

Each occurrence is resolved independently, so repeating `{AMOUNT}` three times
on a line prints three different sums — good for a climbing counter.

---

## 2. The attack-view cues

`@vis NAME` switches the right-hand window (spec §1.7) to a scene and hands it
the current `{TARGET}`. Pass `target={TARGET}` explicitly to be safe; with no
parameter the engine falls back to the current target anyway.

| `@vis` name | Scene | Pair it with |
|---|---|---|
| `scan` | port sweep | recon / `scan` commands |
| `map` | route map | building the SOURCE → TARGET path |
| `breach` | perimeter breach | getting in |
| `crack` | key crack | recovering keys / credentials |
| `impersonate` | identity clone | taking over an identity |
| `exfil` | exfiltration stream | moving money or data TARGET → SOURCE |
| `takeover` | node takeover | seizing / persisting |
| `idle` | holding animation | the tally / sign-off |

Give a scene room to play: follow the cue with a `@pause` (about 2.5–3.5 s for a
full chapter beat) so the console slows while the animation runs, exactly as the
brief asks.

### Colours

`@color #RRGGBB` sets the colour of the output lines that follow, until the next
`@color`; `@color off` returns to the default grey. The palette in use:

| Colour | Hex | Used for |
|---|---|---|
| grey | `#C8C8C8` | default / neutral output |
| blue | `#8FD0FF` | informational `[scan] [route] [exfil]` steps |
| green | `#6FE26F` | `[ok]` successes |
| amber | `#FFC14D` | `[**]` warnings / aggressive steps |
| red | `#FF5A5A` | the hardest lines (arming, zeroing balances, vanishing) |

---

## 3. Chapters and targets

`@target` with no name advances to the next company in the per-run shuffled
pool, so each chapter gets a fresh, random victim and the order changes every
run. `@target Some Name` sets a specific one.

A chapter is one target taken through a few of the scenes above — not always the
same ones, so the montage does not repeat. The shipping screenplay runs: a cold
boot, six full chapters (recon+breach, credential crack, identity theft,
financial drain, mass exfil, takeover+persistence), a fast multi-target sweep,
and a tally / sign-off. Keep money and data flowing **TARGET → SOURCE** in every
line, per the brief's correction.

---

## 4. The shipping script

The actual screenplay lives beside this file as
[daemon.script](daemon.script); it is what the saver plays by default (spec §3).
Edit that file to change the show — no rebuild needed — and keep it inside the
movie-prop constraint in §1 and in spec §2: it must look like a hacker's
terminal and be, in fact, nothing at all.