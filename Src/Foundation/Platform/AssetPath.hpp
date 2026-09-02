#pragma once

#include <filesystem>
#include <string>

namespace vengine {

// Absolute path to the project root, discovered once at first use by walking up
// from the executable location (and, as a fallback, the current working
// directory) until a directory containing both "Src" and "Models" is found.
// This makes asset loading independent of where the terminal / debugger sets the
// working directory (e.g. V-Engine, V-Engine/build, V-Engine/build/Debug).
const std::filesystem::path& projectRoot();

// Resolves a path that is written relative to the project root into an absolute
// path. Absolute inputs are returned unchanged. Does not check for existence.
std::string resolveAssetPath(const std::string& path);

} // namespace vengine
