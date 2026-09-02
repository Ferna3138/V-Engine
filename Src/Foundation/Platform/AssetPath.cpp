#include "Foundation/Platform/AssetPath.hpp"

#include <system_error>

#if defined(_WIN32)
    #include <windows.h>
#elif defined(__APPLE__)
    #include <mach-o/dyld.h>
#elif defined(__linux__)
    #include <unistd.h>
#endif

namespace vengine {

namespace {

namespace fs = std::filesystem;

// Location of the running executable, or an empty path if it can't be queried.
fs::path executablePath() {
#if defined(_WIN32)
    wchar_t buffer[MAX_PATH];
    DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (length == 0 || length == MAX_PATH)
        return {};
    return fs::path(buffer, buffer + length);
#elif defined(__APPLE__)
    char buffer[4096];
    uint32_t size = sizeof(buffer);
    if (_NSGetExecutablePath(buffer, &size) != 0)
        return {};
    std::error_code ec;
    fs::path resolved = fs::canonical(buffer, ec);
    return ec ? fs::path(buffer) : resolved;
#elif defined(__linux__)
    std::error_code ec;
    fs::path resolved = fs::canonical("/proc/self/exe", ec);
    return ec ? fs::path{} : resolved;
#else
    return {};
#endif
}

// Walks up from `start` looking for the directory that holds the project (it
// contains both "Src" and "Models"). Returns an empty path if none is found.
fs::path findRootFrom(fs::path start) {
    std::error_code ec;
    if (start.empty())
        return {};
    if (fs::is_regular_file(start, ec))
        start = start.parent_path();

    for (fs::path dir = start; !dir.empty(); dir = dir.parent_path()) {
        if (fs::is_directory(dir / "Src", ec) && fs::is_directory(dir / "Models", ec))
            return dir;
        if (dir == dir.root_path())
            break;
    }
    return {};
}

fs::path discoverProjectRoot() {
    if (fs::path root = findRootFrom(executablePath()); !root.empty())
        return root;

    std::error_code ec;
    if (fs::path root = findRootFrom(fs::current_path(ec)); !ec && !root.empty())
        return root;

    // Last resort: assume the working directory is already the root.
    return fs::current_path(ec);
}

} // namespace

const fs::path& projectRoot() {
    static const fs::path root = discoverProjectRoot();
    return root;
}

std::string resolveAssetPath(const std::string& path) {
    fs::path p(path);
    if (p.is_absolute())
        return p.string();
    return (projectRoot() / p).lexically_normal().string();
}

} // namespace vengine
