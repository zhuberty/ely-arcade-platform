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

## Adding a game

1. Create the game repo from the same layout as `game-thick-cube` (`sdk/` submodule, `build/premake5.lua` calling `ely.app_project`, `resources/`).
2. `git submodule add <url> games/<name>`
3. Add an entry to `GAMES[]` in `src/main.cpp` (path is `games/<name>/bin/Release/<name>`).

## Updating

- Bump a game: `git -C games/<name> pull origin main`, then commit the new pointer here.
- Bump the SDK for a game: pull inside `games/<name>/sdk`, commit in the game repo, push, then bump the game here.

## Notes

- `resources/fonts/Inter-VariableFont_opsz,wght.ttf` was never tracked in the old repo. Add it if you want the menu font (otherwise raylib's default font is used).
- `docs/ARCADE_SETUP_PLAN.md` and `launch-arcade.sh` still mention the old `~/apps/raylib-quickstart` path in places. The launcher script is updated. The doc is unchanged apart from its new location.
