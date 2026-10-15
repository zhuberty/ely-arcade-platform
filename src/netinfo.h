// Arcade Menu -- src/netinfo.h
//
// Parsers for the text the settings screens get from NetworkManager (`nmcli -t`) and from the
// update service (/var/lib/ely-arcade-update/status). No raylib and no system calls in here, so
// tests/netinfo_test.cpp can check them on any machine (scripts/test.sh).

#pragma once

#include <string>
#include <vector>

namespace ely
{

// `nmcli -t` separates fields with ':'. A literal ':' or '\' inside a value is written '\:' or '\\'.
std::vector<std::string> SplitTerse(const std::string& line);

// ---- nmcli -t -f DEVICE,TYPE,STATE,CONNECTION device status -------------------------------------------

struct NetDevice
{
    std::string name;
    std::string type;        // wifi, ethernet, loopback, bridge, ...
    std::string state;       // connected, disconnected, unavailable, connecting (...), ...
    std::string connection;  // profile name, "" if none
};

std::vector<NetDevice> ParseDevices(const std::string& out);

struct NetSummary
{
    bool known = false;          // false: nmcli missing or NetworkManager not running
    bool online = false;         // a wifi or ethernet device is connected
    bool wifiPresent = false;
    std::string wifiDevice;      // first wifi device ("" = none)
    std::string wifiState;       // its state
    std::string wifiConnection;  // its profile name while connected
    std::string wiredConnection; // first connected ethernet profile
};

NetSummary Summarize(const std::vector<NetDevice>& devices);

// ---- nmcli -t -f IN-USE,SSID,SIGNAL,SECURITY device wifi list -----------------------------------------

struct WifiNetwork
{
    std::string ssid;
    int signal = 0;          // 0..100
    bool secured = false;    // needs a password
    bool saeOnly = false;    // WPA3 without WPA2: the password type is `sae`, not `wpa-psk`
    bool unsupported = false;// 802.1X (username + password): not offered by the menu
    bool active = false;     // the connection in use
};

// Skips hidden (empty) names, keeps the strongest entry per name, strongest first.
std::vector<WifiNetwork> ParseWifiList(const std::string& out);

// A user-readable reason for a failed `nmcli device wifi connect` (from its output).
std::string ExplainConnectError(const std::string& nmcliOutput);

// ---- /var/lib/ely-arcade-update/status ----------------------------------------------------------------
// Lines of key=value written by /usr/local/sbin/ely-arcade-update:
//   state=checking|building|ready|uptodate|offline|failed|frozen|idle   message=<one line>
//   installed=<short commit>  staged=<short commit>  pid=<pid of the running update>  time=<epoch>

struct UpdateStatus
{
    bool valid = false;
    std::string state;
    std::string message;
    std::string installed;
    std::string staged;
    long pid = 0;
    long time = 0;
};

UpdateStatus ParseUpdateStatus(const std::string& text);

// First line of INSTALLED-VERSIONS: "platform <40 hex> (<url>, <branch>)" -> the first 7 hex digits.
std::string ParseInstalledCommit(const std::string& text);

} // namespace ely
