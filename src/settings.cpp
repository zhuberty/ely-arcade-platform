// Arcade Menu -- src/settings.cpp  (see settings.h)

#include "settings.h"
#include "process.h"
#include "ui.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <sstream>

#if !defined(_WIN32)
    #include <cerrno>
    #include <csignal>
    #include <sys/types.h>
#endif

namespace settings
{

namespace
{

std::string g_root = ".";
ely::NetSummary g_net;
ely::UpdateStatus g_update;
std::string g_installed;
ely::AsyncCommand g_netCmd;
double g_nextNet = 0.0;
double g_nextFile = 0.0;
double g_nextInstalled = 0.0;

std::string ReadFile(const std::string& path)
{
    std::ifstream in(path.c_str());
    if (!in) return "";
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// ELY_ARCADE_STATUS lets a development PC point the menu at a made-up status file.
std::string StatusPath()
{
    const char* e = getenv("ELY_ARCADE_STATUS");
    return (e && *e) ? e : "/var/lib/ely-arcade-update/status";
}

bool Alive(long pid)
{
#if defined(_WIN32)
    (void)pid;
    return false;
#else
    return pid > 0 && (kill((pid_t)pid, 0) == 0 || errno == EPERM);
#endif
}

void ReadUpdateStatus()
{
    g_update = ely::ParseUpdateStatus(ReadFile(StatusPath()));
    // A run that died (power cut, killed) leaves "checking"/"building" behind for ever.
    if (g_update.valid && (g_update.state == "checking" || g_update.state == "building") && !Alive(g_update.pid))
    {
        g_update.state = "failed";
        g_update.message = "The last update was interrupted";
    }
}

} // namespace

std::vector<std::string> Wrap(const std::string& text, int maxWidth, int size)
{
    std::vector<std::string> lines;
    std::string cur;
    std::istringstream words(text);
    std::string w;
    while (words >> w)
    {
        std::string t = cur.empty() ? w : cur + " " + w;
        if (!cur.empty() && (int)MeasureTextEx(ui::ItemFont(), t.c_str(), (float)size, 1).x > maxWidth)
        {
            lines.push_back(cur);
            cur = w;
        }
        else cur = t;
    }
    if (!cur.empty()) lines.push_back(cur);
    return lines;
}

std::string Ago(long then)
{
    long d = (long)time(nullptr) - then;
    if (then <= 0 || d < 0) return "";
    if (d < 90) return "just now";
    if (d < 5400) return std::to_string(d / 60) + " minutes ago";
    if (d < 172800) return std::to_string(d / 3600) + " hours ago";
    return std::to_string(d / 86400) + " days ago";
}

void Init(const std::string& platformRoot)
{
    g_root = platformRoot;
    ReadUpdateStatus();
    g_installed = ely::ParseInstalledCommit(ReadFile(g_root + "/INSTALLED-VERSIONS"));
}

std::string RootDir() { return g_root; }
const ely::NetSummary& Net() { return g_net; }
const ely::UpdateStatus& UpdateState() { return g_update; }
// INSTALLED-VERSIONS is the truth; the status file (written by the updater) is the fallback.
std::string InstalledCommit() { return g_installed.empty() ? g_update.installed : g_installed; }
void RefreshNow() { g_nextNet = 0.0; g_nextFile = 0.0; }

void Update()
{
    const double now = ui::Now();

    if (g_netCmd.Running())
    {
        if (g_netCmd.Update())
        {
            g_net = ely::Summarize(ely::ParseDevices(g_netCmd.ExitCode() == 0 ? g_netCmd.Output() : ""));
            g_nextNet = now + 5.0;
        }
    }
    else if (now >= g_nextNet)
    {
        g_netCmd.Start({ "nmcli", "-t", "-f", "DEVICE,TYPE,STATE,CONNECTION", "device", "status" }, 10.0);
        g_nextNet = now + 5.0;   // also the retry delay if it cannot start
    }

    if (now >= g_nextFile)
    {
        ReadUpdateStatus();
        g_nextFile = now + 1.5;
    }
    if (now >= g_nextInstalled)
    {
        g_installed = ely::ParseInstalledCommit(ReadFile(g_root + "/INSTALLED-VERSIONS"));
        g_nextInstalled = now + 30.0;
    }
}

void DrawStatus(int screenWidth)
{
    std::string netText;
    Color netColor = ui::kAmber;
    if (!g_net.known)                         { netText = "Network: unknown"; netColor = ui::kDim; }
    else if (g_net.online)
    {
        netText = !g_net.wifiConnection.empty() ? "Wi-Fi: " + g_net.wifiConnection : "Wired network";
        netColor = ui::kGreen;
    }
    else if (g_net.wifiState.compare(0, 10, "connecting") == 0) netText = "Wi-Fi: connecting...";
    else if (g_net.wifiPresent)               netText = "No internet connection";
    else                                      netText = "No network adapter found";

    std::string updText;
    Color updColor = ui::kGold;
    if (g_update.valid)
    {
        if (g_update.state == "ready")         updText = "Update ready: restart to install";
        else if (g_update.state == "building") updText = "Downloading an update...";
        else if (g_update.state == "failed")   { updText = "Last update failed"; updColor = ui::kDim; }
    }

    int y = 18;
    for (int i = 0; i < 2; ++i)
    {
        const std::string& t = i == 0 ? netText : updText;
        if (t.empty()) continue;
        Vector2 s = MeasureTextEx(ui::HintFont(), t.c_str(), 24, 1);
        DrawTextEx(ui::HintFont(), t.c_str(), { screenWidth - s.x - 30.0f, (float)y }, 24, 1, i == 0 ? netColor : updColor);
        y += 30;
    }
}

// ---------------------------------------------------------------------------
// Updates screen
// ---------------------------------------------------------------------------

static void RunUpdates()
{
    ely::AsyncCommand cmd;        // `systemctl start --no-block ...`: returns at once
    bool asked = false;
    double askedAt = 0.0;
    std::string note;
    int sel = 0;

    ui::Settle();
    ui::Nav nav;
    while (ui::Frame(nav))
    {
        Update();
        if (cmd.Running() && cmd.Update())
        {
            if (cmd.ExitCode() != 0) note = "Could not start the update check.";
            RefreshNow();
        }

        const ely::UpdateStatus& u = UpdateState();
        const bool busy = u.valid && (u.state == "checking" || u.state == "building");
        const bool ready = u.valid && u.state == "ready";
        if (asked && busy) asked = false;
        const bool starting = asked && ui::Now() - askedAt < 8.0;

        std::vector<std::string> rows;
        std::vector<int> actions;     // 0 check, 1 restart, 2 back
        if (ready) { rows.push_back("Restart now to install the update"); actions.push_back(1); }
        rows.push_back("Check for updates now");
        actions.push_back(0);
        rows.push_back("Back");
        actions.push_back(2);
        sel = std::max(0, std::min(sel, (int)rows.size() - 1));

        if (nav == ui::Nav::Back) return;
        if (nav == ui::Nav::Down) sel = (sel + 1) % (int)rows.size();
        if (nav == ui::Nav::Up) sel = (sel + (int)rows.size() - 1) % (int)rows.size();
        if (nav == ui::Nav::Select)
        {
            int action = actions[sel];
            if (action == 2) return;
            if (action == 0 && !busy && !starting)
            {
                cmd.Start({ "systemctl", "start", "--no-block", "ely-arcade-update.service" }, 10.0);
                asked = true;
                askedAt = ui::Now();
                note.clear();
            }
            if (action == 1 && Choose("RESTART", "Restart the arcade to install the update?",
                                      { "Yes, restart now", "No, later" }) == 0)
            {
                ely::AsyncCommand reboot;
                reboot.Start({ "systemctl", "reboot" }, 10.0);
                while (!reboot.Update() && !WindowShouldClose()) { BeginDrawing(); ClearBackground(BLACK); EndDrawing(); }
                if (reboot.ExitCode() != 0)
                    Message("RESTART", "The arcade could not be restarted from here. Switch it off and on again.", false);
            }
        }

        int y = ui::BeginScreen("UPDATES");
        std::string line1 = "Installed version: " + (InstalledCommit().empty() ? std::string("unknown") : InstalledCommit());
        ui::DrawCentered(line1, y, 30, ui::kText, true);
        y += 54;

        std::string text;
        Color col = ui::kText;
        if (starting && !busy)            { text = ui::Working("Starting"); col = ui::kGold; }
        else if (!u.valid)                { text = "No update service found on this machine."; col = ui::kDim; }
        else if (busy)                    { text = ui::Working(u.message.empty() ? "Working" : u.message); col = ui::kGold; }
        else if (ready)                   { text = "An update is ready. It is installed when the arcade restarts."; col = ui::kGreen; }
        else if (u.state == "uptodate")   { text = "Everything is up to date (" + Ago(u.time) + ")."; col = ui::kGreen; }
        else if (u.state == "offline")    { text = "Could not check: no internet connection. Set up Wi-Fi first."; col = ui::kAmber; }
        else if (u.state == "failed")     { text = "The last update failed: " + u.message + " The arcade keeps working as before."; col = ui::kRed; }
        else if (u.state == "frozen")     { text = "Updates are switched off on this arcade."; col = ui::kDim; }
        else                              { text = u.message.empty() ? "Not checked yet." : u.message; col = ui::kDim; }
        for (const std::string& l : Wrap(text, GetScreenWidth() - 200, 30))
        {
            ui::DrawCentered(l, y, 30, col, true);
            y += 44;
        }
        if (!note.empty()) { ui::DrawCentered(note, y, 28, ui::kRed, true); y += 44; }
        y += 24;
        for (size_t i = 0; i < rows.size(); ++i)
            ui::DrawRow(y + (int)i * ui::kRowPitch, rows[i], "", (int)i == sel, actions[i] == 0 && (busy || starting));
        ui::EndScreen("JOYSTICK   choose       ANY BUTTON   select       ESC   back");
    }
}

// ---------------------------------------------------------------------------
// Dialogs
// ---------------------------------------------------------------------------

void Message(const std::string& title, const std::string& text, bool good)
{
    ui::Settle();
    ui::Nav nav;
    while (ui::Frame(nav))
    {
        if (nav == ui::Nav::Select || nav == ui::Nav::Back) return;
        Update();
        int y = ui::BeginScreen(title.c_str()) + 40;
        for (const std::string& line : Wrap(text, GetScreenWidth() - 200, 34))
        {
            ui::DrawCentered(line, y, 34, good ? ui::kGreen : ui::kRed, true);
            y += 50;
        }
        ui::EndScreen("ANY BUTTON   OK");
    }
}

int Choose(const std::string& title, const std::string& prompt, const std::vector<std::string>& options)
{
    int sel = 0;
    ui::Settle();
    ui::Nav nav;
    while (ui::Frame(nav))
    {
        const int n = (int)options.size();
        if (nav == ui::Nav::Back) return -1;
        if (nav == ui::Nav::Select) return sel;
        if (nav == ui::Nav::Down) sel = (sel + 1) % n;
        if (nav == ui::Nav::Up) sel = (sel + n - 1) % n;
        Update();
        int y = ui::BeginScreen(title.c_str());
        for (const std::string& line : Wrap(prompt, GetScreenWidth() - 200, 34))
        {
            ui::DrawCentered(line, y, 34, ui::kText, true);
            y += 48;
        }
        y += 30;
        for (int i = 0; i < n; ++i)
            ui::DrawRow(y + i * ui::kRowPitch, options[i], "", i == sel);
        ui::EndScreen("JOYSTICK   choose       ANY BUTTON   select       ESC   back");
    }
    return -1;
}

// ---------------------------------------------------------------------------
// Settings screen
// ---------------------------------------------------------------------------

void Run()
{
    int sel = 0;
    ui::Settle();
    ui::Nav nav;
    while (ui::Frame(nav))
    {
        Update();
        const ely::NetSummary& n = Net();
        const ely::UpdateStatus& u = UpdateState();

        std::string wifiDetail = !n.known ? "" : n.online ? (n.wifiConnection.empty() ? "wired" : n.wifiConnection)
                               : n.wifiPresent ? "not connected" : "no adapter";
        std::string updDetail = !u.valid ? "" : u.state == "ready" ? "update ready" : u.state == "building" ? "downloading" : "";

        if (nav == ui::Nav::Back) return;
        if (nav == ui::Nav::Down) sel = (sel + 1) % 3;
        if (nav == ui::Nav::Up) sel = (sel + 2) % 3;
        if (nav == ui::Nav::Select)
        {
            if (sel == 0) RunWifi();
            else if (sel == 1) RunUpdates();
            else return;
        }

        int y = ui::BeginScreen("SETTINGS") + 10;
        ui::DrawRow(y, "Wi-Fi", wifiDetail, sel == 0);
        ui::DrawRow(y + ui::kRowPitch, "Updates", updDetail, sel == 1);
        ui::DrawRow(y + 2 * ui::kRowPitch, "Back to the menu", "", sel == 2);
        std::string v = InstalledCommit().empty() ? "" : "Version " + InstalledCommit();
        ui::DrawCentered(v, GetScreenHeight() - 100, 24, ui::kDim);
        ui::EndScreen("JOYSTICK   choose       ANY BUTTON   select       ESC   back");
    }
}

} // namespace settings
