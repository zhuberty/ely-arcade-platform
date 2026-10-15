// Arcade Menu -- src/ui.cpp  (see ui.h)

#include "ui.h"
#include "arcade_input.h"

#include <algorithm>
#include <cmath>

namespace ui
{

const Color kGold    = { 255, 200,  50, 255 };
const Color kText    = { 200, 200, 220, 255 };
const Color kTextSel = { 255, 255, 255, 255 };
const Color kDim     = { 130, 130, 150, 255 };
const Color kBlue    = {  80, 160, 255, 255 };
const Color kGreen   = { 110, 220, 130, 255 };
const Color kRed     = { 255, 110, 110, 255 };
const Color kAmber   = { 255, 170,  60, 255 };

static Font g_title, g_item, g_hint;
static bool g_haveFonts = false;

void SetFonts(Font title, Font item, Font hint)
{
    g_title = title;
    g_item = item;
    g_hint = hint;
    g_haveFonts = true;
}

Font TitleFont() { return g_haveFonts ? g_title : GetFontDefault(); }
Font ItemFont()  { return g_haveFonts ? g_item  : GetFontDefault(); }
Font HintFont()  { return g_haveFonts ? g_hint  : GetFontDefault(); }

double Now() { return GetTime(); }

void Settle()
{
    BeginDrawing();
    DrawRectangleGradientV(0, 0, GetScreenWidth(), GetScreenHeight(), Color{ 10, 10, 28, 255 }, Color{ 25, 25, 55, 255 });
    EndDrawing();
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

namespace
{

// Turns "is held" into a press event followed by auto-repeat.
struct Repeater
{
    bool held = false;
    double since = 0.0;
    double last = 0.0;

    bool Fire(bool down, double now)
    {
        if (!down) { held = false; return false; }
        if (!held) { held = true; since = now; last = now; return true; }
        if (now - since > 0.45 && now - last > 0.09) { last = now; return true; }
        return false;
    }
};

Repeater g_rep[4];     // Up, Down, Left, Right

bool EncoderButtonPressed()
{
    for (int b = 0; b < arcade::kEncoderButtonCount; ++b)
        if (arcade::IsButtonPressed(arcade::Player::Any, b))
            return true;
    return false;
}

bool GamepadPressed(int button)
{
    return IsGamepadButtonPressed(0, button) || IsGamepadButtonPressed(1, button);
}

bool GamepadDown(int button)
{
    return IsGamepadButtonDown(0, button) || IsGamepadButtonDown(1, button);
}

} // namespace

bool Frame(Nav& nav, bool textMode)
{
    nav = Nav::None;
    if (WindowShouldClose()) return false;

    arcade::UpdateInput();
    const double now = Now();

    using A = arcade::Action;
    const A actions[4] = { A::Up, A::Down, A::Left, A::Right };
    bool dir[4];
    if (textMode)
    {
        // Letters are text here: only the encoder joysticks, the arrow keys and the d-pad move the cursor.
        const int keys[4] = { KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT };
        const int pads[4] = { GAMEPAD_BUTTON_LEFT_FACE_UP, GAMEPAD_BUTTON_LEFT_FACE_DOWN,
                              GAMEPAD_BUTTON_LEFT_FACE_LEFT, GAMEPAD_BUTTON_LEFT_FACE_RIGHT };
        for (int i = 0; i < 4; ++i)
            dir[i] = arcade::IsEncoderDirectionDown(arcade::Player::Any, actions[i]) ||
                     IsKeyDown(keys[i]) || GamepadDown(pads[i]);
    }
    else
    {
        for (int i = 0; i < 4; ++i)
            dir[i] = arcade::IsActionDown(arcade::Player::Any, actions[i]);
    }
    bool fire[4];
    for (int i = 0; i < 4; ++i) fire[i] = g_rep[i].Fire(dir[i], now);

    bool back = IsKeyPressed(KEY_ESCAPE) || GamepadPressed(GAMEPAD_BUTTON_RIGHT_FACE_RIGHT);
    bool select = EncoderButtonPressed() || GamepadPressed(GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
    if (!textMode)
    {
        back = back || IsKeyPressed(KEY_BACKSPACE);
        select = select || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) || IsKeyPressed(KEY_SPACE);
    }

    if (back)         nav = Nav::Back;
    else if (select)  nav = Nav::Select;
    else if (fire[0]) nav = Nav::Up;
    else if (fire[1]) nav = Nav::Down;
    else if (fire[2]) nav = Nav::Left;
    else if (fire[3]) nav = Nav::Right;
    return true;
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

int BeginScreen(const char* title)
{
    BeginDrawing();
    const int W = GetScreenWidth();
    const int H = GetScreenHeight();
    DrawRectangleGradientV(0, 0, W, H, Color{ 10, 10, 28, 255 }, Color{ 25, 25, 55, 255 });

    Font f = TitleFont();
    Vector2 size = MeasureTextEx(f, title, 56, 2);
    Vector2 pos = { (W - size.x) * 0.5f, 40.0f };
    DrawTextEx(f, title, pos, 56, 2, kGold);
    int lineY = (int)(pos.y + size.y + 16);
    DrawRectangle(W / 2 - 200, lineY, 400, 3, kGold);
    return lineY + 36;
}

void EndScreen(const char* footer)
{
    const int W = GetScreenWidth();
    const int H = GetScreenHeight();
    Font f = HintFont();
    Vector2 size = MeasureTextEx(f, footer, 24, 1);
    DrawTextEx(f, footer, { (W - size.x) * 0.5f, (float)(H - 50) }, 24, 1, kDim);
    EndDrawing();
}

static std::string Fit(Font font, const std::string& text, int size, int maxWidth)
{
    if (maxWidth <= 0) return "";
    if ((int)MeasureTextEx(font, text.c_str(), (float)size, 1).x <= maxWidth) return text;
    std::string s = text;
    while (!s.empty())
    {
        s.pop_back();
        std::string t = s + "...";
        if ((int)MeasureTextEx(font, t.c_str(), (float)size, 1).x <= maxWidth) return t;
    }
    return "";
}

void DrawFit(const std::string& text, int x, int y, int size, Color color, int maxWidth, bool item)
{
    Font f = item ? ItemFont() : HintFont();
    DrawTextEx(f, Fit(f, text, size, maxWidth).c_str(), { (float)x, (float)y }, (float)size, 1, color);
}

void DrawCentered(const std::string& text, int y, int size, Color color, bool item)
{
    Font f = item ? ItemFont() : HintFont();
    std::string t = Fit(f, text, size, GetScreenWidth() - 120);
    Vector2 s = MeasureTextEx(f, t.c_str(), (float)size, 1);
    DrawTextEx(f, t.c_str(), { (GetScreenWidth() - s.x) * 0.5f, (float)y }, (float)size, 1, color);
}

void DrawSignal(int rightX, int y, int signal, bool selected)
{
    const int bars = signal >= 75 ? 4 : signal >= 50 ? 3 : signal >= 25 ? 2 : 1;
    for (int i = 0; i < 4; ++i)
    {
        int h = 8 + i * 8;
        Color c = i < bars ? (selected ? kTextSel : kText) : Color{ 70, 70, 95, 255 };
        DrawRectangle(rightX - (4 - i) * 12, y + 46 - h, 8, h, c);
    }
}

void DrawRow(int y, const std::string& label, const std::string& detail, bool selected, bool dimmed)
{
    const int W = GetScreenWidth();
    const int padX = 60;
    const int boxW = W - padX * 2;
    const int boxH = kRowPitch - 8;

    if (selected)
    {
        float a = 0.55f + 0.45f * sinf((float)Now() * 3.14159f);
        DrawRectangle(padX, y, boxW, boxH, Color{ kBlue.r, kBlue.g, kBlue.b, (unsigned char)(a * 60) });
        DrawRectangleLines(padX, y, boxW, boxH, Color{ kBlue.r, kBlue.g, kBlue.b, (unsigned char)(a * 200) });
        DrawTextEx(ItemFont(), ">", { (float)(padX + 10), (float)(y + 12) }, 34, 1, kBlue);
    }

    Color col = dimmed ? Color{ 110, 110, 130, 255 } : (selected ? kTextSel : kText);
    int detailW = detail.empty() ? 0 : (int)MeasureTextEx(HintFont(), detail.c_str(), 24, 1).x;
    int labelMax = boxW - 50 - 30 - (detailW > 0 ? detailW + 40 : 0);
    DrawFit(label, padX + 50, y + 10, 34, col, labelMax, true);
    if (!detail.empty())
        DrawTextEx(HintFont(), detail.c_str(), { (float)(padX + boxW - 20 - detailW), (float)(y + 20) }, 24, 1,
                   Color{ col.r, col.g, col.b, 180 });
}

std::string Working(const std::string& text)
{
    int dots = (int)(Now() * 3.0) % 4;
    return text + std::string((size_t)dots, '.');
}

} // namespace ui
