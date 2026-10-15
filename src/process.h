// Arcade Menu -- src/process.h
//
// Runs ONE external command in the background so the menu's frame loop never blocks on it
// (nmcli can take 20+ seconds to join a network). Used by the Wi-Fi and update screens.
//
//     ely::AsyncCommand cmd;
//     cmd.Start({ "nmcli", "-t", "device" });                       // optional 3rd argument: text for its stdin
//     ...each frame:  if (cmd.Update()) { use cmd.ExitCode() and cmd.Output(); }
//
// stdout and stderr are captured together. The command runs with LC_ALL=C (stable output to
// parse). Its stdin is /dev/null unless text is given (used for Wi-Fi passwords: unlike an
// argument, stdin is not visible in `ps`). On Windows (development builds) Start() always fails.

#pragma once

#include <string>
#include <vector>

namespace ely
{

class AsyncCommand
{
public:
    AsyncCommand() = default;
    ~AsyncCommand();
    AsyncCommand(const AsyncCommand&) = delete;
    AsyncCommand& operator=(const AsyncCommand&) = delete;

    // Starts the command (argv[0] is looked up in PATH). Returns false if it could not be started;
    // Update() then reports it finished with ExitCode() == -1. A command still running after
    // timeoutSeconds is killed. Any command that is still running is cancelled first.
    bool Start(const std::vector<std::string>& argv, double timeoutSeconds = 60.0,
               const std::string& stdinText = std::string());

    // Call every frame while a command runs. Returns true once it has finished (and keeps returning
    // true until the next Start()). Returns false if nothing was started.
    bool Update();

    bool Running() const { return running_; }
    bool Finished() const { return finished_; }

    // 0 = success; -1 = could not start, was killed or cancelled; otherwise the command's exit code.
    int ExitCode() const { return exitCode_; }
    const std::string& Output() const { return output_; }

    // Kills a running command and forgets it.
    void Cancel();

private:
    void Finish(int exitCode);

    int pid_ = -1;
    int fd_ = -1;
    bool running_ = false;
    bool finished_ = false;
    bool eof_ = false;
    int exitCode_ = -1;
    double deadline_ = 0.0;
    std::string output_;
};

} // namespace ely
