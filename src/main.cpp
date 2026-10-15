// Arcade Menu -- src/main.cpp
//
// Top-level launcher booted directly from LightDM.
// Shows a navigable game list (joystick Up/Down to choose, any encoder button to launch;
// the keyboard's W/S or arrow keys and Enter work too).
// Each game is a separate binary launched as a child process via fork/exec.
// The menu waits (waitpid) until the child exits, then returns to the list.
// ESC inside a game exits that game binary and returns here automatically.
//
// Build:  see scripts/build-all.sh (menu + every game submodule)
// Output: bin/Release/ely-arcade-platform  (started by launch-arcade.sh)

#include "raylib.h"
#include "arcade_input.h"
#include "settings.h"
#include "ui.h"

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
    // Heading shown above this game ("" = none). A heading is drawn whenever an entry's
    // section differs from the previous entry's, so keep games of one section together.
    const char* section;
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
        "",
        "Snake",
        "A snake game for one or two players.",
        // The game's repo and binary are still called edibles-raylib.
        "games/edibles-raylib/bin/Release/edibles-raylib" GAME_EXE_EXT
    },
    {
        "Unfinished Games",
        "Wizards of the West",
        "Explore the wizard's world.",
        // The folder is ely-arcade-wizards-1, but the binary is named wizards1.
        "games/ely-arcade-wizards-1/bin/Release/wizards1" GAME_EXE_EXT
    },
};
static const int GAME_COUNT = (int)(sizeof(GAMES) / sizeof(GAMES[0]));

// The list the player scrolls through is the games followed by one more entry: Settings (Wi-Fi and
// software updates, see settings.h). Its index is GAME_COUNT.
static const int ENTRY_COUNT = GAME_COUNT + 1;

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
// Confirm ("launch the selected game")
// The SDK has no Confirm action: its actions are only Up/Down/Left/Right, and the
// encoder buttons are deliberately unassigned (each program decides what they
// mean). In this menu ANY of the 8 buttons on either arcade encoder launches the
// selected game. For development without the cabinet: Enter, Space or the keypad
// Enter on the keyboard, and the A button on gamepads 0 and 1.
// ---------------------------------------------------------------------------

static bool ConfirmPressed()
{
    for (int button = 0; button < arcade::kEncoderButtonCount; ++button)
        if (arcade::IsButtonPressed(arcade::Player::Any, button))
            return true;

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) || IsKeyPressed(KEY_SPACE))
        return true;

    return IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN) ||
           IsGamepadButtonPressed(1, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
}

// ---------------------------------------------------------------------------
// Pulsing highlight alpha -- sin wave, full cycle ~2 s
// ---------------------------------------------------------------------------

static float PulseAlpha(float t)
{
    return 0.55f + 0.45f * sinf(t * 3.14159f);
}

// ---------------------------------------------------------------------------
// Fonts
// The menu font (resources/fonts/Inter-VariableFont_opsz,wght.ttf) is not tracked in git, so
// it is often missing. A failed LoadFontEx returns an EMPTY font: MeasureTextEx then returns
// 0 x 0 while DrawTextEx silently draws with raylib's built-in font, so everything laid out
// from the measured size (e.g. the line under the title) lands on top of the text. Fall back
// to the built-in font explicitly, so measuring and drawing always use the same font.
// ---------------------------------------------------------------------------

static const char* MENU_FONT = "resources/fonts/Inter-VariableFont_opsz,wght.ttf";

static Font LoadMenuFont(int size)
{
    Font font = LoadFontEx(MENU_FONT, size, 0, 0);
    if (font.texture.id == 0)
        return GetFontDefault();
    return font;
}

static void UnloadMenuFont(Font font)
{
    if (font.texture.id != GetFontDefault().texture.id)
        UnloadFont(font);
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
    Font titleFont = LoadMenuFont(72);
    Font itemFont  = LoadMenuFont(36);
    Font hintFont  = LoadMenuFont(24);
    Font sectFont  = LoadMenuFont(28);

    // The settings screens draw with the same fonts, and look in the platform root for scripts/.
    ui::SetFonts(titleFont, itemFont, hintFont);
    settings::Init(GetWorkingDirectory());

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
    Color headRule = { 255, 200,  50,  90 };  // faint gold rule beside section headings

    while (!WindowShouldClose())
    {
        float now = (float)GetTime();

        // Input. UpdateInput() must run exactly once per frame, before any query: it is
        // what makes "pressed" work for the encoder buttons.
        arcade::UpdateInput();

        settings::Update();   // network and update status, polled in the background

        if (arcade::IsActionPressed(arcade::Player::Any, arcade::Action::Down))
            selectedIndex = (selectedIndex + 1) % ENTRY_COUNT;

        if (arcade::IsActionPressed(arcade::Player::Any, arcade::Action::Up))
            selectedIndex = (selectedIndex - 1 + ENTRY_COUNT) % ENTRY_COUNT;

        if (ConfirmPressed())
        {
            if (selectedIndex == GAME_COUNT)
                settings::Run();                  // returns when the player leaves the settings
            else
                LaunchGame(GAMES[selectedIndex]);
            // After the game (or the settings) exits, execution resumes here and the loop continues
        }

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

        // Decorative separator line under the title, 24 px below the text's bottom edge. The
        // title is measured with the same font it is drawn with (see LoadMenuFont), so the
        // line can no longer land on top of the text.
        int lineY = (int)(titlePos.y + titleSize.y + 24);
        DrawRectangle(W/2 - 200, lineY, 400, 3, titleCol);

        // Game list layout. Each game is one row. A heading row is added above the first
        // game of every named section (GameEntry::section); rows are placed top to bottom.
        const int ITEM_H     = 90;
        const int HEADING_H  = 64;
        const int LIST_TOP   = lineY + 40;
        const int ITEM_PAD_X = 60;
        const int ITEM_W     = W - ITEM_PAD_X * 2;

        int rowY = LIST_TOP;
        const char* lastSection = "";
        for (int i = 0; i < GAME_COUNT; ++i)
        {
            // Section heading: centred text with a thin rule on each side.
            if (strcmp(GAMES[i].section, lastSection) != 0)
            {
                lastSection = GAMES[i].section;
                if (lastSection[0] != '\0')
                {
                    rowY += 20;   // breathing room between the previous game's box and the heading
                    Vector2 hs = MeasureTextEx(sectFont, lastSection, 28, 2);
                    float hx   = (W - hs.x) * 0.5f;
                    int midY   = rowY + (int)(hs.y * 0.5f);
                    DrawTextEx(sectFont, lastSection, { hx, (float)rowY }, 28, 2, titleCol);
                    DrawRectangle(ITEM_PAD_X, midY, (int)(hx - 20 - ITEM_PAD_X), 1, headRule);
                    DrawRectangle((int)(hx + hs.x + 20), midY,
                                  (int)(W - ITEM_PAD_X - (hx + hs.x + 20)), 1, headRule);
                    rowY += HEADING_H;
                }
            }

            int itemY  = rowY;
            rowY      += ITEM_H;
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

        // The last entry: Settings
        {
            int itemY  = rowY;
            bool isSel = (selectedIndex == GAME_COUNT);
            if (isSel)
            {
                float a = PulseAlpha(now);
                Color boxCol    = { hlBox.r, hlBox.g, hlBox.b, (unsigned char)(a * 60) };
                Color borderCol = { hlBox.r, hlBox.g, hlBox.b, (unsigned char)(a * 200) };
                DrawRectangle(ITEM_PAD_X, itemY, ITEM_W, ITEM_H - 10, boxCol);
                DrawRectangleLines(ITEM_PAD_X, itemY, ITEM_W, ITEM_H - 10, borderCol);
                DrawTextEx(itemFont, ">", { (float)(ITEM_PAD_X + 10), (float)(itemY + 14) }, 36, 1, hlBox);
            }
            Color nameCol = isSel ? itemSel : itemNorm;
            DrawTextEx(itemFont, "Settings", { (float)(ITEM_PAD_X + 50), (float)(itemY + 10) }, 36, 1, nameCol);
            Color descCol = { nameCol.r, nameCol.g, nameCol.b, 180 };
            DrawTextEx(hintFont, "Wi-Fi and software updates.", { (float)(ITEM_PAD_X + 50), (float)(itemY + 52) }, 24, 1, descCol);
        }

        // Network / update status, top right
        settings::DrawStatus(W);

        // Footer control hints
        const char* hintText = "JOYSTICK   choose       ANY BUTTON   select";
        Vector2 hintSize = MeasureTextEx(hintFont, hintText, 24, 1);
        DrawTextEx(hintFont, hintText,
                   { (W - hintSize.x) * 0.5f, (float)(H - 50) },
                   24, 1, hintCol);

        EndDrawing();
    }

    UnloadMenuFont(sectFont);
    UnloadMenuFont(hintFont);
    UnloadMenuFont(itemFont);
    UnloadMenuFont(titleFont);
    CloseWindow();
    return 0;
}

