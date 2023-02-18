#include "engineprocess.h"

#ifdef _WIN64

EngineProcess::EngineProcess(const std::string &command)
{
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

    m_childProcessHandle = pi.hProcess;
    m_childStdOut = childStdOutRd;
    m_childStdIn = childStdInWr;
}

EngineProcess::~EngineProcess()
{
    CloseHandle(m_childProcessHandle);
    CloseHandle(m_childStdOut);
    CloseHandle(m_childStdIn);
}

std::vector<std::string> EngineProcess::readEngine(std::string_view last_word)
{
    std::vector<std::string> lines;
    std::string currentLine;
    char buffer[1024];
    DWORD bytesRead;

    while (true)
    {
        if (!ReadFile(m_childStdOut, buffer, sizeof(buffer), &bytesRead, nullptr))
            break;

        if (bytesRead == 0)
            break;

        // Iterate over each character in the buffer
        for (DWORD i = 0; i < bytesRead; i++)
        {
            if (currentLine == last_word)
            {
                lines.push_back(currentLine);
                return lines;
            }

            // If we encounter a newline, add the current line to the vector and start a new one
            if (buffer[i] == '\n')
            {
                lines.push_back(currentLine);
                currentLine.clear();
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

void EngineProcess::writeEngine(const std::string &input)
{
    constexpr char endLine = '\n';
    DWORD bytesWritten;
    WriteFile(m_childStdIn, input.c_str(), input.length(), &bytesWritten, nullptr);
    WriteFile(m_childStdIn, &endLine, 1, &bytesWritten, nullptr);
}

#else

#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

EngineProcess::EngineProcess(const std::string &command)
{

    if (pipe(inPipe) == -1)
    {
        perror("Failed to create input pipe");
        exit(1);
    }

    if (pipe(outPipe) == -1)
    {
        perror("Failed to create output pipe");
        exit(1);
    }

    pid_t processPid = fork();
    if (processPid < 0)
    {
        perror("Fork failed");
        exit(1);
    }

    if (processPid == 0)
    {
        dup2(outPipe[0], 0);
        close(outPipe[0]);
        close(outPipe[1]);

        dup2(inPipe[1], 1);
        close(inPipe[0]);
        close(inPipe[1]);
        execlp(command.c_str(), command.c_str(), (char *)0);
        perror("Failed to create child process");
        exit(1);
    }
}

EngineProcess::~EngineProcess()
{
    close(inPipe[0]);
    close(inPipe[1]);
    close(outPipe[0]);
    close(outPipe[1]);
}

void EngineProcess::writeEngine(const std::string &input)
{
    constexpr char endLine = '\n';
    close(outPipe[0]);
    write(outPipe[1], input.c_str(), input.size());
    write(outPipe[1], &endLine, 1);
}

std::vector<std::string> EngineProcess::readEngine(std::string_view last_word)
{

    // Disable blocking
    fcntl(inPipe[0], F_SETFL, fcntl(inPipe[0], F_GETFL) | O_NONBLOCK);

    std::vector<std::string> lines;
    std::string line;

    while (line != last_word)
    {
        line = "";

        char c = ' ';
        while (c != '\n')
        {
            if (read(inPipe[0], &c, 1) > 0 && c != '\n')
                line += c;
        }

        lines.push_back(line);
    }

    return lines;
}

#endif