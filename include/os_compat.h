// Portable shims for CRT functions MSVC/clang-cl flag as "unsafe" (C4996 /
// -Wdeprecated-declarations). These are standard C/C++ functions; the _s
// replacements (C11 Annex K) are not available on Linux glibc, so wrap once:
// MSVC gets the _s form where it helps, everyone else the standard form.
#pragma once

#include <cstdio>
#include <cstdlib>

namespace recomp::os {

// std::getenv is standard and portable; only MSVC deprecates it. Suppress at
// this single site (clang-cl needs the clang pragma; plain cl needs C4996).
inline const char* getenv(const char* name) {
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#elif defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4996)
#endif
    return std::getenv(name);
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif
}

// Returns nullptr on failure, matching std::fopen. Uses fopen_s on MSVC so no
// deprecation warning; std::fopen elsewhere.
inline FILE* fopen(const char* path, const char* mode) {
#if defined(_MSC_VER)
    FILE* f = nullptr;
    if (::fopen_s(&f, path, mode) != 0) return nullptr;
    return f;
#else
    return std::fopen(path, mode);
#endif
}

} // namespace recomp::os
