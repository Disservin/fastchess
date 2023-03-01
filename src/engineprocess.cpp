#include <cassert>
#include <chrono>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <thread>

#include "engineprocess.h"
#include "options.h"
#include "types.h"

EngineProcess::EngineProcess(const std::string &command)
{
    initProcess(command);
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

    CreateProcessA(nullptr, const_cast<char *>(command.c_str()), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi);

    CloseHandle(childStdOutWr);
    CloseHandle(childStdInRd);

    childStdOut = childStdOutRd;
    childStdIn = childStdInWr;
}

void EngineProcess::closeHandles()
{
    try
    {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);

        CloseHandle(childStdOut);
        CloseHandle(childStdIn);
    }
    catch (const std::exception &)
    {
        errCode = 1;
        errStr = "Error in closing handles.";
    }
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
    currentLine.reserve(300);

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
                errCode = 1;
                errStr = "Cant peek pipe.";
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
            errCode = 1;
            errStr = "Cant read process correctly.";
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
                    Logging::Log->readLog(currentLine, std::this_thread::get_id());

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

    try
    {
        if (!isAlive())
        {
            closeHandles();

            std::stringstream ss;
            ss << "Process is not alive and write occured with message: " << input << "\n";
            std::cout << ss.str();
            return;
        }

        Logging::Log->writeLog(input, std::this_thread::get_id());

        constexpr char endLine = '\n';
        DWORD bytesWritten;
        WriteFile(childStdIn, input.c_str(), input.length(), &bytesWritten, nullptr);
        WriteFile(childStdIn, &endLine, 1, &bytesWritten, nullptr);
    }
    catch (const std::exception &e)
    {
        errCode = 1;
        errStr = "Error in writing to process.\n" + std::string(e.what());
    }
}

bool EngineProcess::isAlive()
{
    assert(isInitalized);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    return exitCode == STILL_ACTIVE;
}

void EngineProcess::killProcess()
{
    if (isInitalized)
    {
        try
        {
            DWORD exitCode = 0;
            GetExitCodeProcess(pi.hProcess, &exitCode);
            if (exitCode == STILL_ACTIVE)
            {
                UINT uExitCode = 0;
                TerminateProcess(pi.hProcess, uExitCode);
            }
            // Clean up the child process resources
            closeHandles();
        }
        catch (const std::exception &e)
        {
            errCode = 1;
            errStr = "Error in writing to process.\n" + std::string(e.what());
        }
    }
}
#else

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <string.h>
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
        perror(strerror(errno));
        exit(1);
    }

    // If this is the child process, set up the pipes and start the engine
    if (forkPid == 0)
    {
        // Redirect the child's standard input to the read end of the output pipe
        if (dup2(outPipe[0], 0) == -1)
            perror("Failed to duplicate outpipe");

        if (close(outPipe[0]) == -1)
            perror("Failed to close outpipe");

        // Redirect the child's standard output to the write end of the input pipe
        if (dup2(inPipe[1], 1) == -1)
            perror("Failed to duplicate inpipe");

        if (close(inPipe[1]) == -1)
            perror("Failed to close inpipe");

        // Execute the engine
        if (execl(command.c_str(), command.c_str(), nullptr) == -1)
            perror(strerror(errno));

        perror(strerror(errno));
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
        errCode = 1;
        errStr = "Error in writing process.";

        std::stringstream ss;
        ss << "Process is not alive and write occured with message: " << input << "\n";
        std::cout << ss.str();
        return;
    }

    // Append a newline character to the end of the input string
    constexpr char endLine = '\n';

    // Write the input and a newline to the output pipe
    if (write(outPipe[1], input.c_str(), input.size()) == -1)
    {
        perror(strerror(errno));
        std::stringstream ss;
        ss << "Process is not alive and write occured with message: " << input;
        std::cout << ss.str();
        errCode = 1;
        errStr = ss.str();
    }

    if (write(outPipe[1], &endLine, 1) == -1)
    {
        perror(strerror(errno));
        std::stringstream ss;
        ss << "Process is not alive and write occured with message: " << input;
        std::cout << ss.str();
        errCode = 1;
        errStr = ss.str();
    }
}
std::vector<std::string> EngineProcess::readProcess(std::string_view last_word, bool &timeout, int64_t timeoutThreshold)
{
    assert(isInitalized);

    // Disable blocking
    fcntl(inPipe[0], F_SETFL, fcntl(inPipe[0], F_GETFL) | O_NONBLOCK);

    std::vector<std::string> lines;
    lines.reserve(30);

    std::string currentLine;
    currentLine.reserve(300);

    char buffer[4096];
    timeout = false;

    struct pollfd pollfds[1];
    pollfds[0].fd = inPipe[0];
    pollfds[0].events = POLLIN;

    // Set up the timeout for poll
    int timeoutMillis = timeoutThreshold;
    if (timeoutMillis <= 0)
    {
        timeoutMillis = -1; // wait indefinitely
    }

    // Continue reading output lines until the line matches the specified line or a timeout occurs
    while (true)
    {
        const int ret = poll(pollfds, 1, timeoutMillis);

        if (ret == -1)
        {
            perror(strerror(errno));
            errCode = 1;
            errStr = strerror(errno);
        }
        else if (ret == 0)
        {
            // timeout
            lines.emplace_back(currentLine);
            timeout = true;
            break;
        }
        else if (pollfds[0].revents & POLLIN)
        {
            // input available on the pipe
            const int bytesRead = read(inPipe[0], buffer, sizeof(buffer));

            if (bytesRead == -1)
            {
                perror(strerror(errno));
                errCode = 1;
                errStr = strerror(errno);
            }
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

    const pid_t r = waitpid(processPid, &status, WNOHANG);
    if (r == -1)
    {
        perror(strerror(errno));
        errCode = 1;
        errStr = strerror(errno);
        return false;
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

        if (r == 0)
        {
            kill(processPid, SIGKILL);
            wait(nullptr);
        }
    }
}

#endif
