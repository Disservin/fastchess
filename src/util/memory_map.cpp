#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#include <util/memory_map.hpp>

namespace fast_chess::util::memory {

#ifdef _WIN32
#    include <windows.h>

void WindowsMemoryMappedFile::mapFile(const std::string& filename) {
    fileHandle = CreateFileA(filename.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (fileHandle == INVALID_HANDLE_VALUE) throw std::runtime_error("Error opening file");

    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(fileHandle, &fileSize)) throw std::runtime_error("Error getting file size");

    size      = static_cast<size_t>(fileSize.QuadPart);
    mapHandle = CreateFileMappingA(fileHandle, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!mapHandle) throw std::runtime_error("Error creating file mapping");

    data = MapViewOfFile(mapHandle, FILE_MAP_READ, 0, 0, size);
    if (!data) throw std::runtime_error("Error mapping file");
}

void WindowsMemoryMappedFile::unmapFile() {
    if (data) {
        UnmapViewOfFile(data);
        data = nullptr;
    }
    if (mapHandle) {
        CloseHandle(mapHandle);
        mapHandle = nullptr;
    }
    if (fileHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(fileHandle);
        fileHandle = INVALID_HANDLE_VALUE;
    }
}

WindowsMemoryMappedFile::~WindowsMemoryMappedFile() { unmapFile(); }

#else

#    include <fcntl.h>
#    include <sys/mman.h>
#    include <sys/stat.h>
#    include <unistd.h>

void LinuxMemoryMappedFile::mapFile(const std::string& filename) {
    fd = open(filename.c_str(), O_RDONLY);
    if (fd == -1) throw std::runtime_error("Error opening file");

    struct stat sb;
    if (fstat(fd, &sb) == -1) throw std::runtime_error("Error getting file size");

    size = sb.st_size;
    data = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (data == MAP_FAILED) throw std::runtime_error("Error mapping file");
}

void LinuxMemoryMappedFile::unmapFile() {
    if (data) {
        munmap(data, size);
        data = nullptr;
    }
    if (fd != -1) {
        close(fd);
        fd = -1;
    }
}

LinuxMemoryMappedFile::~LinuxMemoryMappedFile() { unmapFile(); }
#endif

}  // namespace fast_chess::util::memory