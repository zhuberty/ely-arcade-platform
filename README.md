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

## Adding a game

1. Create the game repo from the same layout as `game-thick-cube` (`sdk/` submodule, `build/premake5.lua` calling `arcade.app_project`, `resources/`).
2. `git submodule add <url> games/<name>`
3. Add an entry to `GAMES[]` in `src/main.cpp` (path is `games/<name>/bin/Release/<name>`).

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
- Bump the SDK for a game: pull inside `games/<name>/sdk`, commit in the game repo, push, then bump the game here.

## Notes

- `resources/fonts/Inter-VariableFont_opsz,wght.ttf` was never tracked in the old repo. Add it if you want the menu font (otherwise raylib's default font is used).
- `docs/ARCADE_SETUP_PLAN.md` and `launch-arcade.sh` still mention the old `~/apps/raylib-quickstart` path in places. The launcher script is updated. The doc is unchanged apart from its new location.
