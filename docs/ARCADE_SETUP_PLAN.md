# Arcade Machine Boot Plan

## Overview

The end goal is an arcade cabinet that boots directly into a fullscreen raylib application — no desktop, no login screen, just the app. Eventually `src/main.cpp` becomes a full arcade game menu, but for now it runs a 3D raylib test scene (a thick cube with holes) as a stand-in to validate the complete boot chain end-to-end.

This document covers:
- What we discovered about the machine when we SSHed in to investigate
- Why we made each decision
- The exact steps to implement the plan (not yet done — these are instructions)
- What files need to be created or changed and the full content of each

> **SSH credentials** for the arcade machine are stored in `.env` at the root of this repository. Load them before running any SSH commands:
> ```bash
> source .env
> sshpass -p "$ARCADE_PASSWORD" ssh ${ARCADE_USER}@${ARCADE_HOST}
> ```
> `.env` is git-ignored (never committed). See `.env.example` for the expected keys: `ARCADE_HOST`, `ARCADE_USER`, `ARCADE_PASSWORD`.
>
> **Note on `sshpass`:** The standard `ssh` command always prompts interactively for a password, which blocks scripting. `sshpass` feeds the password non-interactively. It was not installed by default on the dev machine and had to be installed first:
> ```bash
> sudo apt-get install -y sshpass
> ```

---

## Target Machine Specs

These were discovered by SSHing into the machine and running inspection commands.

| Property | Value |
|---|---|
| **Hostname** | `wyse-4932211` |
| **IP Address** | `10.42.0.220` (see `.env`) |
| **OS** | Debian GNU/Linux 13 (Trixie) |
| **Kernel** | `6.12.63+deb13-amd64` |
| **CPU** | AMD GX-424CC SOC with Radeon R5E Graphics (4-core x86_64) |
| **GPU** | AMD/ATI Mullins [Radeon R4/R5 Graphics] (rev 01) — integrated APU GPU |
| **Display Driver** | Mesa 25.0.7, RADEON KMS modesetting (open source AMD driver) |
| **Connected Display** | `card0-DP-2` (DisplayPort) — `1920x1080 @ 96 DPI` |
| **RAM** | 6.7 GiB total, ~6.1 GiB available |
| **Disk** | 55 GB total, ~46 GB free on `/` |
| **Display Manager** | LightDM (active + enabled via systemd, running since boot) |
| **Desktop Environment** | MATE (installed, currently the default login session) |
| **Session Protocol** | X11 (Xorg) — Wayland is **not** in use |
| **User** | `zhuberty` — in groups: `sudo`, `video`, `audio`, `plugdev` |

---

## What We Found When We SSHed In

### The display stack

The machine runs a standard Linux graphical stack:

```
systemd (graphical.target)
  └─ lightdm.service          ← Display manager, handles login + session launch
       └─ Xorg :0             ← X server, owns the display hardware
            └─ x-session-manager  ← Starts MATE desktop after login
                 └─ MATE panels, file manager, etc.
```

**LightDM** is the piece that sits between the hardware and the user session. It starts Xorg, shows the login greeter, and after authentication it hands off to whatever session the user selected. This is the key leverage point — by configuring LightDM's autologin, we can skip the greeter entirely and inject our own session (the raylib app) instead of MATE.

### The display manager config

`/etc/lightdm/lightdm.conf` was found to be entirely default — every line commented out. This is actually ideal: it means we can add our autologin settings cleanly with no conflicts from previous configuration.

### The GPU and OpenGL

The machine uses an AMD Mullins APU with an integrated Radeon GPU. The open-source `radeon` KMS driver is active, Mesa 25.0.7 provides OpenGL, and the display is connected via DisplayPort at **1920×1080**. Confirmed by `/var/log/Xorg.0.log`:

```
(II) RADEON(0): Output DisplayPort-1 using initial mode 1920x1080 +0+0
(==) RADEON(0): DPI set to (96, 96)
```

This is important: raylib uses OpenGL 3.3 Core via GLFW/X11, which is fully supported by this driver stack. No special configuration needed for OpenGL.

### The raylib project already builds on this machine

The project was already cloned and built at `~/apps/raylib-quickstart`. The build system is **Premake5 → GNU Make**. Premake5 generates the Makefiles from `build/premake5.lua`, which downloads raylib from GitHub on first run and compiles it as a static library. The compiled binary and static lib already exist:

```
bin/Debug/raylib-quickstart   ← the executable
bin/Debug/libraylib.a         ← raylib compiled as a static lib
```

All build tools are present on the machine: `gcc 14`, `g++ 14`, `make`, `build-essential`.

The project uses **OpenGL 3.3** (defined in `premake5.lua` as the default) with the **GLFW X11 backend** (`_GLFW_X11` defined for Linux without Wayland). This matches the machine's capabilities exactly.

### The MATE desktop session

MATE is a lightweight desktop environment. The machine has it installed and it's currently the default session (`/home/zhuberty/.dmrc` points to it). MATE loads:
- `marco` (window manager)
- `caja` (file manager / desktop)
- `mate-panel` (taskbar)
- `mate-power-manager`, `mate-screensaver`, etc.

All of this is unnecessary overhead for an arcade machine and will be completely bypassed. The user `zhuberty` is in the `sudo` group, so we have full administrative access to make system-level changes.

---

## Current State (Before Any Changes)

- LightDM starts at boot, shows a login greeter, user logs in manually.
- MATE desktop launches after login — full desktop environment, not the raylib app.
- The raylib binary (`bin/Debug/raylib-quickstart`) opens a **1280×720 windowed** app.
- No autologin configured. No custom session defined. No launcher script exists.

---

## Implementation Plan

These phases have **not yet been executed**. They are the instructions for completing the setup.

---

### Phase 1 — Modify `src/main.cpp` for Fullscreen

**Why:** The app currently calls `InitWindow(1280, 720, ...)`, which opens a regular desktop window. On a kiosk/arcade machine there's no window manager to manage it, and a windowed app on a bare X session would just sit in the top-left corner. We need it to take over the entire display.

**How raylib fullscreen works:** Calling `SetConfigFlags(FLAG_FULLSCREEN_MODE)` *before* `InitWindow` tells GLFW (raylib's windowing backend) to request a true fullscreen context from the OS — it switches the display mode rather than just making a borderless window. Passing `0, 0` as width/height tells raylib to use the monitor's current native resolution, which is `1920x1080` on this machine.

**`SetExitKey(KEY_NULL)`** disables the default behaviour where pressing `ESC` closes the app. On an arcade machine you don't want a stray button press to exit the launcher. A deliberate exit mechanism (e.g. a specific key combo) can be added later.

**Changes to `src/main.cpp`:**

```cpp
// Replace this single line in main():
InitWindow(1280, 720, "Thick cube with holes (earcut + raylib)");

// With these two lines:
SetConfigFlags(FLAG_FULLSCREEN_MODE);
InitWindow(0, 0, "Arcade");  // 0, 0 tells raylib to use the monitor's native resolution

// And add this immediately after InitWindow:
SetExitKey(KEY_NULL);  // Disable ESC so the app can't be accidentally closed
```

**Rebuild using the release config** (optimisations on, no debug symbols — appropriate for a kiosk):
```bash
cd ~/apps/raylib-quickstart
make config=release_x64
# Output: bin/Release/raylib-quickstart
```

> **Why release_x64?** The debug build (`bin/Debug/`) includes debug symbols and no compiler optimisations. For a kiosk that runs 24/7 you want the release build. The `x64` suffix is because this machine is `x86_64` — confirmed during the SSH investigation via `uname -a` and `lscpu`.

---

### Phase 2 — Configure LightDM Autologin

**Why:** By default LightDM presents a graphical login greeter and waits for a user to type their password. For an arcade machine that's unacceptable — nobody should need to log in. LightDM supports autologin natively via its config file.

**Why LightDM specifically?** We discovered during the SSH investigation that LightDM is already installed, active, and enabled as the system's display manager (`systemctl status display-manager` confirmed it). There's no GDM or SDDM on this machine. LightDM is also well-suited to kiosk setups — it's lightweight and its autologin configuration is straightforward.

**What `autologin-session=arcade` does:** Instead of launching whatever session the user last selected (MATE, in this case), LightDM will look up `arcade` in `/usr/share/xsessions/arcade.desktop` and launch that instead. We create that file in Phase 3.

**Why not just autologin into MATE and launch the app from there?** MATE starts a full desktop stack — window manager, file manager, taskbar, power manager, screensaver daemon. All of that loads before anything else runs, consumes RAM and CPU, and flashes briefly on screen. A dedicated bare session launches only what we specify — nothing else.

**Edit `/etc/lightdm/lightdm.conf`** (requires `sudo`):

Find the `[Seat:*]` section (it exists but is entirely commented out) and add these lines beneath the section header:

```ini
[Seat:*]
autologin-user=zhuberty
autologin-user-timeout=0
autologin-session=arcade
```

- `autologin-user=zhuberty` — the user to log in as automatically.
- `autologin-user-timeout=0` — don't wait at the greeter at all; log in immediately.
- `autologin-session=arcade` — the session to launch (matches the `.desktop` filename we create in Phase 3).

---

### Phase 3 — Create a Custom `arcade` X Session

**Why:** LightDM needs a `.desktop` file in `/usr/share/xsessions/` to know what a session *is*. When the `autologin-session=arcade` setting fires, LightDM reads `/usr/share/xsessions/arcade.desktop`, finds the `Exec=` line, and runs that command inside the already-running Xorg session. This is the same mechanism used by MATE, GNOME, KDE, etc. — we're just defining our own minimal one.

We confirmed during investigation that `/usr/share/xsessions/` currently contains only two files: `lightdm-xsession.desktop` (the generic fallback) and `mate.desktop`. We're adding a third.

**What "no window manager" means:** A standard Linux desktop session runs a window manager (e.g. `marco` for MATE, `kwin` for KDE) that draws title bars, handles Alt+Tab, manages window stacking, etc. Our session has none of that — the `Exec=` line runs *only* our launcher script. Raylib takes over the display via GLFW's fullscreen mode at the X11 level, so no window manager is needed or wanted.

**Create `/usr/share/xsessions/arcade.desktop`** (requires `sudo`):

```ini
[Desktop Entry]
Name=Arcade
Comment=Raylib Arcade Launcher
Exec=/home/zhuberty/apps/raylib-quickstart/launch-arcade.sh
TryExec=/home/zhuberty/apps/raylib-quickstart/bin/Release/raylib-quickstart
Type=Application
```

- `Exec` — the command LightDM actually runs to start the session. Points to our wrapper script (created in Phase 4).
- `TryExec` — a sanity check path; LightDM uses this to verify the session is available before offering it. Points to the actual binary.
- No `DesktopNames`, no `X-LightDM-*` keys needed — this minimal format is sufficient.

---

### Phase 4 — Create the Launcher Script and Fix the Resources Path

**Why a wrapper script at all?** Two reasons:

1. **Working directory problem.** The binary uses relative paths for all its resources:
   ```cpp
   LoadFontEx("resources/fonts/Inter-VariableFont_opsz,wght.ttf", ...);
   LoadShader("resources/shaders/glsl330/lighting.vs", ...);
   ```
   When LightDM launches a session, the working directory is **not** the project folder — it's typically `/` or `/home/zhuberty`. Raylib would silently fail to load these files (it doesn't crash, it just uses fallbacks or renders nothing). The wrapper script changes directory to the project root before executing the binary.

2. **Future extensibility.** The script gives us a place to add environment setup, cursor hiding, crash-restart logic, or environment variable exports without touching the C++ source.

**Why `exec` instead of just running the binary?** `exec` replaces the shell process with the binary process. Without `exec`, the shell stays alive as an idle parent. With `exec`, the binary *is* the session process — when it exits, the session terminates cleanly and LightDM takes control back.

**Create `/home/zhuberty/apps/raylib-quickstart/launch-arcade.sh`:**

```bash
#!/bin/bash
cd /home/zhuberty/apps/raylib-quickstart
exec ./bin/Release/raylib-quickstart
```

**Make it executable:**
```bash
chmod +x /home/zhuberty/apps/raylib-quickstart/launch-arcade.sh
```

> **Alternative: fix the path in code.** The project already includes `include/resource_dir.h`, which provides `SearchAndSetResourceDir("resources")` — a helper that walks up the directory tree until it finds the `resources/` folder. Adding that call at the top of `main()` makes the binary self-locating regardless of working directory. The shell wrapper is preferred for now because it keeps the C++ unchanged and is easy to extend.

---

### Phase 7 — Arcade Game Menu

**What this phase adds:** `src/main.cpp` becomes a real fullscreen arcade menu. Individual games are compiled as separate binaries under `games/`. The menu launches a selected game as a child process, waits for it to finish, then returns to the selector screen.

#### Architecture

```
bin/Release/raylib-quickstart   ← Arcade menu (launched by LightDM via launch-arcade.sh)
bin/Release/game1               ← Game 1 (Thick Cube demo)
bin/Release/game2               ← Game 2 (Thick Cube II — different params, blue tint)
```

**Why separate processes?** Each game is an independent binary with its own OpenGL context, assets, and resources. The menu forks a child process, `exec`s the game binary, then `waitpid`s until the child exits. When the game exits (via ESC), `waitpid` returns and the menu resumes. No shared state, no shared raylib context — clean separation.

**Concurrency model:**
1. Menu minimises its window (`MinimizeWindow()`).
2. Menu forks a child process.
3. Child: `chdir` to project root → `execv` game binary. The game runs with its own fullscreen GLFW context.
4. Parent: blocks in `waitpid`. Nothing is rendered while blocked — that's fine, the game has the display.
5. Game exits (ESC key, `SetExitKey(KEY_ESCAPE)` in the game binary).
6. `waitpid` returns in parent. Menu calls `RestoreWindow()` + `SetWindowState(FLAG_FULLSCREEN_MODE)` to reclaim the display.

**Navigation:** UP / DOWN arrows move the selection cursor. ENTER launches the highlighted game.

#### Directory structure added

```
games/
  game1/
    game1_main.cpp   ← Game 1 source (Thick Cube, white, standard camera)
  game2/
    game2_main.cpp   ← Game 2 source (Thick Cube II, blue tint, alternate camera)
```

Both game sources include `earcut.hpp` and `rlights.h` (shared from `src/`), and use `SearchAndSetResourceDir("resources")` so they locate their shaders and fonts regardless of CWD.

#### Premake changes

A reusable `define_game_project(name, srcDir)` Lua function was added to `build/premake5.lua`. It defines a `ConsoleApp` project that:
- Picks up all `.cpp/.h/.hpp` files from `srcDir` plus the shared earcut/rlights headers from `../src/`
- Links against `raylib` and the correct platform libraries
- Outputs to `../bin/%{cfg.buildcfg}/` alongside the menu binary

Two calls at the bottom register the game projects:
```lua
define_game_project("game1", "../games/game1")
define_game_project("game2", "../games/game2")
```

#### Build

```bash
# Regenerate Makefiles (adds game1/game2 targets)
cd ~/apps/raylib-quickstart/build && ./premake5 gmake

# Build all (menu + both games + raylib static lib)
cd ~/apps/raylib-quickstart && make config=release_x64

# Outputs:
#   bin/Release/raylib-quickstart  (menu)
#   bin/Release/game1
#   bin/Release/game2
```

#### Adding a real game later

1. Create `games/mygame/mygame_main.cpp` with a standard `int main()` that uses `SetExitKey(KEY_ESCAPE)`.
2. Add to `build/premake5.lua`: `define_game_project("mygame", "../games/mygame")`
3. Add to the `GAMES[]` array in `src/main.cpp`: title, description, and absolute binary path.
4. Rebuild: `make config=release_x64`

---

### Phase 5 — Test Without Rebooting

**Why test this way?** A full reboot takes time and if something is misconfigured, you end up staring at a black screen with no easy way to recover except physical access. Restarting LightDM over SSH achieves the same thing — it reloads the config, runs the autologin, and launches the session — but leaves your SSH connection alive so you can diagnose and fix any issues immediately.

**Important:** Run this from your SSH session *after* completing Phases 1–4.

```bash
sudo systemctl restart lightdm
```

LightDM will kill the current session (if any), reload its config, and fire the autologin. Watch the monitor — it should go directly to the raylib app with no MATE desktop visible.

**If the screen goes black and stays black:**
The session launched but the binary crashed or couldn't find its resources. Check:
```bash
cat ~/.xsession-errors | tail -30
```

**If LightDM itself fails to start:**
Something is wrong with the config syntax or the session file path. Check:
```bash
sudo journalctl -u lightdm -n 50
sudo cat /var/log/lightdm/lightdm.log | tail -50
```

**To recover MATE while SSHed in** (if you need to undo and get back to a normal desktop):
```bash
# Edit lightdm.conf and remove or comment out the autologin-session=arcade line
sudo nano /etc/lightdm/lightdm.conf
# Then restart LightDM — MATE will load instead
sudo systemctl restart lightdm
```

---

### Phase 6 — Full Reboot Validation

Once Phase 5 works correctly over SSH, do a full power-cycle reboot to confirm the entire boot chain works from cold start:

```bash
sudo reboot
```

**What happens at each stage:**

```
BIOS/UEFI POST
  └─ GRUB bootloader           (currently shows menu briefly — can be hidden later)
       └─ Linux kernel loads
            └─ AMD radeon KMS driver initialises the GPU + DisplayPort output
                 └─ systemd reaches graphical.target
                      └─ lightdm.service starts
                           └─ Xorg :0 starts  (1920x1080, DisplayPort)
                                └─ LightDM autologin fires for zhuberty
                                     └─ /usr/share/xsessions/arcade.desktop → Exec
                                          └─ launch-arcade.sh  (cd to project root)
                                               └─ raylib-quickstart  (fullscreen 1920x1080)
```

Estimated time from power-on to raylib app visible: **~15–25 seconds** on this hardware (AMD GX-424CC, spinning or SSD).

---

## File Change Summary

Everything that needs to exist or be changed to make this work:

| File | Action | Requires sudo | Notes |
|---|---|---|---|
| `src/main.cpp` | Modify | No | **Phase 7**: Now the arcade game menu — navigable list, fork/exec games |
| `games/game1/game1_main.cpp` | Create | No | **Phase 7**: Thick Cube demo as Game 1 (ESC returns to menu) |
| `games/game2/game2_main.cpp` | Create | No | **Phase 7**: Thick Cube II demo as Game 2 (different params, blue tint) |
| `build/premake5.lua` | Modify | No | **Phase 7**: Added `define_game_project()` helper + `game1`/`game2` projects |
| `launch-arcade.sh` | Create | No | Shell wrapper: sets CWD, then `exec`s the menu binary |
| `/usr/share/xsessions/arcade.desktop` | Create | Yes | Registers the `arcade` session with LightDM |
| `/etc/lightdm/lightdm.conf` | Modify | Yes | Adds `autologin-user`, `autologin-user-timeout`, `autologin-session` to `[Seat:*]` |

---

## Optional / Future Enhancements

### Crash-resilient launcher loop
If the binary crashes, LightDM will drop back to the greeter (or try to re-autologin, depending on LightDM version). To guarantee the app always restarts instead, replace `exec` in the launcher with a loop:

```bash
#!/bin/bash
cd /home/zhuberty/apps/raylib-quickstart
while true; do
    ./bin/Release/raylib-quickstart
    sleep 1   # brief pause before restart to avoid tight crash loops
done
```

### Hide the X11 cursor
On a bare X session with no window manager, the default X cursor (a plain X cross) may appear on screen. To suppress it, add before the `exec` in the launcher:
```bash
xsetroot -cursor_name none &
```
Alternatively, call `HideCursor()` inside the raylib app after `InitWindow` — this hides the cursor within raylib's rendering context.

### Suppress boot messages for a polished look
Currently the GRUB menu appears briefly and kernel boot messages scroll by. For a production arcade cabinet, suppress all of that:

In `/etc/default/grub`:
```ini
GRUB_TIMEOUT=0
GRUB_CMDLINE_LINUX_DEFAULT="quiet loglevel=0"
```
Then apply:
```bash
sudo update-grub
```

### Build strategy — on the arcade machine vs. dev machine
Both machines are `x86_64` with GCC 14, Premake5, and Make installed. Two options:
1. **Build directly on the arcade machine** — pull changes via `git pull`, run `make config=release_x64` there. Simplest, no file transfers.
2. **Build on dev, deploy via scp** — faster compile iteration if your dev machine is faster, then:
   ```bash
   source .env
   scp -r bin/Release/raylib-quickstart resources/ ${ARCADE_USER}@${ARCADE_HOST}:~/apps/raylib-quickstart/
   ```

### Future: Full arcade menu
The arcade menu (Phase 7) is now implemented. `src/main.cpp` is the game selector. Individual games live under `games/` as independent binaries.

---

## Quick Reference Commands

```bash
# ── SSH ACCESS ──────────────────────────────────────────────────────────────
# Load credentials from .env (git-ignored), then SSH in
source .env
sshpass -p "$ARCADE_PASSWORD" ssh ${ARCADE_USER}@${ARCADE_HOST}

# ── BUILD ────────────────────────────────────────────────────────────────────
# Regenerate Makefiles after adding/changing premake5.lua
cd ~/apps/raylib-quickstart/build && ./premake5 gmake

# Build all targets: menu + game1 + game2 + raylib (release)
cd ~/apps/raylib-quickstart && make config=release_x64
# Outputs: bin/Release/raylib-quickstart  bin/Release/game1  bin/Release/game2

# Deploy all binaries + resources from dev machine to arcade machine
source .env
scp -r bin/Release/raylib-quickstart bin/Release/game1 bin/Release/game2 resources/ \
    ${ARCADE_USER}@${ARCADE_HOST}:~/apps/raylib-quickstart/

# ── SETUP (run on arcade machine via SSH) ────────────────────────────────────
# Phase 1: rebuild after modifying src/main.cpp for fullscreen
make config=release_x64

# Phase 2: edit LightDM autologin config
sudo nano /etc/lightdm/lightdm.conf

# Phase 3: create the custom arcade X session file
sudo nano /usr/share/xsessions/arcade.desktop

# Phase 4: create and chmod the launcher script
nano ~/apps/raylib-quickstart/launch-arcade.sh
chmod +x ~/apps/raylib-quickstart/launch-arcade.sh

# Phase 5: test without rebooting
sudo systemctl restart lightdm

# Phase 6: full reboot validation
sudo reboot

# ── DIAGNOSTICS ──────────────────────────────────────────────────────────────
# LightDM service log (why did the session fail to start?)
sudo journalctl -u lightdm -n 50

# LightDM detailed log (autologin events, session launch)
sudo cat /var/log/lightdm/lightdm.log | tail -50

# X session errors (why did the binary crash or show nothing?)
cat ~/.xsession-errors | tail -30

# ── RECOVERY ─────────────────────────────────────────────────────────────────
# Restore MATE session (comment out autologin-session in lightdm.conf)
sudo nano /etc/lightdm/lightdm.conf
sudo systemctl restart lightdm
```
