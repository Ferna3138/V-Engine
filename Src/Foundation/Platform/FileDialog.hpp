#pragma once

#include <optional>
#include <string>

namespace vengine {

// Opens a native "Open File" dialog filtered to supported model formats
// (.obj, .gltf, .glb). Blocks the calling thread until the dialog closes.
// Returns the chosen path, or nullopt if the user cancelled.
std::optional<std::string> openModelFileDialog();

}  // namespace vengine
