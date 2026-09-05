#pragma once

#include <string>

class Scene;
class Device;
class TextureManager;
class MaterialManager;

namespace vengine {

// Reads a scene description (JSON) and populates `scene` with entities: a
// camera, models and point lights. `path` is resolved against the project root
// (see Foundation/Platform/AssetPath). Models are created through `device` /
// `textureManager` / `materialManager`; a saved "materials" array on a model
// entity (see saveScene) is re-applied as overrides on top of the file's
// defaults. Returns false and logs on a missing or malformed file; entities
// created before the error are left in place.
bool loadScene(const std::string& path, Scene& scene,
               Device& device, TextureManager& textureManager, MaterialManager& materialManager);

// Writes every serializable entity in `scene` back to `path` (also project-root
// relative), producing a file loadScene() can read back. `materialManager` is
// read to serialise each model entity's current material state. Returns false
// on an I/O error.
bool saveScene(const std::string& path, const Scene& scene, MaterialManager& materialManager);

} // namespace vengine
