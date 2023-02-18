#include <iostream>
#include <string>
#include <vector>
#include <windows.h>

class Process
{
  public:
    Process(const std::string &command);

    ~Process();

    // reads until it finds last_word
    std::vector<std::string> Read(std::string_view last_word);

    void Write(const std::string &input);

  private:
    HANDLE m_childProcessHandle;
    HANDLE m_childStdOut;
    HANDLE m_childStdIn;
};
