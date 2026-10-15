// Arcade Menu -- src/keyboard.h
//
// An on-screen keyboard for the cabinet, which has only joysticks and buttons: move with the
// joystick, press any button to type the highlighted key. A USB keyboard also works (type,
// Backspace, Enter = OK, Esc = cancel). Used for the Wi-Fi password.
//
// Printable ASCII only (32..126): that is all a WPA passphrase may contain.

#pragma once

#include <string>

namespace ui
{

// Blocks, running its own frame loop. `text` is the initial and the final value. Returns true if the
// user finished with OK (and text has at least minLen characters), false if cancelled (text untouched).
// secret = show the text as dots until the user picks SHOW.
bool TextEntry(const std::string& title, const std::string& prompt, std::string& text,
               bool secret, size_t minLen, size_t maxLen);

} // namespace ui
