# Create a new arcade game repo skeleton in <platform parent>\ely-arcade-games\<name>.
# Usage: scripts\new-game.ps1 <game-name>      (lowercase letters, digits, hyphens)
param([Parameter(Mandatory = $true)][string]$Name)
$ErrorActionPreference = 'Stop'

if ($Name -cnotmatch '^[a-z0-9]+(-[a-z0-9]+)*$') {
    Write-Error "Name must be lowercase letters, digits and single hyphens (e.g. space-blasters)."
}

$Root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$GamesDir = Join-Path (Split-Path $Root -Parent) 'ely-arcade-games'
$Dest = Join-Path $GamesDir $Name
$SdkUrl = 'git@github.com:zhuberty/ely-arcade-sdk.git'

if (Test-Path $Dest) { Write-Error "$Dest already exists." }

New-Item -ItemType Directory -Force -Path $GamesDir | Out-Null
foreach ($d in 'build', 'src', 'resources', '.vscode') {
    New-Item -ItemType Directory -Force -Path (Join-Path $Dest $d) | Out-Null
}

# Write files with LF endings (CRLF for .bat) and no BOM, matching .gitattributes.
function Write-File($RelPath, $Content) {
    $full = Join-Path $Dest $RelPath
    $text = $Content -replace "`r`n", "`n"
    if ($RelPath -like '*.bat') { $text = $text -replace "`n", "`r`n" }
    [System.IO.File]::WriteAllText($full, $text, (New-Object System.Text.UTF8Encoding($false)))
}

Write-File 'build\premake5.lua' @"
-- $Name build script. Shared logic lives in the ely-arcade-sdk submodule (sdk/).
dofile("../sdk/premake/arcade_sdk.lua")

arcade.prepare_dirs()
arcade.workspace("$Name")
arcade.raylib_project()
arcade.sdk_project("../sdk")
arcade.app_project("$Name", "../src", "../sdk")
"@

Write-File 'src\main.cpp' @"
// $Name
//
// Press ESC to exit back to the arcade menu.
// The menu launches this as a child process and waits for it to terminate.

#include "raylib.h"
#include "resource_dir.h"
#include "arcade_input.h"

int main(void)
{
    SetConfigFlags(FLAG_FULLSCREEN_MODE);
    InitWindow(0, 0, "$Name");   // 0,0 = monitor native resolution
    SetExitKey(KEY_ESCAPE);      // ESC exits back to the arcade menu
    SetTargetFPS(60);

    SearchAndSetResourceDir("resources");

    while (!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(BLACK);
        DrawText("$Name", 100, 100, 40, WHITE);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
"@

Write-File 'build.sh' @'
#!/bin/sh
set -e
cd build
../sdk/tools/premake/premake5 gmake
cd ..
make config=release_x64 "$@"
'@

Write-File 'build.bat' @'
@echo off
cd build
..\sdk\tools\premake\premake5.exe gmake || exit /b 1
cd ..
mingw32-make config=release_x64 %*
'@

Write-File '.gitignore' @'
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
'@

Write-File '.gitattributes' @'
* text=auto eol=lf
*.sh text eol=lf
*.bat text eol=crlf
'@

Write-File '.luarc.json' @'
{
  "runtime.version": "Lua 5.3",
  "workspace.library": ["sdk/premake/luals"],
  "workspace.ignoreDir": ["build/external", "bin", ".git"],
  "diagnostics.globals": ["arcade"]
}
'@

Write-File '.vscode\c_cpp_properties.json' @'
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
'@

Write-File '.vscode\launch.json' @'
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
'@

Write-File '.vscode\tasks.json' @'
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
'@

Write-File 'resources\.gitkeep' ''

Push-Location $Dest
try {
    git init -q
    git submodule add $SdkUrl sdk
    if ($LASTEXITCODE -ne 0) {
        Write-Warning "Could not add the SDK submodule (network/SSH?). Run this later in ${Dest}:"
        Write-Host "  git submodule add $SdkUrl sdk"
    }
} finally {
    Pop-Location
}

Write-Host @"

Created $Dest

Next steps:
  1. Build and test:  cd "$Dest"; .\build.bat; bin\Release\$Name.exe
  2. Create a GitHub repo, then:  git remote add origin <url>; git push -u origin main
  3. In the platform repo:  git submodule add <url> games/$Name
                            git submodule update --init --recursive games/$Name
  4. Add to GAMES[] in src/main.cpp:
       { "$Name", "Description.", "games/$Name/bin/Release/$Name" GAME_EXE_EXT },
"@
