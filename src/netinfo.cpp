// Arcade Menu -- src/netinfo.cpp  (see netinfo.h)

#include "netinfo.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace ely
{

static std::vector<std::string> Lines(const std::string& text)
{
    std::vector<std::string> lines;
    std::string cur;
    for (char c : text)
    {
        if (c == '\n') { lines.push_back(cur); cur.clear(); }
        else if (c != '\r') cur += c;
    }
    if (!cur.empty()) lines.push_back(cur);
    return lines;
}

std::vector<std::string> SplitTerse(const std::string& line)
{
    std::vector<std::string> fields(1);
    for (size_t i = 0; i < line.size(); ++i)
    {
        char c = line[i];
        if (c == '\\' && i + 1 < line.size()) fields.back() += line[++i];
        else if (c == ':') fields.emplace_back();
        else fields.back() += c;
    }
    return fields;
}

std::vector<NetDevice> ParseDevices(const std::string& out)
{
    std::vector<NetDevice> devices;
    for (const std::string& line : Lines(out))
    {
        std::vector<std::string> f = SplitTerse(line);
        if (f.size() < 3 || f[0].empty()) continue;
        NetDevice d;
        d.name = f[0];
        d.type = f[1];
        d.state = f[2];
        if (f.size() > 3 && f[3] != "--") d.connection = f[3];
        devices.push_back(d);
    }
    return devices;
}

static bool IsConnected(const NetDevice& d)
{
    return d.state.compare(0, 9, "connected") == 0 && d.state.find("(connecting") == std::string::npos;
}

NetSummary Summarize(const std::vector<NetDevice>& devices)
{
    NetSummary s;
    s.known = !devices.empty();
    for (const NetDevice& d : devices)
    {
        if (d.type == "wifi")
        {
            if (!s.wifiPresent)
            {
                s.wifiPresent = true;
                s.wifiDevice = d.name;
                s.wifiState = d.state;
            }
            if (IsConnected(d) && s.wifiConnection.empty())
            {
                s.wifiDevice = d.name;
                s.wifiState = d.state;
                s.wifiConnection = d.connection;
                s.online = true;
            }
        }
        else if (d.type == "ethernet" && IsConnected(d))
        {
            if (s.wiredConnection.empty()) s.wiredConnection = d.connection.empty() ? d.name : d.connection;
            s.online = true;
        }
    }
    return s;
}

std::vector<WifiNetwork> ParseWifiList(const std::string& out)
{
    std::vector<WifiNetwork> nets;
    for (const std::string& line : Lines(out))
    {
        std::vector<std::string> f = SplitTerse(line);
        if (f.size() < 4 || f[1].empty()) continue;

        WifiNetwork n;
        n.active = (f[0] == "*");
        n.ssid = f[1];
        n.signal = std::max(0, std::min(100, atoi(f[2].c_str())));
        const std::string& sec = f[3];
        bool open = sec.empty() || sec == "--" || sec == "OWE";
        n.secured = !open;
        n.saeOnly = sec.find("WPA3") != std::string::npos && sec.find("WPA2") == std::string::npos &&
                    sec.find("WPA1") == std::string::npos;
        n.unsupported = sec.find("802.1X") != std::string::npos || n.ssid[0] == '-';   // '-' would look like an option to nmcli

        bool merged = false;
        for (WifiNetwork& e : nets)
        {
            if (e.ssid != n.ssid) continue;
            merged = true;
            e.active = e.active || n.active;
            if (n.signal > e.signal) { e.signal = n.signal; e.secured = n.secured; e.saeOnly = n.saeOnly; e.unsupported = n.unsupported; }
        }
        if (!merged) nets.push_back(n);
    }
    std::stable_sort(nets.begin(), nets.end(), [](const WifiNetwork& a, const WifiNetwork& b) {
        if (a.active != b.active) return a.active;
        return a.signal > b.signal;
    });
    return nets;
}

static bool Contains(const std::string& text, const char* what)
{
    return text.find(what) != std::string::npos;
}

std::string ExplainConnectError(const std::string& out)
{
    if (Contains(out, "Secrets were required") || Contains(out, "(7)") || Contains(out, "802.1X supplicant"))
        return "Wrong password?";
    if (Contains(out, "No network with SSID"))
        return "That network was not found.";
    if (Contains(out, "IP configuration could not be reserved") || Contains(out, "(5)"))
        return "Joined the network but got no address.";
    if (Contains(out, "Timeout expired") || Contains(out, "timed out"))
        return "The network did not answer in time.";
    if (Contains(out, "not authorized") || Contains(out, "Insufficient privileges"))
        return "Not allowed to change the network settings.";
    if (Contains(out, "NetworkManager is not running"))
        return "NetworkManager is not running.";
    // Fall back to the last line of the output, without nmcli's "Error: " prefix.
    std::vector<std::string> lines = Lines(out);
    for (size_t i = lines.size(); i-- > 0;)
    {
        std::string l = lines[i];
        if (l.compare(0, 7, "Error: ") == 0) l = l.substr(7);
        if (!l.empty()) return l.size() > 90 ? l.substr(0, 87) + "..." : l;
    }
    return "Could not connect.";
}

UpdateStatus ParseUpdateStatus(const std::string& text)
{
    UpdateStatus s;
    for (const std::string& line : Lines(text))
    {
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq), value = line.substr(eq + 1);
        if (key == "state") { s.state = value; s.valid = true; }
        else if (key == "message") s.message = value;
        else if (key == "installed") s.installed = value;
        else if (key == "staged") s.staged = value;
        else if (key == "pid") s.pid = atol(value.c_str());
        else if (key == "time") s.time = atol(value.c_str());
    }
    return s;
}

std::string ParseInstalledCommit(const std::string& text)
{
    const std::string prefix = "platform ";
    if (text.compare(0, prefix.size(), prefix) != 0) return "";
    std::string hash = text.substr(prefix.size(), 7);
    for (char c : hash)
        if (!isxdigit((unsigned char)c)) return "";
    return hash.size() == 7 ? hash : "";
}

} // namespace ely
