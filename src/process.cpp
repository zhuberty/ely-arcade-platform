// Arcade Menu -- src/process.cpp  (see process.h)

#include "process.h"

#include <cstring>

#if !defined(_WIN32)
    #include <cerrno>
    #include <csignal>
    #include <ctime>
    #include <fcntl.h>
    #include <sys/types.h>
    #include <sys/wait.h>
    #include <unistd.h>
    extern char** environ;
#endif

namespace ely
{

#if defined(_WIN32)

AsyncCommand::~AsyncCommand() {}
bool AsyncCommand::Start(const std::vector<std::string>&, double, const std::string&) { Finish(-1); return false; }
bool AsyncCommand::Update() { return finished_; }
void AsyncCommand::Cancel() { running_ = false; finished_ = false; }
void AsyncCommand::Finish(int exitCode) { running_ = false; finished_ = true; exitCode_ = exitCode; }

#else

static double NowSeconds()
{
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

// Output beyond this is dropped (a runaway command must not eat the cabinet's memory).
static const size_t MAX_OUTPUT = 256 * 1024;

AsyncCommand::~AsyncCommand()
{
    Cancel();
}

void AsyncCommand::Finish(int exitCode)
{
    if (fd_ >= 0) { close(fd_); fd_ = -1; }
    pid_ = -1;
    running_ = false;
    finished_ = true;
    exitCode_ = exitCode;
}

bool AsyncCommand::Start(const std::vector<std::string>& argv, double timeoutSeconds, const std::string& stdinText)
{
    Cancel();
    output_.clear();
    eof_ = false;
    exitCode_ = -1;
    finished_ = false;
    if (argv.empty()) { Finish(-1); return false; }

    // Everything the child needs is prepared BEFORE fork(), so the child only calls exec.
    std::vector<char*> args;
    for (const std::string& a : argv) args.push_back(const_cast<char*>(a.c_str()));
    args.push_back(nullptr);

    std::vector<std::string> envStore;
    for (char** e = environ; e && *e; ++e)
        if (strncmp(*e, "LC_ALL=", 7) != 0) envStore.push_back(*e);
    envStore.push_back("LC_ALL=C");
    std::vector<char*> env;
    for (std::string& e : envStore) env.push_back(const_cast<char*>(e.c_str()));
    env.push_back(nullptr);

    int fds[2];
    if (pipe2(fds, O_CLOEXEC) != 0) { Finish(-1); return false; }

    // The command's stdin: a pipe that already holds the text (small texts fit the pipe buffer, so
    // writing it here cannot block), or /dev/null.
    int in = -1;
    if (!stdinText.empty() && stdinText.size() <= 4096)
    {
        int inFds[2];
        if (pipe2(inFds, O_CLOEXEC) == 0)
        {
            ssize_t w = write(inFds[1], stdinText.data(), stdinText.size());
            close(inFds[1]);
            if (w == (ssize_t)stdinText.size()) in = inFds[0]; else close(inFds[0]);
        }
    }
    if (in < 0) in = open("/dev/null", O_RDONLY | O_CLOEXEC);

    pid_t pid = fork();
    if (pid < 0)
    {
        close(fds[0]);
        close(fds[1]);
        if (in >= 0) close(in);
        Finish(-1);
        return false;
    }
    if (pid == 0)
    {
        if (in >= 0) dup2(in, 0);
        dup2(fds[1], 1);   // dup2 clears close-on-exec on the new descriptor
        dup2(fds[1], 2);
        execvpe(args[0], args.data(), env.data());
        _exit(127);
    }

    close(fds[1]);
    if (in >= 0) close(in);
    fcntl(fds[0], F_SETFL, O_NONBLOCK);
    fd_ = fds[0];
    pid_ = pid;
    running_ = true;
    deadline_ = NowSeconds() + timeoutSeconds;
    return true;
}

bool AsyncCommand::Update()
{
    if (!running_) return finished_;

    if (!eof_)
    {
        char buf[4096];
        for (;;)
        {
            ssize_t n = read(fd_, buf, sizeof(buf));
            if (n > 0)
            {
                if (output_.size() < MAX_OUTPUT) output_.append(buf, (size_t)n);
                continue;
            }
            if (n == 0) eof_ = true;                        // the command closed its output
            else if (errno == EINTR) continue;
            break;                                          // EAGAIN: nothing more right now
        }
    }

    int status = 0;
    pid_t r = waitpid(pid_, &status, WNOHANG);
    if (r == pid_)
    {
        // Drain what was written just before the exit.
        char buf[4096];
        ssize_t n;
        while ((n = read(fd_, buf, sizeof(buf))) > 0)
            if (output_.size() < MAX_OUTPUT) output_.append(buf, (size_t)n);
        Finish(WIFEXITED(status) ? WEXITSTATUS(status) : -1);
        return true;
    }
    if (r < 0 && errno != EINTR)
    {
        Finish(-1);
        return true;
    }

    if (NowSeconds() > deadline_)
    {
        kill(pid_, SIGKILL);
        waitpid(pid_, &status, 0);
        output_ += "\n(timed out)";
        Finish(-1);
        return true;
    }
    return false;
}

void AsyncCommand::Cancel()
{
    if (running_ && pid_ > 0)
    {
        int status = 0;
        kill(pid_, SIGKILL);
        waitpid(pid_, &status, 0);
    }
    if (fd_ >= 0) { close(fd_); fd_ = -1; }
    pid_ = -1;
    running_ = false;
    finished_ = false;
}

#endif

} // namespace ely
