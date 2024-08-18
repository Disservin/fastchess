#pragma once

namespace fastchess::util {

#if defined(__MINGW32__) && defined(__GNUC__) && __GNUC__ == 8
#    define NO_STD_FILESYSTEM
#endif

#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ < 8
#    define NO_STD_FILESYSTEM
#endif

#if defined(__clang_major__) && __clang_major__ < 7
#    define NO_STD_FILESYSTEM
#endif

#if defined(_MSC_VER) && _MSC_VER < 1914
#    define NO_STD_FILESYSTEM
#endif

#if defined(__IPHONE_OS_VERSION_MIN_REQUIRED) && __IPHONE_OS_VERSION_MIN_REQUIRED < 130000
#    define NO_STD_FILESYSTEM
#endif

#if defined(__MAC_OS_X_VERSION_MIN_REQUIRED) && __MAC_OS_X_VERSION_MIN_REQUIRED < 101500
#    define NO_STD_FILESYSTEM
#endif

#ifndef NO_STD_FILESYSTEM

#    if defined(__cpp_lib_filesystem)
#        include <filesystem>
#    elif defined(__cpp_lib_experimental_filesystem)
#        include <experimental/filesystem>
#    else

#        if defined(__has_include)
#            if __has_include(<filesystem>)
#                include <filesystem>
#            elif __has_include(<experimental/filesystem>)
#                include <experimental/filesystem>
#            endif
#        endif
#    endif
#endif

}  // namespace fastchess::util
