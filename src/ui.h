// Arcade Menu -- src/ui.h
//
// Drawing and input helpers shared by the settings screens (settings.cpp, keyboard.cpp).
// The look matches the main menu in main.cpp: navy gradient, gold title, blue selection box.
//
// Every screen is a blocking loop, like the SDK's RunEncoderSetup():
//
//     ui::Nav nav;
//     while (ui::Frame(nav))                       // UpdateInput() + read one navigation event
//     {
//         if (nav == ui::Nav::Back) break;
//         int y = ui::BeginScreen("TITLE");        // BeginDrawing, background, title; returns the content top
//         ...ui::DrawRow(...)...
//         ui::EndScreen("JOYSTICK   choose       ANY BUTTON   select");
//     }
//
// Input (the cabinet has only a joystick and buttons per player, so every screen works with them alone):
//   Up/Down/Left/Right  encoder joysticks, keyboard W A S D and arrows, gamepad d-pad (hold = repeat)
//   Select              ANY encoder button, Enter, Space, gamepad A
//   Back                Esc, Backspace, gamepad B   (every screen also has a visible "Back" row)

#pragma once

#include "raylib.h"

#include <string>

namespace ui
{

enum class Nav { None, Up, Down, Left, Right, Select, Back };

// The fonts the main menu loaded (main.cpp keeps ownership and unloads them).
void SetFonts(Font title, Font item, Font hint);
Font TitleFont();
Font ItemFont();
Font HintFont();

// Palette (same values as main.cpp).
extern const Color kGold;       // titles
extern const Color kText;       // normal text
extern const Color kTextSel;    // selected text
extern const Color kDim;        // hints, details
extern const Color kBlue;       // selection box
extern const Color kGreen;      // success
extern const Color kRed;        // errors
extern const Color kAmber;      // warnings

// One frame step for a blocking screen: calls arcade::UpdateInput() (exactly once) and reads the
// navigation event. Returns false if the window was closed.
// textMode = true for the on-screen keyboard: the keyboard's letters, Enter, Space and Backspace are
// text, not navigation (arrows, encoders and gamepads still move the cursor; Esc is still Back).
bool Frame(Nav& nav, bool textMode = false);

double Now();

// Call first thing when a screen opens. raylib refreshes its key/button state only in EndDrawing(), so the
// press that OPENED a screen is still "pressed" inside it and would immediately trigger that screen's first
// row (and the next screen's, and so on). Settle() draws one background frame, which clears it.
void Settle();

// BeginDrawing + gradient + title + rule. Returns the y where content can start.
int BeginScreen(const char* title);
// Footer hint, EndDrawing.
void EndScreen(const char* footer);

constexpr int kRowPitch = 72;   // distance between two rows

// A one-line row: label on the left, optional detail on the right. Text is shortened to fit.
void DrawRow(int y, const std::string& label, const std::string& detail, bool selected, bool dimmed = false);
// Four signal bars (signal 0..100) ending at x (right edge), vertically centred on a row at y.
void DrawSignal(int rightX, int y, int signal, bool selected);
// Text centred on the screen.
void DrawCentered(const std::string& text, int y, int size, Color color, bool item = false);
// Left-aligned text, shortened with "..." to maxWidth.
void DrawFit(const std::string& text, int x, int y, int size, Color color, int maxWidth, bool item = false);
// "Working..." text with a moving dot count, so the screen is visibly alive.
std::string Working(const std::string& text);

} // namespace ui
