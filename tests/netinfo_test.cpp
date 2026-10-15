// Tests for src/netinfo.cpp and src/process.cpp (no raylib needed).  Run: scripts/test.sh

#include "../src/netinfo.h"
#include "../src/process.h"

#include <chrono>
#include <cstdio>
#include <thread>

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

static void TestSplit()
{
    auto f = ely::SplitTerse("a:b\\:c:d\\\\e::");
    CHECK(f.size() == 5);
    CHECK(f[0] == "a" && f[1] == "b:c" && f[2] == "d\\e" && f[3] == "" && f[4] == "");
}

static void TestDevices()
{
    const char* out =
        "wlan0:wifi:connected:Cafe Wifi\n"
        "eth0:ethernet:unavailable:\n"
        "lo:loopback:connected (externally):lo\n"
        "p2p-dev-wlan0:wifi-p2p:disconnected:--\n";
    auto s = ely::Summarize(ely::ParseDevices(out));
    CHECK(s.known && s.online && s.wifiPresent);
    CHECK(s.wifiDevice == "wlan0" && s.wifiConnection == "Cafe Wifi");
    CHECK(s.wiredConnection.empty());

    // connecting is not online
    s = ely::Summarize(ely::ParseDevices("wlan0:wifi:connecting (getting IP configuration):Home\n"));
    CHECK(s.wifiPresent && !s.online && s.wifiConnection.empty());

    // wired only
    s = ely::Summarize(ely::ParseDevices("enp1s0:ethernet:connected:Wired connection 1\nlo:loopback:connected (externally):lo\n"));
    CHECK(s.online && !s.wifiPresent && s.wiredConnection == "Wired connection 1");

    // nothing / NetworkManager down
    s = ely::Summarize(ely::ParseDevices(""));
    CHECK(!s.known && !s.online);
    s = ely::Summarize(ely::ParseDevices("Error: NetworkManager is not running.\n"));
    CHECK(!s.known);
}

static void TestWifiList()
{
    const char* out =
        " :LOON2:57:WPA2\n"
        "*:LOON2:92:WPA2\n"
        " ::74:WPA2\n"                        // hidden network: skipped
        " :Guest\\:Open:40:\n"                // open, with a ':' in the name
        " :Office:88:WPA2 802.1X\n"
        " :Neighbour:99:WPA1 WPA2\n"
        " :Cafe:63:--\n"
        " :Weak:3:WPA3\n"
        " :-dash:2:WPA2\n";
    auto n = ely::ParseWifiList(out);
    CHECK(n.size() == 7);
    CHECK(n[0].ssid == "LOON2" && n[0].active && n[0].signal == 92);   // active first, merged, strongest
    CHECK(n[1].ssid == "Neighbour" && n[1].secured);
    CHECK(n[2].ssid == "Office" && n[2].unsupported && n[2].secured);
    CHECK(n[3].ssid == "Cafe" && !n[3].secured);
    CHECK(n[4].ssid == "Guest:Open" && !n[4].secured);
    CHECK(n[5].ssid == "Weak" && n[5].signal == 3 && n[5].saeOnly);
    CHECK(!n[1].saeOnly && !n[0].saeOnly);
    CHECK(n[6].ssid == "-dash" && n[6].unsupported);
    CHECK(ely::ParseWifiList("").empty());
}

static void TestErrors()
{
    CHECK(ely::ExplainConnectError("Error: Connection activation failed: (7) Secrets were required, but not provided.") == "Wrong password?");
    CHECK(ely::ExplainConnectError("Error: No network with SSID 'x' found.") == "That network was not found.");
    CHECK(ely::ExplainConnectError("Error: Connection activation failed: (5) IP configuration could not be reserved (no available address, timeout, etc.).") == "Joined the network but got no address.");
    CHECK(ely::ExplainConnectError("Error: Timeout expired (90 seconds)") == "The network did not answer in time.");
    CHECK(ely::ExplainConnectError("Error: something odd\n") == "something odd");
    CHECK(ely::ExplainConnectError("") == "Could not connect.");
}

static void TestUpdateStatus()
{
    auto s = ely::ParseUpdateStatus("state=ready\nmessage=Update ready\ninstalled=abc1234\nstaged=def5678\npid=42\ntime=1760000000\n");
    CHECK(s.valid && s.state == "ready" && s.message == "Update ready");
    CHECK(s.installed == "abc1234" && s.staged == "def5678" && s.pid == 42 && s.time == 1760000000);
    CHECK(!ely::ParseUpdateStatus("").valid);
    CHECK(!ely::ParseUpdateStatus("garbage").valid);

    CHECK(ely::ParseInstalledCommit("platform 4a5267f049bbc8e81954839c442118bfc92e2342 (https://x, main)\nabc") == "4a5267f");
    CHECK(ely::ParseInstalledCommit("nothing").empty());
    CHECK(ely::ParseInstalledCommit("platform zzzzzzzz").empty());
}

static void TestProcess()
{
#if !defined(_WIN32)
    auto wait = [](ely::AsyncCommand& c) {
        for (int i = 0; i < 400 && !c.Update(); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    };
    ely::AsyncCommand c;
    CHECK(!c.Update());                                           // nothing started
    CHECK(c.Start({ "sh", "-c", "echo out; echo err >&2; exit 3" }));
    wait(c);
    CHECK(c.Finished() && c.ExitCode() == 3);
    CHECK(c.Output().find("out") != std::string::npos && c.Output().find("err") != std::string::npos);

    CHECK(c.Start({ "sh", "-c", "echo $LC_ALL; cat" }));          // stdin is /dev/null, LC_ALL=C
    wait(c);
    CHECK(c.ExitCode() == 0 && c.Output() == "C\n");

    CHECK(c.Start({ "sh", "-c", "read a; read b; echo \"$b/$a\"" }, 5.0, "first\nsecond word\n"));   // stdin text
    wait(c);
    CHECK(c.ExitCode() == 0 && c.Output() == "second word/first\n");

    CHECK(c.Start({ "sh", "-c", "sleep 5" }, 0.3));               // timeout kills it
    wait(c);
    CHECK(c.Finished() && c.ExitCode() == -1);

    CHECK(c.Start({ "this-command-does-not-exist" }));            // exec failure -> exit 127
    wait(c);
    CHECK(c.ExitCode() == 127);

    CHECK(c.Start({ "sh", "-c", "sleep 5" }));                    // cancel
    c.Cancel();
    CHECK(!c.Running() && !c.Finished());

    CHECK(c.Start({ "sh", "-c", "head -c 600000 /dev/zero | tr '\\0' x" }));   // output is capped
    wait(c);
    CHECK(c.ExitCode() == 0 && c.Output().size() <= 256 * 1024 + 4096);
#endif
}

int main()
{
    TestSplit();
    TestDevices();
    TestWifiList();
    TestErrors();
    TestUpdateStatus();
    TestProcess();
    if (failures == 0) printf("ALL TESTS PASSED\n");
    return failures == 0 ? 0 : 1;
}
