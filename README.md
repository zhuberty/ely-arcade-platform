# ely-arcade-platform

The arcade cabinet itself: the boot menu, the machine setup instructions, and
the list of installed games (as git submodules).

| Path | What |
|---|---|
| `src/main.cpp` | Arcade menu. Launches each game as a child process from that game's own directory. |
| `sdk/` | Submodule: [ely-arcade-sdk](https://github.com/zhuberty/ely-arcade-sdk) (input, shared headers, premake helpers). |
| `games/*` | Submodules: one repo per game. Each also pins its own `sdk/`. |
| `docs/ARCADE_SETUP_PLAN.md` | How the cabinet was set up (LightDM autologin, X session, launcher). Use it to bootstrap another machine. |
| `launch-arcade.sh` | Started by the X session; runs the menu. |
| `scripts/build-all.sh` / `.bat` | Builds the menu and every game. |
| `.env.example` | Copy to `.env` (git-ignored) for SSH credentials. |

## Clone

```
git clone --recurse-submodules git@github.com:zhuberty/ely-arcade-platform.git
```

If you already cloned without it: `git submodule update --init --recursive`.

## Build

```
scripts/build-all.sh        # Linux (arcade machine)
scripts\build-all.bat       # Windows dev (needs mingw32-make on PATH)
```

Outputs: `bin/Release/ely-arcade-platform` and `games/<game>/bin/Release/<game>`.
Each project downloads raylib on first build (into its own `build/external/`).

## How the pieces fit together

```
ely-arcade-platform/          <- the menu (this repo)
├── sdk/                      <- ely-arcade-sdk: shared C++ code + ALL the premake logic
│   ├── premake/arcade_sdk.lua   (defines the raylib, sdk, and app projects)
│   └── tools/premake/           (premake5 executables for Windows/Linux/macOS)
├── build/premake5.lua        <- 8 lines: "build the menu using the SDK helpers"
├── src/main.cpp              <- menu source
└── games/<game>/             <- each game has the same shape as this repo:
    ├── sdk/                     its own pinned copy of the SDK
    ├── build/premake5.lua       "build this game using the SDK helpers"
    └── src/main.cpp
```

Every build is two steps. All the scripts and VS Code tasks are combinations of them:

1. **premake generates Makefiles.** Run the bundled premake with the `gmake` action from a project's `build/` folder
   (`../sdk/tools/premake/premake5 gmake`; see "Running premake" below).
   It reads `build/premake5.lua` (which calls `sdk/premake/arcade_sdk.lua`) and writes
   `Makefile` and `build/build_files/*.make`. The first time, it also downloads raylib into
   `build/external/`. The generated files are git-ignored, so you can delete and regenerate them.
2. **make compiles.** Run `make config=<debug|release>_x64` from the project root
   (`mingw32-make` on Windows). It builds raylib, then the SDK, then the app, into `bin/<Debug|Release>/`.

Redo step 1 only if you changed a `premake5.lua`, added or removed source files, or cloned fresh.
For an ordinary code edit, step 2 is enough.

The menu and each game are **self-contained**: each has its own `build/`, its own copy of
raylib, and its own `bin/`. Building one never touches another.

## Building and running

Run these from the root of the project in question. Use `mingw32-make` instead of `make` on
Windows.

### Running premake

Premake is **not installed system-wide**. This repo bundles it, so typing `premake5` alone
gives "not recognized" / "command not found". Run it by path, from the `build/` folder:

| Shell | Command (from `build/`) |
|---|---|
| PowerShell / cmd (Windows) | `..\sdk\tools\premake\premake5.exe gmake` |
| bash (Linux) | `../sdk/tools/premake/premake5 gmake` |

For a game, it's the same path relative to `games/<game>/build/`, because each game has its own `sdk/`.
PowerShell needs the `.\` or `..\` prefix for programs in a relative path. Quick check that it runs:
`..\sdk\tools\premake\premake5.exe --version`.

In the table below, `premake5` means that path.

| I want to... | Do this |
|---|---|
| Build everything (menu and all games, Release) | `scripts/build-all.sh` or `scripts\build-all.bat` |
| Build just the menu | Linux: `cd build && ../sdk/tools/premake/premake5 gmake && cd .. && make config=release_x64`. Windows PowerShell: `cd build; ..\sdk\tools\premake\premake5.exe gmake; cd ..; mingw32-make config=release_x64` |
| Build just one game | From `games/<game>/`: `./build.sh` (or `build.bat`). Same two steps, Release. |
| Build Debug instead | Use `config=debug_x64`. Output goes to `bin/Debug/`. |
| Rebuild after editing only `.cpp` files | Skip premake: `make config=release_x64` |
| Clean | `make clean` (add `config=...` if you built a non-default one) |
| Run the menu | `bin/Release/ely-arcade-platform` (on the cabinet, `launch-arcade.sh` does this) |
| Run a game by itself | `cd games/<game>`, then `bin/Release/<game>`. Games load `resources/` by relative path, so the working directory matters. |
| Generate Visual Studio files | `premake5 vs2022` instead of `gmake` |

- Plain `make` with no `config=` builds **debug_x64**. The scripts build **release_x64**.
- The menu only launches **Release** game binaries (`games/<game>/bin/Release/...`). A game built only in Debug won't launch from the menu.
- The menu runs each game with the game's own folder as the working directory. That is how games find their `resources/`.

## Debugging in VS Code

`.vscode/` is an editor convenience layer for the **menu** (this repo). It runs the same commands as the table above and doesn't change how anything is built.

- **`tasks.json`** defines named build commands. `build debug` and `build release` run `make` (`mingw32-make` on Windows). Both depend on `UpdateMake`, which runs `premake5 gmake` in `build/` first.
- **`launch.json`** defines what F5 does. Each entry runs a `preLaunchTask` (a build), then starts gdb on `bin/<Debug|Release>/<folder-name>`. The binary name comes from `${workspaceFolderBasename}`, so it has to match the folder name.
  - **Debug**: rebuild with premake regeneration, then debug.
  - **Debug NoPremake**: same, but skips regeneration. It's faster when only `.cpp` files changed.
  - **Run Release**: build Release and run it under gdb. Release has no debug symbols, so breakpoints won't work well.
- **`c_cpp_properties.json`** only tells IntelliSense where to find headers like `raylib.h`. It has no effect on the build.

### Known problems with the current VS Code setup

This `.vscode/` came from the raylib-quickstart template and hasn't been adapted to the SDK layout yet:

1. **`UpdateMake` fails.** It runs `./premake5.exe` inside `build/`, but premake lives in `sdk/tools/premake/`. The template put a copy in `build/`, and this layout doesn't.
2. **`Generate compile_commands.json` fails.** It runs the `ecc` premake action, which the bundled premake doesn't have (`Error: no such action 'ecc'`). `UpdateMake` depends on this task too.
3. **Net effect:** **Debug** and **Run Release** fail in their pre-launch task. **Debug NoPremake** skips `UpdateMake`, so it can work once the Makefiles exist (run step 1 from the build section once).
4. **Games have no `.vscode/`.** To debug a game, open it as its own VS Code folder with a copied `.vscode/`, or use gdb from a terminal.
5. **You can't debug a game through the menu.** The menu starts each game as a separate process and the debugger won't follow it. Debug the game directly.

### Debugging from a terminal (works today)

```
cd build && ../sdk/tools/premake/premake5 gmake && cd ..
make config=debug_x64
gdb bin/Debug/ely-arcade-platform
```

For a game, do the same inside `games/<game>/`, and run gdb from that folder so `resources/` resolves.

## Troubleshooting: my code, my setup, or my environment?

| Symptom | Likely cause |
|---|---|
| Compiler error pointing at a file in `src/` | Your code. |
| `premake5` not found / no such file | Wrong directory, or the SDK submodule isn't checked out (`git submodule update --init --recursive`). |
| `Error: no such action ...` | The bundled premake doesn't include that action. |
| raylib download fails | No network, or `build/external/` is half-populated. Delete `build/external/raylib-master*` and rerun premake. |
| `No rule to make target` or a stale build | Generated files are out of date. Rerun `premake5 gmake`. |
| Undefined references to `winmm`, `gdi32`, `X11`, etc. | OS libraries are set in `arcade.link_system_libs` in `sdk/premake/arcade_sdk.lua`. |
| Game runs but has no textures, fonts, or sounds | The working directory isn't the game's folder. |
| A game is missing from the menu | It isn't in `GAMES[]` in `src/main.cpp`, or its Release binary isn't built. |
| A game is in the menu but won't launch | Its `GAMES[]` path doesn't match the binary name, it was built in Debug only, or the submodule isn't checked out. |
| `scripts/build-all` skips a game | The game has no `build/` folder. |
| ESC doesn't return to the menu | The game doesn't call `SetExitKey(KEY_ESCAPE)`. |

## Adding a game

### Quick start: the generator script

```
scripts/new-game.sh <name>        # Linux / macOS / Git Bash
scripts\new-game.ps1 <name>       # Windows PowerShell
```

`<name>` is lowercase letters, digits and single hyphens (e.g. `space-blasters`). The script creates
`<parent of this repo>/ely-arcade-games/<name>/` (making `ely-arcade-games/` if needed) with everything
from step 1 and a starter `main.cpp` from step 2, runs `git init`, and adds the SDK submodule (this needs
SSH access to GitHub; if it fails, the script prints the command to run later). It prints the remaining
steps (3-6 below) when it finishes. Steps 1 and 2 describe what it generates.

### 1. Create the game repo

Layout (same as `game-thick-cube`):

```
<name>/
├── sdk/                   <- submodule of ely-arcade-sdk
├── build/premake5.lua
├── resources/             <- fonts, textures, sounds
├── src/main.cpp           <- every .cpp/.c/.h under src/ is built automatically
└── .gitignore             <- bin/ obj/ build_files/ external/ Makefile *.make
```

```
git init <name> && cd <name>
git submodule add git@github.com:zhuberty/ely-arcade-sdk.git sdk
mkdir build src resources
```

`build/premake5.lua` (replace `<name>` with the repo name; the binary gets this name):

```lua
dofile("../sdk/premake/arcade_sdk.lua")

arcade.prepare_dirs()
arcade.workspace("<name>")
arcade.raylib_project()
arcade.sdk_project("../sdk")
arcade.app_project("<name>", "../src", "../sdk")
```

Optionally copy `build.sh` / `build.bat` from `game-thick-cube` to build just this game.

### 2. Write `src/main.cpp`

Minimal skeleton:

```cpp
#include "raylib.h"
#include "resource_dir.h"
#include "arcade_input.h"

int main(void)
{
    SetConfigFlags(FLAG_FULLSCREEN_MODE);
    InitWindow(0, 0, "My Game");   // 0,0 = monitor native resolution
    SetExitKey(KEY_ESCAPE);        // ESC quits the game and returns to the menu
    SetTargetFPS(60);

    SearchAndSetResourceDir("resources");

    while (!WindowShouldClose())
    {
        if (arcade::IsActionPressed(arcade::Player::Any, arcade::Action::Confirm)) { /* ... */ }

        BeginDrawing();
        ClearBackground(BLACK);
        DrawText("Hello, arcade", 100, 100, 40, WHITE);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
```

What the menu expects from a game:

- **Exit cleanly.** The menu launches the game as a child process and waits for it. Call `SetExitKey(KEY_ESCAPE)` and return from `main` (after `CloseWindow()`) so control goes back to the menu. The menu itself disables ESC, so a game is the only place ESC exits.
- **Use `arcade_input.h`** rather than raw keys, so the game works with the keyboard, gamepads, and the cabinet's encoders. Use `Player::One` / `Player::Two` for two-player games and `Player::Any` for menus and single player. Actions: Up, Down, Left, Right, Confirm, Back, Restart. See `sdk/include/arcade_input.h` for the key and gamepad bindings.
- **Load assets by relative path.** The menu sets the working directory to the game's folder. `SearchAndSetResourceDir("resources")` then makes paths like `"fonts/x.ttf"` resolve to `resources/fonts/x.ttf`.
- **Name the binary after the folder** (`games/<name>/bin/Release/<name>`). The menu uses that path.

### 3. Build and test the game alone

```
cd build && ../sdk/tools/premake/premake5 gmake     # Windows: ..\sdk\tools\premake\premake5.exe gmake
cd .. && make config=release_x64                    # Windows: mingw32-make
bin/Release/<name>
```

### 4. Push the repo, then add it here

```
git submodule add <url> games/<name>
```

### 5. Register it in the menu

Add an entry to `GAMES[]` in `src/main.cpp`:

```cpp
{
    "My Game",
    "One-line description.",
    "games/<name>/bin/Release/<name>" GAME_EXE_EXT
},
```

### 6. Build everything and commit

Run `scripts/build-all.sh` (or `.bat`). It builds every `games/*` folder that has a `build/` directory. Start the menu, launch the game, and press ESC to check that it returns. Then commit `.gitmodules`, `games/<name>`, and `src/main.cpp` in this repo.

## Where to change build settings

All of these live in `sdk/premake/arcade_sdk.lua`, so a change there applies to the menu and every game the next time each one is rebuilt (games pin their own SDK copy, so bump the SDK in each game to pick it up).

| Change | Where |
|---|---|
| C++ standard (currently C++17) | `cppdialect` in `arcade.app_project` and `arcade.sdk_project` |
| Compiler flags / warnings | `flags`, `buildoptions` in the same functions |
| Debug vs Release settings | `arcade.workspace` (`DEBUG`/`NDEBUG`, symbols, optimize) |
| Libraries linked | `links` in `arcade.app_project`, and `arcade.link_system_libs` for OS libraries |
| Which source files are built | Automatic: everything under the project's `src/` (`**.cpp`, `**.c`, `**.h`) |
| raylib graphics or window backend | Command line: `premake5 gmake --graphics=opengl33 --backend=glfw` |

## Updating

- Bump a game: `git -C games/<name> pull origin main`, then commit the new pointer here.
- Bump the SDK for a game: pull inside `games/<name>/sdk`, commit the new SDK pointer in the game repo, push, then bump the game here. Skipping the push or the final commit here leaves the platform pointing at the old version.
- `docs/ARCADE_SETUP_PLAN.md` is a historical record of the machine setup. Its "Adding a real game later" steps (`define_game_project`) are outdated. Use "Adding a game" above.

## Notes

- `resources/fonts/Inter-VariableFont_opsz,wght.ttf` was never tracked in the old repo. Add it if you want the menu font (otherwise raylib's default font is used).
- `docs/ARCADE_SETUP_PLAN.md` and `launch-arcade.sh` still mention the old `~/apps/raylib-quickstart` path in places. The launcher script is updated. The doc is unchanged apart from its new location.
