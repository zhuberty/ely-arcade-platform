#!/bin/bash
# Create a new arcade game repo skeleton in <platform parent>/ely-arcade-games/<name>.
# Usage: scripts/new-game.sh <game-name>      (lowercase letters, digits, hyphens)
set -e

NAME="$1"
if [ -z "$NAME" ]; then echo "Usage: $0 <game-name>"; exit 1; fi
if ! echo "$NAME" | grep -Eq '^[a-z0-9]+(-[a-z0-9]+)*$'; then
    echo "Error: name must be lowercase letters, digits and single hyphens (e.g. space-blasters)."; exit 1
fi

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
GAMES_DIR="$(cd "$ROOT/.." && pwd)/ely-arcade-games"
DEST="$GAMES_DIR/$NAME"
SDK_URL="git@github.com:zhuberty/ely-arcade-sdk.git"

if [ -e "$DEST" ]; then echo "Error: $DEST already exists."; exit 1; fi

mkdir -p "$GAMES_DIR"
mkdir -p "$DEST/build" "$DEST/src" "$DEST/resources" "$DEST/.vscode"
cd "$DEST"

cat > build/premake5.lua <<EOF
-- $NAME build script. Shared logic lives in the ely-arcade-sdk submodule (sdk/).
dofile("../sdk/premake/arcade_sdk.lua")

arcade.prepare_dirs()
arcade.workspace("$NAME")
arcade.raylib_project()
arcade.sdk_project("../sdk")
arcade.app_project("$NAME", "../src", "../sdk")
EOF

cat > src/main.cpp <<EOF
// $NAME
//
// Press ESC to exit back to the arcade menu.
// The menu launches this as a child process and waits for it to terminate.

#include "raylib.h"
#include "resource_dir.h"
#include "arcade_input.h"

int main(void)
{
    SetConfigFlags(FLAG_FULLSCREEN_MODE);
    InitWindow(0, 0, "$NAME");   // 0,0 = monitor native resolution
    SetExitKey(KEY_ESCAPE);      // ESC exits back to the arcade menu
    SetTargetFPS(60);

    SearchAndSetResourceDir("resources");

    while (!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(BLACK);
        DrawText("$NAME", 100, 100, 40, WHITE);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
EOF

cat > build.sh <<'EOF'
#!/bin/sh
set -e
cd build
../sdk/tools/premake/premake5 gmake
cd ..
make config=release_x64 "$@"
EOF
chmod +x build.sh

printf '@echo off\r\ncd build\r\n..\\sdk\\tools\\premake\\premake5.exe gmake || exit /b 1\r\ncd ..\r\nmingw32-make config=release_x64 %%*\r\n' > build.bat

cat > .gitignore <<'EOF'
bin/
obj/
build_files/
external/
Makefile
*.make
.vs/
*.sln
*.vcxproj*
compile_commands.json
EOF

cat > .gitattributes <<'EOF'
* text=auto eol=lf
*.sh text eol=lf
*.bat text eol=crlf
EOF

cat > .luarc.json <<'EOF'
{
  "runtime.version": "Lua 5.3",
  "workspace.library": ["sdk/premake/luals"],
  "workspace.ignoreDir": ["build/external", "bin", ".git"],
  "diagnostics.globals": ["arcade"]
}
EOF

cat > .vscode/c_cpp_properties.json <<'EOF'
{
    "configurations": [
        {
            "name": "Default",
            "includePath": [
                "${workspaceFolder}/sdk/include/**",
                "${workspaceFolder}/src/**",
                "${workspaceFolder}/build/external/raylib-master/src"
            ],
            "defines": [
                "_DEBUG",
                "UNICODE",
                "_UNICODE",
                "GRAPHICS_API_OPENGL_33",
                "PLATFORM_DESKTOP"
            ],
            "cStandard": "c17",
            "cppStandard": "c++17"
        }
    ],
    "version": 4
}
EOF

cat > .vscode/launch.json <<'EOF'
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "Debug",
            "type": "cppdbg",
            "request": "launch",
            "args": [],
            "stopAtEntry": false,
            "cwd": "${workspaceFolder}",
            "environment": [],
            "externalConsole": false,
            "MIMode": "gdb",
            "setupCommands": [
                {
                    "description": "Enable pretty-printing for gdb",
                    "text": "-enable-pretty-printing",
                    "ignoreFailures": false
                }
            ],
            "windows": {
                "program": "${workspaceFolder}/bin/Debug/${workspaceFolderBasename}.exe",
                "miDebuggerPath": "gdb.exe"
            },
            "osx": {
                "program": "${workspaceFolder}/bin/Debug/${workspaceFolderBasename}",
                "MIMode": "lldb"
            },
            "linux": {
                "program": "${workspaceFolder}/bin/Debug/${workspaceFolderBasename}",
                "miDebuggerPath": "/usr/bin/gdb"
            },
            "preLaunchTask": "build debug"
        },
        {
            "name": "Debug NoPremake",
            "type": "cppdbg",
            "request": "launch",
            "args": [],
            "stopAtEntry": false,
            "cwd": "${workspaceFolder}",
            "environment": [],
            "externalConsole": false,
            "MIMode": "gdb",
            "setupCommands": [
                {
                    "description": "Enable pretty-printing for gdb",
                    "text": "-enable-pretty-printing",
                    "ignoreFailures": false
                }
            ],
            "windows": {
                "program": "${workspaceFolder}/bin/Debug/${workspaceFolderBasename}.exe",
                "miDebuggerPath": "gdb.exe"
            },
            "osx": {
                "program": "${workspaceFolder}/bin/Debug/${workspaceFolderBasename}",
                "MIMode": "lldb"
            },
            "linux": {
                "program": "${workspaceFolder}/bin/Debug/${workspaceFolderBasename}",
                "miDebuggerPath": "/usr/bin/gdb"
            },
            "preLaunchTask": "build debug no premake"
        },
        {
            "name": "Run Release",
            "type": "cppdbg",
            "request": "launch",
            "args": [],
            "stopAtEntry": false,
            "cwd": "${workspaceFolder}",
            "environment": [],
            "externalConsole": false,
            "MIMode": "gdb",
            "windows": {
                "program": "${workspaceFolder}/bin/Release/${workspaceFolderBasename}.exe",
                "miDebuggerPath": "gdb.exe"
            },
            "osx": {
                "program": "${workspaceFolder}/bin/Release/${workspaceFolderBasename}",
                "MIMode": "lldb"
            },
            "linux": {
                "program": "${workspaceFolder}/bin/Release/${workspaceFolderBasename}",
                "miDebuggerPath": "/usr/bin/gdb"
            },
            "preLaunchTask": "build release"
        }
    ]
}
EOF

cat > .vscode/tasks.json <<'EOF'
{
    "version": "2.0.0",
    "tasks": [
        {
            "label": "premake",
            "type": "process",
            "command": "${workspaceFolder}/sdk/tools/premake/premake5",
            "args": ["gmake"],
            "options": { "cwd": "${workspaceFolder}/build" },
            "windows": { "command": "${workspaceFolder}/sdk/tools/premake/premake5.exe" },
            "osx": { "command": "${workspaceFolder}/sdk/tools/premake/premake5.osx" },
            "problemMatcher": []
        },
        {
            "label": "build debug",
            "type": "process",
            "command": "make",
            "args": ["config=debug_x64"],
            "options": { "cwd": "${workspaceFolder}" },
            "windows": { "command": "mingw32-make.exe" },
            "osx": { "args": ["config=debug_arm64"] },
            "group": { "kind": "build", "isDefault": true },
            "problemMatcher": ["$gcc"],
            "dependsOn": ["premake"]
        },
        {
            "label": "build debug no premake",
            "type": "process",
            "command": "make",
            "args": ["config=debug_x64"],
            "options": { "cwd": "${workspaceFolder}" },
            "windows": { "command": "mingw32-make.exe" },
            "osx": { "args": ["config=debug_arm64"] },
            "group": "build",
            "problemMatcher": ["$gcc"]
        },
        {
            "label": "build release",
            "type": "process",
            "command": "make",
            "args": ["config=release_x64"],
            "options": { "cwd": "${workspaceFolder}" },
            "windows": { "command": "mingw32-make.exe" },
            "osx": { "args": ["config=release_arm64"] },
            "group": "build",
            "problemMatcher": ["$gcc"],
            "dependsOn": ["premake"]
        }
    ]
}
EOF

touch resources/.gitkeep

git init -q
if ! git submodule add "$SDK_URL" sdk; then
    echo "Warning: could not add the SDK submodule (network/SSH?). Run this later in $DEST:"
    echo "  git submodule add $SDK_URL sdk"
fi

cat <<EOF

Created $DEST

Next steps:
  1. Build and test:  cd "$DEST" && ./build.sh && bin/Release/$NAME
  2. Create a GitHub repo, then:  git remote add origin <url> && git push -u origin main
  3. In the platform repo:  git submodule add <url> games/$NAME
                            git submodule update --init --recursive games/$NAME
  4. Add to GAMES[] in src/main.cpp:
       { "$NAME", "Description.", "games/$NAME/bin/Release/$NAME" GAME_EXE_EXT },
EOF
