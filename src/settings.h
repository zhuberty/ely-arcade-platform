// Arcade Menu -- src/settings.h
//
// The "Settings" entry of the menu (Wi-Fi and software updates) and the status line in the corner of the
// main menu. All of it works with the joystick and buttons alone (see ui.h, keyboard.h).
//
// What it talks to (nothing here needs root; the cabinet is set up by ely-arcade-fai, which also writes
// the units and the polkit rule that allow the arcade account to do exactly this):
//   NetworkManager   `nmcli`, through scripts/wifi.sh for joining/forgetting a network (the password goes
//                    over stdin). The arcade account is in the `netdev` group.
//   Update service   /var/lib/ely-arcade-update/status is read; `systemctl start ely-arcade-update.service`
//                    asks for a check now; `systemctl reboot` installs a finished update.
// Where a program is missing (a development PC without NetworkManager or systemd) the screens say so.

#pragma once

#include "netinfo.h"

#include <string>
#include <vector>

namespace settings
{

// Call once, after the menu has changed into the platform root (the folder with scripts/).
void Init(const std::string& platformRoot);

// Every frame of the main menu: refreshes the network and update status in the background.
void Update();

// The one-line status in the top-right corner of the main menu.
void DrawStatus(int screenWidth);

// The Settings screen (blocking, returns when the user leaves it).
void Run();

// ---- shared by the screens (settings.cpp and wifi_screen.cpp) ----------------------------------------------
const ely::NetSummary& Net();               // last known network state
const ely::UpdateStatus& UpdateState();     // last known update state (valid = false if there is no status file)
std::string InstalledCommit();              // short commit of the installed software, "" if unknown
void RefreshNow();                          // poll again on the next frame
std::string RootDir();                      // platform root, as given to Init()

// Blocking dialogs in the style of the menu. All return when the user presses a button.
void Message(const std::string& title, const std::string& text, bool good);
// Shows the prompt and the options; returns the chosen index, or -1 for Back.
int Choose(const std::string& title, const std::string& prompt, const std::vector<std::string>& options);


// The Wi-Fi screen (blocking).
void RunWifi();

} // namespace settings
