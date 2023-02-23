#include <cassert>
#include <sstream>
#include <stdexcept>
#include <iostream>

#include "engineprocess.h"
#include "helper.h"
#include "types.h"

EngineProcess::EngineProcess(const std::string &command)
{
    initProcess(command);
}

bool Process::isResponsive()
{
    assert(isInitalized);
    if (!isAlive())
        return false;

    bool timeout = false;
    writeProcess("isready");
    readProcess("readyok", timeout, PING_TIMEOUT_THRESHOLD);
    return !timeout;
}

#ifdef _WIN64

void EngineProcess::initProcess(const std::string &command)
{
    isInitalized = true;
    STARTUPINFOA si = STARTUPINFOA();
    si.dwFlags = STARTF_USESTDHANDLES;

    SECURITY_ATTRIBUTES sa = SECURITY_ATTRIBUTES();
    sa.bInheritHandle = TRUE;

    HANDLE childStdOutRd, childStdOutWr;
    CreatePipe(&childStdOutRd, &childStdOutWr, &sa, 0);
    SetHandleInformation(childStdOutRd, HANDLE_FLAG_INHERIT, 0);
    si.hStdOutput = childStdOutWr;

    HANDLE childStdInRd, childStdInWr;
    CreatePipe(&childStdInRd, &childStdInWr, &sa, 0);
    SetHandleInformation(childStdInWr, HANDLE_FLAG_INHERIT, 0);
    si.hStdInput = childStdInRd;

    PROCESS_INFORMATION pi = PROCESS_INFORMATION();
    CreateProcessA(nullptr, const_cast<char *>(command.c_str()), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi);

    CloseHandle(childStdOutWr);
    CloseHandle(childStdInRd);

    childdwProcessId = pi.dwProcessId;
    childProcessHandle = pi.hProcess;
    childStdOut = childStdOutRd;
    childStdIn = childStdInWr;
}

void EngineProcess::closeHandles()
{
    CloseHandle(childProcessHandle);
    CloseHandle(childStdOut);
    CloseHandle(childStdIn);
}

EngineProcess::~EngineProcess()
{
    killProcess();
}

std::vector<std::string> EngineProcess::readProcess(std::string_view last_word, bool &timeout, int64_t timeoutThreshold)
{
    assert(isInitalized);

    std::vector<std::string> lines;
    lines.reserve(30);

    std::stringstream currentLine{};

    char buffer[4096];
    DWORD bytesRead;
    DWORD bytesAvail;

    int checkTime = 255;

    timeout = false;

    auto start = std::chrono::high_resolution_clock::now();

    while (true)
    {
        if (!PeekNamedPipe(childStdOut, buffer, sizeof(buffer), &bytesRead, &bytesAvail, nullptr))
        {
            throw std::runtime_error("Cant peek Pipe");
        }

        // Check if timeout milliseconds have elapsed
        if (checkTime-- == 0)
        {
            auto now = std::chrono::high_resolution_clock::now();

            if (timeoutThreshold > 0 &&
                std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count() > timeoutThreshold)
            {
                lines.emplace_back(currentLine.str());
                timeout = true;
                break;
            }

            checkTime = 255;
        }

        // no new bytes to read
        if (bytesAvail == 0)
            continue;

        if (!ReadFile(childStdOut, buffer, sizeof(buffer), &bytesRead, nullptr))
        {
            throw std::runtime_error("Cant read process correctly");
        }

        // this is actually an error. There are bytes to read but we read zero.
        if (bytesRead == 0)
            break;

        // Iterate over each character in the buffer
        for (DWORD i = 0; i < bytesRead; i++)
        {
            // If we encounter a newline, add the current line to the vector and reset the currentLine
            // on windows newlines are \r\n
            if (buffer[i] == '\n' || buffer[i] == '\r')
            {
                // dont add empty lines
                if (!currentLine.str().empty())
                {
                    lines.emplace_back(currentLine.str());

                    if (contains(currentLine.str(), last_word))
                    {
                        return lines;
                    }

                    currentLine.str(std::string());
                }
            }
            // Otherwise, append the character to the current line
            else
            {
                currentLine << buffer[i];
            }
        }
    }

    return lines;
}

void EngineProcess::writeProcess(const std::string &input)
{
    assert(isInitalized);
    if (!isAlive())
    {
        closeHandles();

        std::stringstream ss;
        ss << "Trying to write to process with message: " << input;
        throw std::runtime_error(ss.str());
    }

    constexpr char endLine = '\n';
    DWORD bytesWritten;
    WriteFile(childStdIn, input.c_str(), input.length(), &bytesWritten, nullptr);
    WriteFile(childStdIn, &endLine, 1, &bytesWritten, nullptr);
}

bool EngineProcess::isAlive()
{
    assert(isInitalized);
    DWORD exitCode = 0;
    GetExitCodeProcess(childProcessHandle, &exitCode);
    return exitCode == STILL_ACTIVE;
}

void EngineProcess::killProcess()
{
    if (isInitalized)
    {
        DWORD exitCode = 0;
        GetExitCodeProcess(childProcessHandle, &exitCode);
        if (exitCode == STILL_ACTIVE)
        {
            UINT uExitCode = 0;
            TerminateProcess(childProcessHandle, uExitCode);
        }
        // Clean up the child process resources
        closeHandles();
    }
}
#else

#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

void EngineProcess::initProcess(const std::string &command)
{
    isInitalized = true;
    // Create input pipe
    if (pipe(inPipe) == -1)
    {
        perror("Failed to create input pipe");
        exit(1);
    }

    // Create output pipe
    if (pipe(outPipe) == -1)
    {
        perror("Failed to create output pipe");
        exit(1);
    }

    // Fork the current process
    pid_t forkPid = fork();
    if (forkPid < 0)
    {
        perror("Fork failed");
        exit(1);
    }

    // If this is the child process, set up the pipes and start the engine
    if (forkPid == 0)
    {
        // Redirect the child's standard input to the read end of the output pipe
        dup2(outPipe[0], 0);
        close(outPipe[0]);

        // Redirect the child's standard output to the write end of the input pipe
        dup2(inPipe[1], 1);
        close(inPipe[1]);

        // Execute the engine
        execlp(command.c_str(), command.c_str(), nullptr);

        perror("Failed to create child process");
        exit(1);
    }
    else
    {
        processPid = forkPid;
    }
}

EngineProcess::~EngineProcess()
{
    killProcess();
}

void EngineProcess::writeProcess(const std::string &input)
{
    assert(isInitalized);

    if (!isAlive())
    {
        std::stringstream ss;
        ss << "Trying to write to process with message: " << input;
        throw std::runtime_error(ss.str());
    }

    // Append a newline character to the end of the input string
    constexpr char endLine = '\n';

    // Write the input and a newline to the output pipe
    write(outPipe[1], input.c_str(), input.size());
    write(outPipe[1], &endLine, 1);
}

std::vector<std::string> EngineProcess::readProcess(std::string_view last_word, bool &timeout, int64_t timeoutThreshold)
{
    assert(isInitalized);

    // Disable blocking
    fcntl(inPipe[0], F_SETFL, fcntl(inPipe[0], F_GETFL) | O_NONBLOCK);

    std::vector<std::string> lines;
    lines.reserve(30);

    std::stringstream currentLine;

    char buffer[4096];
    int bytesRead = 0;
    int checkTime = 255;

    timeout = false;

    // Get the current time in milliseconds since epoch
    auto start = std::chrono::high_resolution_clock::now();

    // Continue reading output lines until the line matches the specified line or a timeout occurs
    while (true)
    {
        // Check if timeout milliseconds have elapsed
        if (checkTime-- == 0)
        {
            auto now = std::chrono::high_resolution_clock::now();

            if (timeoutThreshold > 0 &&
                std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count() > timeoutThreshold)
            {
                lines.emplace_back(currentLine.str());
                timeout = true;
                break;
            }

            checkTime = 255;
        }

        bytesRead = read(inPipe[0], &buffer, sizeof(buffer));

        // no new bytes to read
        if (bytesRead == 0)
            continue;

        // Iterate over each character in the buffer
        for (int i = 0; i < bytesRead; i++)
        {
            // If we encounter a newline, add the current line to the vector and reset the currentLine
            if (buffer[i] == '\n')
            {
                // dont add empty lines
                if (!currentLine.str().empty())
                {
                    lines.emplace_back(currentLine.str());

                    if (contains(currentLine.str(), last_word))
                    {
                        return lines;
                    }

                    currentLine.str(std::string());
                }
            }
            // Otherwise, append the character to the current line
            else
            {
                currentLine << buffer[i];
            }
        }
    }

    return lines;
}

bool EngineProcess::isAlive()
{
    assert(isInitalized);
    int status;

    pid_t r = waitpid(processPid, &status, WNOHANG);
    if (r == -1)
    {
        perror("waitpid() error");
        exit(1);
    }
    else
    {
        return r == 0;
    }
}

void EngineProcess::killProcess()
{
    if (isInitalized)
    {
        int status;
        pid_t r = waitpid(processPid, &status, WNOHANG);
        if (r == 0)
            kill(processPid, SIGKILL);

        close(inPipe[0]);
        close(inPipe[1]);
        close(outPipe[0]);
        close(outPipe[1]);
    }
}

#endif