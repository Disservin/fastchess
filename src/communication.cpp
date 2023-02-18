#include "communication.h"

Process::Process(const std::string &command)
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

Process::~Process()
{
    CloseHandle(m_childProcessHandle);
    CloseHandle(m_childStdOut);
    CloseHandle(m_childStdIn);
}

std::vector<std::string> Process::Read(std::string_view last_word)
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

void Process::Write(const std::string &input)
{
    DWORD bytesWritten;
    WriteFile(m_childStdIn, input.c_str(), input.length(), &bytesWritten, nullptr);
}