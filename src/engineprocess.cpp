#include <cassert>
#include <chrono>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include "engineprocess.h"
#include "options.h"
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

    std::string currentLine;

    char buffer[4096];
    DWORD bytesRead;
    DWORD bytesAvail = 0;

    int checkTime = 255;

    timeout = false;

    auto start = std::chrono::high_resolution_clock::now();

    while (true)
    {

        // Check if timeout milliseconds have elapsed
        if (checkTime-- == 0)
        {
            /* To achieve "non blocking" file reading on windows with anonymous pipes the only solution
            that I found was using peeknamedpipe however it turns out this function is terribly slow and leads to
            timeouts for the engines. Checking this only after n runs seems to reduce the impact of this.
            For high concurrency windows setups timeoutThreshold should probably be 0. Using
            the assumption that the engine works rather clean and is able to send the last word.*/
            if (!PeekNamedPipe(childStdOut, NULL, 0, 0, &bytesAvail, nullptr))
            {
                throw std::runtime_error("Cant peek Pipe");
            }

            if (timeoutThreshold > 0 &&
                std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start)
                        .count() > timeoutThreshold)
            {
                lines.emplace_back(currentLine);
                timeout = true;
                break;
            }

            checkTime = 255;
        }

        // no new bytes to read
        if (timeoutThreshold && bytesAvail == 0)
            continue;

        if (!ReadFile(childStdOut, buffer, sizeof(buffer), &bytesRead, nullptr))
        {
            throw std::runtime_error("Cant read process correctly");
        }

        // Iterate over each character in the buffer
        for (DWORD i = 0; i < bytesRead; i++)
        {
            // If we encounter a newline, add the current line to the vector and reset the currentLine
            // on windows newlines are \r\n
            if (buffer[i] == '\n' || buffer[i] == '\r')
            {
                // dont add empty lines
                if (!currentLine.empty())
                {
                    lines.emplace_back(currentLine);

                    if (currentLine.rfind(last_word, 0) == 0)
                    {
                        return lines;
                    }

                    currentLine = "";
                }
            }
            // Otherwise, append the character to the current line
            else
            {
                currentLine += buffer[i];
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

    std::string currentLine;

    char buffer[4096];
    int checkTime = 255;
    timeout = false;

    // Set up the file descriptor set for select
    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(inPipe[0], &readSet);

    // Set up the timeout for select
    struct timeval tm;
    tm.tv_sec = timeoutThreshold / 1000; // convert milliseconds to secs
    tm.tv_usec = (timeoutThreshold % 1000) * 1000;

    // Continue reading output lines until the line matches the specified line or a timeout occurs
    while (true)
    {
        int ret = select(inPipe[0] + 1, &readSet, nullptr, nullptr, (timeoutThreshold == 0 ? nullptr : &tm));

        if (ret == -1)
        {
            throw std::runtime_error("Select error occured in engineprocess.cpp");
        }
        else if (ret == 0)
        {
            // timeout
            lines.emplace_back(currentLine);
            timeout = true;
            break;
        }
        else
        {
            // input available on the pipe
            int bytesRead = read(inPipe[0], buffer, sizeof(buffer));

            // Iterate over each character in the buffer
            for (int i = 0; i < bytesRead; i++)
            {
                // If we encounter a newline, add the current line to the vector and reset the currentLine
                if (buffer[i] == '\n')
                {
                    // dont add empty lines
                    if (!currentLine.empty())
                    {
                        lines.emplace_back(currentLine);

                        if (currentLine.rfind(last_word, 0) == 0)
                        {
                            return lines;
                        }

                        currentLine = "";
                    }
                }
                // Otherwise, append the character to the current line
                else
                {
                    currentLine += buffer[i];
                }
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
        close(inPipe[0]);
        close(inPipe[1]);
        close(outPipe[0]);
        close(outPipe[1]);

        int status;
        pid_t r = waitpid(processPid, &status, WNOHANG);

        if (r == 0) {
            kill(processPid, SIGKILL);
            wait(nullptr);
        }
    }
}

#endif