// Arcade Menu -- src/wifi_screen.cpp
//
// Settings > Wi-Fi: lists the networks in range, asks for the password with the on-screen keyboard,
// joins through scripts/wifi.sh and can forget a saved network. Joystick and buttons only.

#include "settings.h"
#include "keyboard.h"
#include "netinfo.h"
#include "process.h"
#include "ui.h"

#include <algorithm>

namespace settings
{

namespace
{

// Runs scripts/wifi.sh (the command runs in the background; this draws a "please wait" screen) and
// returns whether it succeeded. `output` gets what it printed.
bool RunWifiScript(const std::string& what, const std::vector<std::string>& args, const std::string& stdinText,
                   double timeout, std::string& output)
{
    std::vector<std::string> argv = { "bash", RootDir() + "/scripts/wifi.sh" };
    argv.insert(argv.end(), args.begin(), args.end());
    ely::AsyncCommand cmd;
    if (!cmd.Start(argv, timeout, stdinText))
    {
        output = "Could not start the Wi-Fi helper.";
        return false;
    }
    ui::Nav nav;
    while (ui::Frame(nav) && !cmd.Update())
    {
        Update();
        int y = ui::BeginScreen("WI-FI") + 60;
        ui::DrawCentered(ui::Working(what), y, 40, ui::kGold, true);
        ui::DrawCentered("This can take up to a minute.", y + 70, 28, ui::kDim);
        ui::EndScreen("Please wait");
    }
    output = cmd.Output();
    return cmd.Finished() && cmd.ExitCode() == 0;
}

void Join(const std::string& ssid, const std::string& security, bool hidden, bool secured)
{
    std::string password;
    if (secured &&
        !ui::TextEntry("WI-FI PASSWORD", "Password for \"" + ssid + "\"", password, true, 8, 63))
        return;

    std::vector<std::string> args = { "connect", ssid, security };
    if (hidden) args.push_back("hidden");
    std::string out;
    bool ok = RunWifiScript("Connecting to " + ssid, args, password.empty() ? "" : password + "\n", 80.0, out);
    password.assign(password.size(), '\0');    // do not keep the password in memory longer than needed
    RefreshNow();
    if (ok) Message("WI-FI", "Connected to " + ssid + ". The arcade will connect to it automatically from now on.", true);
    else    Message("WI-FI", "Could not connect to " + ssid + ". " + ely::ExplainConnectError(out), false);
}

void JoinHidden()
{
    std::string ssid;
    if (!ui::TextEntry("HIDDEN NETWORK", "Name of the network", ssid, false, 1, 32)) return;
    int c = Choose("HIDDEN NETWORK", "How is \"" + ssid + "\" protected?",
                   { "With a password", "No password (open network)" });
    if (c < 0) return;
    Join(ssid, "wpa-psk", true, c == 0);
}

void ManageActive(const ely::WifiNetwork& net)
{
    int c = Choose("WI-FI", "Connected to \"" + net.ssid + "\".",
                   { "Enter a new password", "Forget this network", "Keep it as it is" });
    if (c == 0) Join(net.ssid, net.saeOnly ? "sae" : "wpa-psk", false, net.secured);
    if (c == 1)
    {
        std::string out;
        RunWifiScript("Forgetting " + net.ssid, { "forget", net.ssid }, "", 20.0, out);
        RefreshNow();
        Message("WI-FI", "Forgot \"" + net.ssid + "\". The arcade will not join it again unless you set it up.", true);
    }
}

} // namespace

void RunWifi()
{
    ely::AsyncCommand scan;
    std::vector<ely::WifiNetwork> nets;
    bool scanned = false;
    double nextScan = 0.0;
    int sel = 0, first = 0;

    ui::Settle();
    ui::Nav nav;
    while (ui::Frame(nav))
    {
        Update();
        const ely::NetSummary& n = Net();

        if (scan.Running())
        {
            if (scan.Update())
            {
                if (scan.ExitCode() == 0) nets = ely::ParseWifiList(scan.Output());
                scanned = true;
                nextScan = ui::Now() + 15.0;
            }
        }
        else if (n.wifiPresent && ui::Now() >= nextScan)
        {
            scan.Start({ "nmcli", "-t", "-f", "IN-USE,SSID,SIGNAL,SECURITY", "device", "wifi", "list", "--rescan", "yes" }, 30.0);
            nextScan = ui::Now() + 15.0;
        }

        const int count = (int)nets.size() + 2;          // + hidden network, + back
        sel = std::max(0, std::min(sel, count - 1));
        if (nav == ui::Nav::Back) return;
        if (nav == ui::Nav::Down) sel = (sel + 1) % count;
        if (nav == ui::Nav::Up) sel = (sel + count - 1) % count;
        if (nav == ui::Nav::Select)
        {
            if (sel == count - 1) return;
            if (!n.wifiPresent) Message("WI-FI", "No Wi-Fi adapter was found in this arcade.", false);
            else if (sel == count - 2) { JoinHidden(); nextScan = 0.0; }
            else
            {
                const ely::WifiNetwork net = nets[sel];
                if (net.unsupported)
                    Message("WI-FI", "\"" + net.ssid + "\" needs a username as well as a password. That kind of network cannot be set up here.", false);
                else if (net.active) ManageActive(net);
                else Join(net.ssid, net.saeOnly ? "sae" : "wpa-psk", false, net.secured);
                nextScan = 0.0;
            }
        }

        int y = ui::BeginScreen("WI-FI");
        std::string status = n.online ? (n.wifiConnection.empty() ? "Connected by cable" : "Connected to " + n.wifiConnection)
                           : !n.known ? "Network status unknown" : n.wifiPresent ? "Not connected to the internet" : "No Wi-Fi adapter found";
        ui::DrawCentered(status, y - 12, 30, n.online ? ui::kGreen : ui::kAmber, true);
        y += 40;

        const int visible = std::max(3, (GetScreenHeight() - y - 90) / ui::kRowPitch);
        if (sel < first) first = sel;
        if (sel >= first + visible) first = sel - visible + 1;
        for (int i = first; i < count && i < first + visible; ++i)
        {
            const int ry = y + (i - first) * ui::kRowPitch;
            if (i == count - 1) ui::DrawRow(ry, "Back", "", i == sel);
            else if (i == count - 2) ui::DrawRow(ry, "Join a hidden network...", "", i == sel);
            else
            {
                const ely::WifiNetwork& w = nets[i];
                std::string detail = w.active ? "connected" : w.unsupported ? "not supported" : w.secured ? "password" : "open";
                ui::DrawRow(ry, w.ssid, detail, i == sel);
                int detailW = (int)MeasureTextEx(ui::HintFont(), detail.c_str(), 24, 1).x;
                ui::DrawSignal(GetScreenWidth() - 80 - detailW - 30, ry, w.signal, i == sel);
            }
        }
        if (n.wifiPresent && !scanned)
            ui::DrawCentered(ui::Working("Looking for networks"), y + 2 * ui::kRowPitch + 10, 28, ui::kGold);
        if (n.wifiPresent && scanned && nets.empty())
            ui::DrawCentered("No networks found. Move closer to the router, or join a hidden network.",
                             y + 2 * ui::kRowPitch + 10, 26, ui::kDim);
        ui::EndScreen("JOYSTICK   choose       ANY BUTTON   select       ESC   back");
    }
}

} // namespace settings
