// Arcade Menu -- src/keyboard.cpp  (see keyboard.h)

#include "keyboard.h"
#include "ui.h"

#include <algorithm>
#include <cmath>

namespace ui
{

namespace
{

// Three pages of four rows of ten keys, then a row of special keys.
const char* const kPages[3][4] = {
    { "1234567890", "qwertyuiop", "asdfghjkl.", "zxcvbnm,-_" },
    { "1234567890", "QWERTYUIOP", "ASDFGHJKL.", "ZXCVBNM,-_" },
    { "1234567890", "!@#$%^&*()", "-_=+[]{};:", "'\"\\|/<>?,." },
};
const int kKeyRows = 4;
const int kKeyCols = 10;
const int kRows = kKeyRows + 1;           // + the special row

enum Special { kShift, kSymbols, kSpace, kDelete, kShow, kDone, kSpecialCount };
// Width of each special key, in key widths (they add up to kKeyCols).
const float kSpecialWidth[kSpecialCount] = { 1.5f, 1.5f, 2.5f, 1.5f, 1.5f, 1.5f };

int ColsIn(int row) { return row < kKeyRows ? kKeyCols : (int)kSpecialCount; }

// Left edge and width of key `col` of `row`, in key widths (0..kKeyCols).
void KeySpan(int row, int col, float& left, float& width)
{
    if (row < kKeyRows) { left = (float)col; width = 1.0f; return; }
    left = 0.0f;
    for (int i = 0; i < col; ++i) left += kSpecialWidth[i];
    width = kSpecialWidth[col];
}

// The key of `row` that lies under horizontal position x (in key widths).
int ColAt(int row, float x)
{
    for (int c = 0; c < ColsIn(row); ++c)
    {
        float left, width;
        KeySpan(row, c, left, width);
        if (x < left + width) return c;
    }
    return ColsIn(row) - 1;
}

// The end of `text` that fits in maxWidth (the cursor end is the one that matters while typing).
std::string Tail(const std::string& text, int maxWidth, float size)
{
    std::string s = text;
    while (s.size() > 1 && MeasureTextEx(ItemFont(), s.c_str(), size, 1).x > maxWidth)
        s.erase(0, 1);
    return s;
}

} // namespace

bool TextEntry(const std::string& title, const std::string& prompt, std::string& text,
               bool secret, size_t minLen, size_t maxLen)
{
    std::string value = text;
    int page = 0;                // 0 lower case, 1 upper case, 2 symbols
    int row = 1, col = 0;
    bool show = !secret;
    std::string warning;
    double warningUntil = 0.0;

    Settle();
    Nav nav;
    while (Frame(nav, true))
    {
        // --- physical keyboard -------------------------------------------------------------------------
        for (int ch = GetCharPressed(); ch != 0; ch = GetCharPressed())
            if (ch >= 32 && ch <= 126 && value.size() < maxLen) value += (char)ch;
        if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && !value.empty()) value.pop_back();

        bool done = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER);

        // --- joystick / buttons ------------------------------------------------------------------------
        if (nav == Nav::Back) return false;
        if (nav == Nav::Left)  col = (col + ColsIn(row) - 1) % ColsIn(row);
        if (nav == Nav::Right) col = (col + 1) % ColsIn(row);
        if (nav == Nav::Up || nav == Nav::Down)
        {
            float left, width;
            KeySpan(row, col, left, width);
            float centre = left + width * 0.5f;
            row = (row + kRows + (nav == Nav::Down ? 1 : -1)) % kRows;
            col = ColAt(row, centre);
        }
        if (nav == Nav::Select)
        {
            if (row < kKeyRows)
            {
                if (value.size() < maxLen) value += kPages[page][row][col];
            }
            else switch (col)
            {
                case kShift:   page = (page == 0) ? 1 : 0; break;
                case kSymbols: page = (page == 2) ? 0 : 2; break;
                case kSpace:   if (value.size() < maxLen) value += ' '; break;
                case kDelete:  if (!value.empty()) value.pop_back(); break;
                case kShow:    show = !show; break;
                case kDone:    done = true; break;
            }
        }
        if (done)
        {
            if (value.size() >= minLen) { text = value; return true; }
            warning = minLen == 1 ? std::string("Type at least one character")
                                  : "At least " + std::to_string(minLen) + " characters are needed";
            warningUntil = Now() + 3.0;
        }

        // --- draw --------------------------------------------------------------------------------------
        const int W = GetScreenWidth();
        const int H = GetScreenHeight();
        int y = BeginScreen(title.c_str());
        DrawCentered(prompt, y, 28, kText);
        y += 44;

        // The text field.
        const int fieldW = std::min(W - 160, 1100);
        const int fieldX = (W - fieldW) / 2;
        DrawRectangle(fieldX, y, fieldW, 70, Color{ 8, 8, 22, 255 });
        DrawRectangleLines(fieldX, y, fieldW, 70, kBlue);
        std::string shown = show ? value : std::string(value.size(), '*');
        if (((int)(Now() * 2.0)) % 2 == 0) shown += "_";
        shown = Tail(shown, fieldW - 40, 40);
        DrawTextEx(ItemFont(), shown.c_str(), { (float)(fieldX + 20), (float)(y + 14) }, 40, 1, kTextSel);
        const bool warn = Now() < warningUntil;
        std::string count = std::to_string(value.size()) + " / " + std::to_string(maxLen);
        DrawFit(warn ? warning : count, fieldX, y + 78, 24, warn ? kAmber : kDim, fieldW);
        y += 120;

        // The keys.
        const int cell = std::max(40, std::min((W - 200) / kKeyCols, (H - y - 110) / kRows));
        const int gridX = (W - cell * kKeyCols) / 2;
        const char* labels[kSpecialCount] = { page == 0 ? "ABC" : "abc", page == 2 ? "abc" : "#+=",
                                              "SPACE", "DEL", show ? "HIDE" : "SHOW", "OK" };
        for (int r = 0; r < kRows; ++r)
        {
            for (int c = 0; c < ColsIn(r); ++c)
            {
                float left, width;
                KeySpan(r, c, left, width);
                Rectangle rect = { gridX + left * cell + 3.0f, (float)(y + r * cell + 3), width * cell - 6.0f, (float)(cell - 6) };
                const bool sel = (r == row && c == col);
                const bool special = r >= kKeyRows;
                Color fill = sel ? Color{ kBlue.r, kBlue.g, kBlue.b, 150 } : Color{ 40, 40, 80, 255 };
                if (special && c == kDone && !sel) fill = Color{ 40, 90, 60, 255 };
                DrawRectangleRec(rect, fill);
                DrawRectangleLinesEx(rect, sel ? 3.0f : 1.0f, sel ? kTextSel : Color{ 90, 90, 130, 255 });

                char one[2] = { special ? '\0' : kPages[page][r][c], '\0' };
                const char* label = special ? labels[c] : one;
                Vector2 size = MeasureTextEx(ItemFont(), label, 34, 1);
                DrawTextEx(ItemFont(), label, { rect.x + (rect.width - size.x) * 0.5f, rect.y + (rect.height - size.y) * 0.5f },
                           34, 1, sel ? kTextSel : kText);
            }
        }
        EndScreen("JOYSTICK   move       ANY BUTTON   type the key       ESC   cancel");
    }
    return false;
}

} // namespace ui
