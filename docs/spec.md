# Daemon — screen saver specification

Daemon is a Windows screen saver that pretends the computer it runs on has
been taken over and is now being used to hack the world. It starts with the
user's real desktop, tears it down, launches a fake program, and then puts on a
Hollywood hacking show for a few minutes before the "monitor" switches off like
an old CRT. After five seconds of black it starts again.

Everything on screen is fake. No network traffic is sent, no real window is
touched, no file is written, and every company, address and host name is made
up.

---

## 1. The sequence

One run of the show lasts about **3 minutes 30 seconds** by default (see §9).
Times below are from the moment the saver starts.

| # | Phase | Approx. time | What the viewer sees |
|---|---|---|---|
| 0 | Capture | before the first frame | Nothing. The desktop is photographed before the saver window appears. |
| 1 | Hold | 0.0 – 1.5 s | Their own desktop, exactly as they left it. Nothing seems to have happened. |
| 2 | Windows close | 1.5 – 1.8 s | Every open window closes **at the same moment**, revealing the desktop. |
| 3 | Desktop deleted | 2.2 – 5.0 s | The taskbar and other desktop furniture are erased piece by piece. |
| 4 | Icons deleted | 5.0 – 7.0 s | The desktop icons vanish one after another, quickly. |
| 5 | The payload | 7.0 – 9.5 s | A new icon, **U R P0WNED.exe**, appears. A mouse pointer glides over and double-clicks it. |
| 6 | Console | 9.5 s on | A Git Bash window opens on the left and a script starts typing commands. |
| 7 | Attack view | ~20 s on | A larger window opens on the right with animated attack graphics tied to the console. |
| 8 | Finale | last ~15 s | The last target falls; a closing flourish across both windows. |
| 9 | CRT off | ~0.8 s | The picture collapses to a thin horizontal line, then to a dot, then nothing. |
| 10 | Black | 5 s | Pure black. |
| — | Loop | | Back to phase 1, using the cached desktop images, with a new random seed. |

Input ends the saver at any point, including during the black gap, in the
usual way (§8).

### 1.1 Phase 0 — capture

Before any window is created, Daemon gathers four images and some geometry.
All of it stays in memory and is never written to disk (outside the dev
capture hooks in §10).

1. **Screenshot.** The whole virtual screen (all monitors) as it is right now,
   via `BitBlt` from the screen DC with `CAPTUREBLT`. Per-monitor DPI aware, so
   it is in physical pixels.
2. **Wallpaper.** A screen-sized copy of the background picture for each
   monitor, built from the wallpaper file itself rather than from the screen:
   - `IDesktopWallpaper` gives, per monitor, the image path, the position mode
     (fill, fit, stretch, tile, centre, span) and the background colour.
   - The image is loaded with WIC and laid out exactly as Windows lays it out
     for that mode, so it lines up pixel for pixel with the screenshot.
   - Fallbacks, in order: `SPI_GETDESKWALLPAPER`; the cached
     `%APPDATA%\Microsoft\Windows\Themes\TranscodedWallpaper`; the solid
     background colour. A slideshow or Spotlight wallpaper is handled by
     whichever of these returns the picture currently shown.
3. **Clean desktop.** The wallpaper with the desktop icons on it but no windows
   — what the screen would look like after "Show desktop". Two ways to get it,
   chosen by a spike (§11):
   - **A (preferred):** `PrintWindow(…, PW_RENDERFULLCONTENT)` on the desktop's
     icon view (`SHELLDLL_DefView` / `SysListView32` under Progman or WorkerW),
     which renders it even though windows cover it.
   - **B (fallback):** draw it ourselves on the wallpaper: icon positions from
     the shell's desktop `IFolderView` (`GetItemPosition`, `GetSpacing`,
     `IFolderView2::GetViewModeAndIconSize`), images from
     `IShellItemImageFactory`, labels in Segoe UI white with the Windows drop
     shadow.
4. **Taskbar.** Cut out of the screenshot using `SHAppBarMessage
   (ABM_GETTASKBARPOS)`, plus any secondary-monitor taskbars
   (`Shell_SecondaryTrayWnd`). An auto-hidden taskbar is simply absent.

Geometry recorded alongside:

- every visible, non-cloaked top-level window in z-order, with its true frame
  from `DwmGetWindowAttribute(DWMWA_EXTENDED_FRAME_BOUNDS)`;
- every desktop icon's rectangle (icon plus label), from the same
  `IFolderView` calls as fallback B;
- the monitors, from `EnumDisplayMonitors`.

The first frame the saver shows is the screenshot, so switching from the real
desktop to the saver's picture of it is invisible.

### 1.2 Phase 2 — the windows close

The screenshot is cut into pieces: each pixel belongs to the topmost window
covering it, or to the desktop. All the window pieces then play the Windows 11
close animation **together** — scale to about 96 % around their own centre and
fade out over ~180 ms — over the clean desktop image. The taskbar stays.

A few windows always get skipped: the taskbar, the desktop itself, and
anything cloaked or zero-sized.

### 1.3 Phase 3 — the desktop is deleted

"Deleting" means painting over something with the matching patch of the
wallpaper, so it looks as though it was never there. The taskbar (and, if
present, widgets, the system tray and secondary-monitor taskbars) goes this
way: it breaks into blocks of 16–48 px that disappear in a quick sweep with
some jitter, each block replaced by the wallpaper underneath. A block may
flicker once or shift a pixel or two before it goes, so it reads as
corruption rather than as a wipe.

### 1.4 Phase 4 — the icons are deleted

The same method, one icon at a time, in a random order or a sweep. About 60–120
ms per icon, with a short lag between them; for a desktop with many icons the
pace shortens so the phase stays within about 2 s. Each icon's rectangle
shatters into a few blocks and is replaced by wallpaper.

After this the screen shows nothing but the wallpaper.

### 1.5 Phase 5 — U R P0WNED.exe

- A new icon fades in on the first free grid slot (the top-left one, now empty),
  at the system's desktop icon size and spacing. The icon is our own artwork:
  a pixel skull in a terminal window, drawn in code. Its label is
  `U R P0WNED.exe`.
- A mouse pointer appears near the centre of the primary monitor. It is drawn
  by us (the real arrow cursor image, from `LoadCursor(IDC_ARROW)`); the real
  cursor stays hidden.
- The pointer glides to the icon on an eased, slightly curved path, as a hand
  would move it (~0.8 s).
- Double click: the icon shows the hover tint, then the blue selection box, on
  two clicks ~120 ms apart.
- The pointer turns into the "app starting" cursor (`IDC_APPSTARTING`) for half a
  second, then the console opens.

### 1.6 Phase 6 — the console (left window)

A fake **Git Bash** window — mintty, not cmd and not PowerShell.

**Placement.** Centred in the left half of the primary monitor, 50 % of the
screen height. The width is 46 % of the screen by default: at exactly 50 % it
would feel too wide to sit in one half, so it is pulled in a little.

**Chrome.** A dark mintty title bar (`MINGW64:/opt/daemon`) with the usual
minimise / restore / close glyphs (dead — they are decoration), a one-pixel
border, and a near-black body (`#0C0C0C`). The prompt is the Git Bash default:
green `user@host`, purple `MINGW64`, yellow path, then `$ ` on the next line.

**Typing.** The window runs a script (§3). Typed commands appear key by key at
roughly human speed; output lines print faster, in bursts, in the mintty
palette. A block cursor blinks on the active line. When the text passes the
bottom of the body it scrolls, oldest lines first, so the window reads like a
live terminal.

**Driving the show.** As the script runs it emits `@vis` cues; each cue hands
the attack view (§1.7) a scene name and the current target. The first cue is
also what opens the attack view.

### 1.7 Phase 7 — the attack view (right window)

A second, larger window opens on the right once the first `@vis` cue fires.

**Placement.** Centred in the right half of the primary monitor, **75 %** of the
screen height and **40 %** of the width, so it overlaps the console's half only
slightly and clearly dominates it.

**Frame.** A dark tactical panel: a title strip (`TARGET ACQUISITION //
DAEMON`), a **SOURCE** header on the left (`DAEMON-NODE`) and a **TARGET** header
on the right (the current fictional company), HUD brackets at the corners,
faint scan-lines, and a footer (`LINK SECURE  INTRUSION ACTIVE   MODE: <scene>`).

**Scenes.** One per `@vis` cue, each a stylised animation of a step in the
attack, redrawn from a per-scene seed so it is different every run:

| Cue | Scene | Shows |
|---|---|---|
| `scan` | Port sweep | a service list filling in (open / filtered) and a radial sweep over the target. |
| `map` | Route map | the SOURCE → relay → TARGET path drawing itself, pulses travelling **toward the source**. |
| `breach` | Perimeter breach | a wall of nodes; the perimeter buckling and a path punching through to the core. |
| `crack` | Key crack | a vault / tumbler working, hash digits settling into a recovered key. |
| `impersonate` | Identity clone | a target identity card copied onto a SOURCE badge. |
| `exfil` | Exfiltration | chevrons and a labelled `TARGET → SOURCE` stream draining value out of the target. |
| `takeover` | Takeover | the target's nodes flipping, one by one, to the SOURCE colour. |
| `idle` | Idle | a slow holding animation when no cue is active (e.g. the closing tally). |

**Direction of flow.** The source is the thief and the target is the victim, so
every flow of money, data or control runs **from target to source** — the map
pulses, the exfil chevrons and the takeover all point back at the SOURCE.

### 1.8 Phase 8 — the finale

The script's last chapter is a fast montage: a quick hit on each remaining
target, then a tally (enterprises owned, funds and records moved to the source,
identities held) and a sign-off (`nothing was ever here.`). When the last line
has printed and its final pause has elapsed, the console is **finished**, and a
short beat (2.5 s) later the CRT switch-off begins.

### 1.9 Phase 9 — the CRT switches off

Over about **0.85 s**, in three parts, entirely in bright white — like an old
tube losing deflection:

1. **Squash** (first ~30 %): the picture collapses vertically into a bright
   white horizontal band.
2. **Beam** (next ~50 %): the band becomes a thin white beam that recedes
   horizontally toward the centre, a soft glow around it.
3. **Dot** (last ~20 %): the beam shrinks to a white dot that flares and dies.

### 1.10 Phase 10 — black, then loop

The screen holds pure black for **5 s**. Then the show restarts from phase 1,
reusing the cached capture images from phase 0 (the desktop is photographed only
once, at startup) but with a **new random seed**, so the targets, imagery, hosts
and figures are all different. All per-run state is reset, so every loop plays
the script in full from the top.

---

## 2. Fictional content and safety

Daemon is a prop. The constraint on every visible string is that it reads like a
hacking scene but is not one:

- Invented tool and command names (`daemon`, `breach`, `exfil`, `crack`,
  `impersonate`, `takeover`, …); no real tools, flags, CVEs or techniques.
- Invented companies, host names, IP addresses, keys, ASNs and dollar / record
  figures, all generated from the seed.
- No sequence of commands that would do anything if typed; no payloads.

Nothing is sent, touched or written (outside the dev hooks in §10). See
[script.md](script.md) for the authoring rules that keep it that way.

---

## 3. The console script

The left window is driven by an external script file so the screenplay can be
rewritten without rebuilding. Resolution order:

1. `%DAEMON_SCRIPT%`, if set and the file exists;
2. `daemon.script` next to the executable;
3. `docs\daemon.script` in the project tree;
4. a small built-in demo, if none of the above is found.

The format (directives, `$`/output lines, `@vis` cues, `@color`, tokens) is
specified in full in [script.md](script.md). The shipping screenplay is
[daemon.script](daemon.script).

---

## 4. Rendering

- **Direct2D** (`ID2D1HwndRenderTarget` at 96 DPI, so 1 DIP = 1 px) for all
  drawing; **DirectWrite** for text; **WIC** for loading the wallpaper and for
  offscreen capture (§10); **GDI** only for the phase-0 screenshot and taskbar
  cut-out.
- The show is drawn in a fixed **virtual-pixel space** (the virtual-screen size)
  and mapped into the target window with a world transform, so fullscreen,
  preview and windowed modes all share one layout.
- One run-target is recreated on device loss (`D2DERR_RECREATE_TARGET`); all
  bitmaps and brushes are rebuilt from the cached images on the next frame.

---

## 5. Monitors and scaling

Phase 0 records every monitor and the full virtual-screen bounds. The capture
images span all monitors; the windows (console, attack view) and the payload
icon are placed relative to the **primary** monitor. The manifest marks the
process **Per-Monitor-V2 DPI aware**, so captures are in physical pixels and
nothing is bitmap-stretched by the OS.

---

## 6. Screen-saver modes

Standard screen-saver command line:

| Args | Mode | Behaviour |
|---|---|---|
| `/s` | Full screen | A top-most pop-up over the whole virtual screen. The real show. |
| `/p <hwnd>` | Preview | A child window inside the Screen Saver dialog's little monitor. Both `/p 1234` and `/p:1234` spellings are accepted. |
| `/c` | Configure | There is nothing to configure; a short message box. |
| `/w` | Windowed | A normal resizable window, for development. |

With no arguments the saver runs full screen.

---

## 7. Input and exit

In **full-screen** mode the saver exits on the first real input: a key, a mouse
button, or mouse movement past a small threshold (to ignore the jitter of the
cursor being hidden). The real cursor is hidden for the duration. **Preview** and
**windowed** modes ignore input and never self-exit.

---

## 8. Timing and duration

A run lasts as long as the script takes to play, plus the 2.5 s beat, the 0.85 s
CRT and the 5 s black — about **3 min 30 s** with the shipping screenplay. The
show loops forever until input ends it. Timing is wall-clock in normal use
(`WM_TIMER` every 16 ms); the capture hooks (§10) use a fixed `dt` of 1/60 s for
repeatable frames.

---

## 9. Developer capture hooks

Environment variables, all optional, for building and verifying frames offline.
They do not affect a normal run.

| Variable | Effect |
|---|---|
| `DAEMON_SEED` | Fixes the RNG seed, so a run is byte-for-byte repeatable. |
| `DAEMON_SCRIPT` | Path to the console script to play (see §3). |
| `DAEMON_LOG` | Writes a diagnostic log (seed, `finishedAt`, `restart at t=`, `captured frame at t=`). Truncated per process. |
| `DAEMON_CAPTURE` | Path to a `.bmp`; the saver grabs one frame and quits. |
| `DAEMON_CAPTURE_AT` | The time (s) at which to grab. This is a **monotonic** clock that does **not** reset on the loop, so a value past one run's length lands in the next loop — handy for testing the restart. |

The grab renders the frame **offscreen to a WIC bitmap**, not by reading back
the window, because reading a Direct2D window with `BitBlt` / `PrintWindow` is
unreliable when the saver is launched from a background shell.

---

## 10. Notes / open questions

- Desktop-capture method A (`PrintWindow(PW_RENDERFULLCONTENT)` on the icon
  view) vs. B (redraw from `IFolderView`) is chosen at runtime; B is the always-
  available fallback.
- Auto-hidden or per-monitor taskbars, slideshow / Spotlight wallpapers, and
  mixed-DPI multi-monitor setups are the fiddly cases to keep an eye on.