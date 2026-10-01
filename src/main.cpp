// Arcade Menu -- src/main.cpp
//
// Top-level launcher booted directly from LightDM.
// Shows a navigable game list (Up/Down + Enter to launch).
// Each game is a separate binary launched as a child process via fork/exec.
// The menu waits (waitpid) until the child exits, then returns to the list.
// ESC inside a game exits that game binary and returns here automatically.
//
// Build:  see scripts/build-all.sh (menu + every game submodule)
// Output: bin/Release/ely-arcade-platform  (started by launch-arcade.sh)

#include "raylib.h"
#include "arcade_input.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#if defined(_WIN32)
    #include <process.h>   // _spawnl
#else
    #include <unistd.h>    // fork, execv, chdir
    #include <sys/wait.h>  // waitpid
    #include <sys/types.h>
#endif

// ---------------------------------------------------------------------------
// Game catalogue
// ---------------------------------------------------------------------------

struct GameEntry
{
    const char* title;
    const char* description;
    const char* binaryPath;  // absolute path to the game executable
};

// Each game is its own repo, checked out as a git submodule under
// <platform root>/games/<dir>/ and built there (see scripts/build-all.*).
// binaryPath is relative to the platform root. The game is launched with its
// own directory as the working directory so it finds its resources/ folder.
#if defined(_WIN32)
    #define GAME_EXE_EXT ".exe"
#else
    #define GAME_EXE_EXT ""
#endif

static const GameEntry GAMES[] =
{
    {
        "Thick Cube",
        "A 3D cube with holes on every face.",
        "games/game-thick-cube/bin/Release/game-thick-cube" GAME_EXE_EXT
    },
    {
        "Edibles",
        "A snake game for one or two players.",
        "games/edibles-raylib/bin/Release/edibles-raylib" GAME_EXE_EXT
    },
    {
        "Map Browser",
        "Preview a Tiled-based game map.",
        "games/ely-arcade-map-browser/bin/Release/ely-arcade-map-browser" GAME_EXE_EXT
    },
};
static const int GAME_COUNT = (int)(sizeof(GAMES) / sizeof(GAMES[0]));

// ---------------------------------------------------------------------------
// Launch a game as a child process; block until it exits.
// Menu window is minimised while the game runs so the game's fullscreen
// context can claim the display without conflicting GL contexts.
// ---------------------------------------------------------------------------

static void LaunchGame(const GameEntry& game)
{
    MinimizeWindow();  // hide menu while game runs

    // The menu's working directory is the platform root (set in main()).
    const char* root = GetWorkingDirectory();
    char fullPath[1024];
    snprintf(fullPath, sizeof(fullPath), "%s/%s", root, game.binaryPath);

    // Game's own directory = two levels above "bin/Release/<exe>".
    char gameDir[1024];
    snprintf(gameDir, sizeof(gameDir), "%s", fullPath);
    for (int i = 0; i < 3; ++i)
    {
        char* slash = strrchr(gameDir, '/');
        if (slash) *slash = '\0';
    }

#if defined(_WIN32)
    // Windows (dev): run from the game's dir, block until it exits, then come back.
    ChangeDirectory(gameDir);
    intptr_t result = _spawnl(_P_WAIT, fullPath, fullPath, (const char*)nullptr);
    ChangeDirectory(root);
    if (result == -1)
    {
        TraceLog(LOG_ERROR, "MENU: _spawnl() failed to launch \"%s\"", fullPath);
    }
#else
    pid_t pid = fork();
    if (pid == 0)
    {
        // Child: cd to the game's own dir so it finds resources/ by relative path
        if (chdir(gameDir) != 0) perror("chdir failed");
        char* const argv[] = { fullPath, nullptr };
        execv(fullPath, argv);
        perror("execv failed");
        _exit(1);
    }
    else if (pid > 0)
    {
        int status = 0;
        waitpid(pid, &status, 0);  // block until game process exits
    }
    else
    {
        TraceLog(LOG_ERROR, "MENU: fork() failed, cannot launch game");
    }
#endif

    // Restore menu fullscreen after the game has released the display
    RestoreWindow();
    SetWindowState(FLAG_FULLSCREEN_MODE);
}

// ---------------------------------------------------------------------------
// Pulsing highlight alpha -- sin wave, full cycle ~2 s
// ---------------------------------------------------------------------------

static float PulseAlpha(float t)
{
    return 0.55f + 0.45f * sinf(t * 3.14159f);
}


// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main(void)
{
    // Window / display setup
    SetConfigFlags(FLAG_FULLSCREEN_MODE);
    InitWindow(0, 0, "Arcade");   // 0,0 -> monitor native res (1920x1080)
    SetExitKey(KEY_NULL);         // Disable ESC on the menu itself
    SetTargetFPS(60);

    // Platform root = two levels above bin/Release/. Make it the CWD so that
    // resources/ and games/ resolve the same no matter how the menu was started.
    ChangeDirectory(GetApplicationDirectory());
    ChangeDirectory("../..");

    // Load font at three sizes (falls back to raylib's default font if missing).
    Font titleFont = LoadFontEx("resources/fonts/Inter-VariableFont_opsz,wght.ttf", 72, 0, 0);
    Font itemFont  = LoadFontEx("resources/fonts/Inter-VariableFont_opsz,wght.ttf", 36, 0, 0);
    Font hintFont  = LoadFontEx("resources/fonts/Inter-VariableFont_opsz,wght.ttf", 24, 0, 0);

    // State
    int selectedIndex = 0;

    // Colour palette
    Color bgTop    = {  10,  10,  28, 255 };  // deep navy
    Color bgBottom = {  25,  25,  55, 255 };  // slightly lighter navy
    Color titleCol = { 255, 200,  50, 255 };  // arcade gold
    Color itemNorm = { 200, 200, 220, 255 };  // soft white
    Color itemSel  = { 255, 255, 255, 255 };  // bright white (selected)
    Color hintCol  = { 130, 130, 150, 255 };  // dim grey footer
    Color hlBox    = {  80, 160, 255, 255 };  // blue selection box

    while (!WindowShouldClose())
    {
        float now = (float)GetTime();

        // Input
        if (arcade::IsActionPressed(arcade::Player::Any, arcade::Action::Down))
            selectedIndex = (selectedIndex + 1) % GAME_COUNT;

        if (arcade::IsActionPressed(arcade::Player::Any, arcade::Action::Up))
            selectedIndex = (selectedIndex - 1 + GAME_COUNT) % GAME_COUNT;

        if (arcade::IsActionPressed(arcade::Player::Any, arcade::Action::Confirm))
            LaunchGame(GAMES[selectedIndex]);
            // After game exits, execution resumes here and the loop continues

        // Draw
        BeginDrawing();

        int W = GetScreenWidth();
        int H = GetScreenHeight();

        // Gradient background (row by row for a smooth top-to-bottom fade)
        for (int y = 0; y < H; ++y)
        {
            float t = (float)y / (float)H;
            Color c = {
                (unsigned char)(bgTop.r + t * (bgBottom.r - bgTop.r)),
                (unsigned char)(bgTop.g + t * (bgBottom.g - bgTop.g)),
                (unsigned char)(bgTop.b + t * (bgBottom.b - bgTop.b)),
                255
            };
            DrawLine(0, y, W, y, c);
        }

        // Title
        const char* titleText = "ARCADE";
        Vector2 titleSize = MeasureTextEx(titleFont, titleText, 72, 2);
        Vector2 titlePos  = { (W - titleSize.x) * 0.5f, 80.0f };
        DrawTextEx(titleFont, titleText, titlePos, 72, 2, titleCol);

        // Decorative separator line under title
        int lineY = (int)(titlePos.y + titleSize.y + 20);
        DrawRectangle(W/2 - 200, lineY, 400, 3, titleCol);

        // Game list layout constants
        const int ITEM_H     = 90;
        const int LIST_TOP   = lineY + 40;
        const int ITEM_PAD_X = 60;
        const int ITEM_W     = W - ITEM_PAD_X * 2;

        for (int i = 0; i < GAME_COUNT; ++i)
        {
            int itemY  = LIST_TOP + i * ITEM_H;
            bool isSel = (i == selectedIndex);

            // Selection highlight box
            if (isSel)
            {
                float a = PulseAlpha(now);
                Color boxCol    = { hlBox.r, hlBox.g, hlBox.b, (unsigned char)(a * 60) };
                Color borderCol = { hlBox.r, hlBox.g, hlBox.b, (unsigned char)(a * 200) };
                DrawRectangle(ITEM_PAD_X, itemY, ITEM_W, ITEM_H - 10, boxCol);
                DrawRectangleLines(ITEM_PAD_X, itemY, ITEM_W, ITEM_H - 10, borderCol);
            }

            // Arrow indicator on the selected item
            if (isSel)
                DrawTextEx(itemFont, ">",
                           { (float)(ITEM_PAD_X + 10), (float)(itemY + 14) },
                           36, 1, hlBox);

            // Game name
            Color nameCol = isSel ? itemSel : itemNorm;
            DrawTextEx(itemFont, GAMES[i].title,
                       { (float)(ITEM_PAD_X + 50), (float)(itemY + 10) },
                       36, 1, nameCol);

            // Game description (smaller, semi-transparent)
            Color descCol = { nameCol.r, nameCol.g, nameCol.b, 180 };
            DrawTextEx(hintFont, GAMES[i].description,
                       { (float)(ITEM_PAD_X + 50), (float)(itemY + 52) },
                       24, 1, descCol);
        }

        // Footer control hints
        const char* hintText = "UP / DOWN   navigate       ENTER   launch game";
        Vector2 hintSize = MeasureTextEx(hintFont, hintText, 24, 1);
        DrawTextEx(hintFont, hintText,
                   { (W - hintSize.x) * 0.5f, (float)(H - 50) },
                   24, 1, hintCol);

        EndDrawing();
    }

    UnloadFont(hintFont);
    UnloadFont(itemFont);
    UnloadFont(titleFont);
    CloseWindow();
    return 0;
}

