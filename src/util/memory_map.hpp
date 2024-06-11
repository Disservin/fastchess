#pragma once

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace fast_chess::util::memory {

class MemoryMappedFile {
   protected:
    void* data  = nullptr;
    size_t size = 0;

   public:
    virtual ~MemoryMappedFile() {}

    virtual void mapFile(const std::string& filename) = 0;
    virtual void unmapFile()                          = 0;

    void* getData() const {
        if (!data) throw std::runtime_error("Data is not mapped.");
        return data;
    }

    size_t getSize() const { return size; }
};

#ifdef _WIN32

class WindowsMemoryMappedFile : public MemoryMappedFile {
    void* fileHandle = nullptr;
    void* mapHandle  = nullptr;

   public:
    void mapFile(const std::string& filename) override;

    void unmapFile() override;

    ~WindowsMemoryMappedFile();
};

#else

// Linux-specific implementation
class LinuxMemoryMappedFile : public MemoryMappedFile {
    int fd = -1;

   public:
    void mapFile(const std::string& filename) override;
    void unmapFile() override;
    ~LinuxMemoryMappedFile();
};
#endif

inline std::unique_ptr<MemoryMappedFile> createMemoryMappedFile() {
#ifdef _WIN32
    return std::make_unique<WindowsMemoryMappedFile>();
#else
    return std::make_unique<LinuxMemoryMappedFile>();
#endif
}

}  // namespace fast_chess::util::memory