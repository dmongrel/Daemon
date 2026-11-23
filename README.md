# Daemon

A Windows screen saver that pretends your computer has been taken over and is being used to hack
the world. It starts from your real desktop, tears it down, launches a fake program, and puts on a
Hollywood hacking show for a few minutes. Then the "monitor" switches off like an old CRT, and
after a moment of black the show starts again.

**Everything on screen is fake.** No network traffic is sent, no real window is touched, no file
is written, and every company, address, host name and figure is invented. The commands look like a
hacker's terminal, but no tool, flag or technique in them is real, and none would do anything if
typed.

## The show

About three and a half minutes, then it loops with a new random seed, so the targets, hosts and
figures change every time:

1. **Your desktop.** The first frame is a screenshot of your screen, so nothing seems to happen.
2. **Windows close.** Every open window plays the Windows 11 close animation at the same moment.
3. **The desktop is deleted.** The taskbar breaks into blocks that flicker and vanish, then the
   icons disappear one by one, until only the wallpaper is left.
4. **U R P0WNED.exe.** A new icon appears. A mouse pointer glides over and double-clicks it.
5. **The console.** A fake Git Bash window opens on the left and a script types commands at human
   speed, with bursts of output.
6. **The attack view.** A larger tactical panel opens on the right, animating each step against the
   current target: a port sweep, a route map, a perimeter breach, a key crack, an identity clone,
   exfiltration, a takeover. Money, data and control always flow from the target back to the
   source: your machine.
7. **The finale.** A fast montage over the remaining targets, a tally in the billions, and a
   sign-off.
8. **CRT off.** The picture squashes to a bright line, shrinks to a dot that lingers like a
   tube's phosphor, then embers out to black.

The desktop is photographed once, at start-up. The wallpaper is rebuilt from the wallpaper file
itself, laid out the way Windows lays it out, so it lines up pixel for pixel with the screenshot.
None of these images is written to disk. [docs/spec.md](docs/spec.md) has the full specification.

## Installing

Download `daemon.scr` from the [latest release](https://github.com/dmongrel/Daemon/releases/latest).
Right-click it and choose **Install**, or copy it to `C:\Windows\System32` and pick **Daemon** in
Screen Saver Settings.

The console's screenplay is a separate file, `daemon.script`, also on the release. Put it in the
same folder as `daemon.scr`. Without it, the saver plays a short built-in demo instead.

## The screenplay

The console is driven by a plain-text script, so the show can be rewritten without rebuilding.
Daemon looks for it in this order:

1. the file named by `DAEMON_SCRIPT`, if it exists;
2. `daemon.script` next to the `.scr`;
3. `docs\daemon.script` in the project tree;
4. otherwise, a small built-in demo.

Lines starting `$` are typed commands, plain lines are output, and `@` directives set the pace,
colour and target and cue the attack view's animations. Tokens such as `{TARGET}` and `{HOST}`
are filled in from the seed, so every run reads differently. The format and its rules for keeping
everything fictional are in [docs/script.md](docs/script.md). The shipping screenplay is
[docs/daemon.script](docs/daemon.script).

## Building

### Prerequisites (MSYS2 UCRT64)

- `g++`, `windres` and `make` (`mingw-w64-ucrt-x86_64-toolchain`)

Direct2D, DirectWrite, WIC and GDI+ come with Windows, so nothing else is needed.

### Commands

From an MSYS2 UCRT64 shell, or from Git Bash with `C:\msys64\ucrt64\bin` on `PATH`:

```sh
make            # build daemon.scr
make run        # build, then run it in a window
make install    # copy daemon.scr to System32 (needs an elevated shell)
make clean
```

## Running

| Argument | Behaviour |
|----------|-----------|
| (none) or `/s` | Full screen across every monitor; a key, a click or mouse movement ends it |
| `/p <hwnd>` | Live preview in the Screen Saver Settings dialog (`/p:<hwnd>` also works) |
| `/c` | There is nothing to configure: a short message box |
| `/w` | A resizable window, for development; ignores input |

### Developer environment variables

| Variable | Effect |
|----------|--------|
| `DAEMON_SEED` | Fix the random seed, so a run is repeatable |
| `DAEMON_SCRIPT` | Path of the screenplay to play |
| `DAEMON_LOG` | Path of a diagnostic log (seed, when the script finished, restarts, captures) |
| `DAEMON_CAPTURE` | Path of a `.bmp`: render one frame offscreen and quit |
| `DAEMON_CAPTURE_AT` | When to take that frame, in seconds. The clock doesn't reset on the loop, so a time past one run lands in the next |
| `DAEMON_CRT_NOW` | Skip straight to the CRT switch-off |

## Layout

```
src/main.cpp            modes, windows, input, the frame loop, capture hook
src/capture.*           phase 0: screenshot, wallpaper, clean desktop, taskbar, window and icon geometry
src/show.*              the sequence: windows closing, desktop and icons deleted, payload, CRT off
src/console.*           the fake Git Bash window
src/script.*            the screenplay parser, tokens, built-in demo, script lookup
src/attack.*            the attack view and its scenes
src/renderer.*          Direct2D / DirectWrite drawing
src/common.*            logging, seed, shared helpers
docs/spec.md            the full specification
docs/script.md          the screenplay format
docs/daemon.script      the shipping screenplay
```

## License

Daemon is released under the [MIT License](LICENSE).
