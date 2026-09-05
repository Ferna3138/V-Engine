#pragma once

#include <optional>
#include <string>

namespace vengine {

// Opens a native "Open File" dialog filtered to supported model formats
// (.obj, .gltf, .glb). Blocks the calling thread until the dialog closes.
// Returns the chosen path, or nullopt if the user cancelled.
std::optional<std::string> openModelFileDialog();

// Same as openModelFileDialog(), filtered to image formats supported by the
// engine's texture loader (stb_image: png/jpg/jpeg/bmp/tga).
std::optional<std::string> openImageFileDialog();

}  // namespace vengine
