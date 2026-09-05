#include "Application/Scene/Scene_Serializer.hpp"

#include "Application/Scene/Scene.hpp"
#include "Application/Scene/Components.hpp"
#include "Rendering/Resources/MaterialManager.hpp"
#include "Rendering/Resources/Model.hpp"
#include "Rendering/Resources/TextureManager.hpp"
#include "Rendering/RHI/Device.hpp"
#include "Foundation/Platform/AssetPath.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "json.hpp"

#include <fstream>
#include <iostream>

using json = nlohmann::json;

namespace vengine {
namespace {

glm::vec2 readVec2(const json& j, const char* key, glm::vec2 fallback) {
    if (!j.contains(key)) return fallback;
    const auto& a = j.at(key);
    return {a.at(0).get<float>(), a.at(1).get<float>()};
}

glm::vec3 readVec3(const json& j, const char* key, glm::vec3 fallback) {
    if (!j.contains(key)) return fallback;
    const auto& a = j.at(key);
    return {a.at(0).get<float>(), a.at(1).get<float>(), a.at(2).get<float>()};
}

glm::vec4 readVec4(const json& j, const char* key, glm::vec4 fallback) {
    if (!j.contains(key)) return fallback;
    const auto& a = j.at(key);
    return {a.at(0).get<float>(), a.at(1).get<float>(), a.at(2).get<float>(), a.at(3).get<float>()};
}

void readTransform(const json& j, TransformComponent& transform) {
    transform.translation = readVec3(j, "translation", transform.translation);
    transform.scale = readVec3(j, "scale", transform.scale);
    glm::vec3 euler = readVec3(j, "rotationEuler", glm::degrees(glm::eulerAngles(transform.rotation)));
    transform.rotation = glm::quat(glm::radians(euler));
}

json writeVec2(glm::vec2 v) { return json::array({v.x, v.y}); }
json writeVec3(glm::vec3 v) { return json::array({v.x, v.y, v.z}); }
json writeVec4(glm::vec4 v) { return json::array({v.x, v.y, v.z, v.w}); }

json writeTransform(const TransformComponent& t) {
    return json{
        {"translation", writeVec3(t.translation)},
        {"rotationEuler", writeVec3(glm::degrees(glm::eulerAngles(t.rotation)))},
        {"scale", writeVec3(t.scale)},
    };
}

void readCameraParams(const json& j, CamParameters& p) {
    if (!j.is_object()) return;
    auto num = [&](const char* k, float& out) { if (j.contains(k)) out = j.at(k).get<float>(); };
    num("sensor_width", p.sensor_width);
    num("sensor_height", p.sensor_height);
    num("focal_length", p.focal_length);
    num("fov", p.fov);
    num("aperture", p.aperture);
    num("focus_distance", p.focus_distance);
    num("shutter_angle", p.shutter_angle);
    num("exposure", p.exposure);
    num("blade_rotation", p.blade_rotation);
    num("max_coc", p.max_coc);
    if (j.contains("dof_enabled")) p.dof_enabled = j.at("dof_enabled").get<bool>();
    if (j.contains("aperture_blades")) p.aperture_blades = j.at("aperture_blades").get<int>();
    if (j.contains("dof_samples")) p.dof_samples = j.at("dof_samples").get<int>();
    if (j.contains("white_balance")) p.white_balance = j.at("white_balance").get<int>();
    if (j.contains("motion_blur_enabled")) p.motion_blur_enabled = j.at("motion_blur_enabled").get<bool>();
    if (j.contains("mb_samples")) p.mb_samples = j.at("mb_samples").get<int>();
    num("mb_max_px", p.mb_max_px);
}

json writeCameraParams(const CamParameters& p) {
    return json{
        {"sensor_width", p.sensor_width},
        {"sensor_height", p.sensor_height},
        {"focal_length", p.focal_length},
        {"fov", p.fov},
        {"aperture", p.aperture},
        {"focus_distance", p.focus_distance},
        {"shutter_angle", p.shutter_angle},
        {"exposure", p.exposure},
        {"dof_enabled", p.dof_enabled},
        {"aperture_blades", p.aperture_blades},
        {"blade_rotation", p.blade_rotation},
        {"dof_samples", p.dof_samples},
        {"max_coc", p.max_coc},
        {"white_balance", p.white_balance},
        {"motion_blur_enabled", p.motion_blur_enabled},
        {"mb_samples", p.mb_samples},
        {"mb_max_px", p.mb_max_px}
    };
}

void readTextureTransform(const json& j, TextureTransform& t) {
    t.offset = readVec2(j, "offset", t.offset);
    t.scale = readVec2(j, "scale", t.scale);
    t.rotationDegrees = j.value("rotationDegrees", t.rotationDegrees);
    t.uniformScaleLocked = j.value("uniformScaleLocked", t.uniformScaleLocked);
    t.wrapMode = static_cast<TextureWrapMode>(j.value("wrapMode", static_cast<int32_t>(t.wrapMode)));
}

json writeTextureTransform(const TextureTransform& t) {
    return json{
        {"offset", writeVec2(t.offset)},
        {"scale", writeVec2(t.scale)},
        {"rotationDegrees", t.rotationDegrees},
        {"uniformScaleLocked", t.uniformScaleLocked},
        {"wrapMode", static_cast<int32_t>(t.wrapMode)},
    };
}

// Applies a saved per-material override on top of whatever the loader already
// resolved from the source file's defaults. A texture path only takes effect
// if present and non-empty - re-resolving it registers a (possibly new)
// bindless slot, exactly like a hot-swap through the Inspector would.
void readMaterialOverride(const json& j, Material& mat, TextureManager& textureManager) {
    mat.baseColour = readVec3(j, "baseColour", mat.baseColour);
    mat.metallic = j.value("metallic", mat.metallic);
    mat.roughness = j.value("roughness", mat.roughness);
    // mat.globalTransform is intentionally not persisted - it's session-only
    // bookkeeping for the Inspector's "move everything at once" controls; its
    // net effect is already baked into the per-channel transforms below.

    auto applyTexture = [&](const char* useKey, const char* pathKey, const char* transformKey, bool& useTexture,
                             uint32_t& texIndex, std::string& texPath, VkFormat format, TextureTransform& transform) {
        if (j.contains(useKey)) useTexture = j.at(useKey).get<bool>();
        if (j.contains(pathKey)) {
            std::string path = j.at(pathKey).get<std::string>();
            if (!path.empty()) {
                texIndex = textureManager.addTexture(vengine::resolveAssetPath(path), format);
                texPath = path;
            }
        }
        if (j.contains(transformKey)) readTextureTransform(j.at(transformKey), transform);
    };
    applyTexture("useAlbedoTexture", "albedoTexturePath", "albedoTransform", mat.useAlbedoTexture, mat.albedoTexIndex,
                 mat.albedoTexturePath, VK_FORMAT_R8G8B8A8_SRGB, mat.albedoTransform);
    applyTexture("useMRTexture", "mrTexturePath", "mrTransform", mat.useMRTexture, mat.mrTexIndex,
                 mat.mrTexturePath, VK_FORMAT_R8G8B8A8_UNORM, mat.mrTransform);
    applyTexture("useNormalTexture", "normalTexturePath", "normalTransform", mat.useNormalTexture, mat.normalTexIndex,
                 mat.normalTexturePath, VK_FORMAT_R8G8B8A8_UNORM, mat.normalTransform);
}

json writeMaterialOverride(const Material& mat) {
    return json{
        {"name", mat.name},
        {"baseColour", writeVec3(mat.baseColour)},
        {"useAlbedoTexture", mat.useAlbedoTexture},
        {"albedoTexturePath", mat.albedoTexturePath},
        {"albedoTransform", writeTextureTransform(mat.albedoTransform)},
        {"metallic", mat.metallic},
        {"roughness", mat.roughness},
        {"useMRTexture", mat.useMRTexture},
        {"mrTexturePath", mat.mrTexturePath},
        {"mrTransform", writeTextureTransform(mat.mrTransform)},
        {"useNormalTexture", mat.useNormalTexture},
        {"normalTexturePath", mat.normalTexturePath},
        {"normalTransform", writeTextureTransform(mat.normalTransform)},
    };
}

} // namespace

bool loadScene(const std::string& path, Scene& scene,
               Device& device, TextureManager& textureManager, MaterialManager& materialManager) {
    const std::string resolved = vengine::resolveAssetPath(path);
    std::ifstream file(resolved);
    if (!file.is_open()) {
        std::cerr << "[SceneSerializer] cannot open scene: " << path
                  << " (resolved to " << resolved << ")\n";
        return false;
    }

    json root;
    try {
        file >> root;
    } catch (const std::exception& e) {
        std::cerr << "[SceneSerializer] failed to parse " << path << ": " << e.what() << '\n';
        return false;
    }

    auto& registry = scene.getRegistry();

    if (root.contains("camera")) {
        const json& camJson = root.at("camera");
        entt::entity camera = scene.createEntity("Camera");
        auto& cameraComp = registry.emplace<CameraComponent>(camera);

        if (camJson.value("model", std::string{"Pinhole"}) == "Physical")
            cameraComp.cameraModel = CameraModel::Physical;

        readCameraParams(camJson.value("params", json::object()), cameraComp.cameraParams);
        if (camJson.contains("transform"))
            readTransform(camJson.at("transform"), registry.get<TransformComponent>(camera));
    }

    for (const json& entJson : root.value("entities", json::array())) {
        const std::string name = entJson.value("name", std::string{"Entity"});

        if (entJson.contains("model")) {
            const std::string modelPath = entJson.at("model").get<std::string>();
            std::shared_ptr<Model> model = Model::createModelFromFile(device, textureManager, materialManager, modelPath);
            if (!model) {
                std::cerr << "[SceneSerializer] skipping entity '" << name
                          << "': model not found: " << modelPath << '\n';
                continue;
            }
            entt::entity entity = scene.createModelEntity(model, modelPath);
            registry.get<TagComponent>(entity).name = name;
            registry.get<ModelComponent>(entity).visible = entJson.value("visible", true);
            if (entJson.contains("transform"))
                readTransform(entJson.at("transform"), registry.get<TransformComponent>(entity));

            if (entJson.contains("materials")) {
                const json& matsJson = entJson.at("materials");
                const auto& modelMaterialIndices = model->getMaterialIndices();
                for (size_t i = 0; i < matsJson.size(); i++) {
                    const json& matJson = matsJson[i];
                    std::string wantName = matJson.value("name", std::string{});

                    // Matched by name first (robust to a re-exported asset
                    // reordering its materials), falling back to position.
                    uint32_t globalIdx = 0;
                    bool found = false;
                    if (!wantName.empty()) {
                        for (uint32_t idx : modelMaterialIndices) {
                            if (materialManager.getMaterial(idx).name == wantName) {
                                globalIdx = idx;
                                found = true;
                                break;
                            }
                        }
                    }
                    if (!found) {
                        if (i >= modelMaterialIndices.size()) continue;
                        globalIdx = modelMaterialIndices[i];
                    }
                    readMaterialOverride(matJson, materialManager.getMaterial(globalIdx), textureManager);
                }
            }
        } else if (entJson.contains("pointLight")) {
            const json& lightJson = entJson.at("pointLight");
            glm::vec4 colour = readVec4(lightJson, "colour", glm::vec4(1.f));
            float radius = lightJson.value("radius", 0.1f);
            float range  = lightJson.value("range", 20.f);
            entt::entity entity = scene.createPointLight(colour, radius, range);
            registry.get<TagComponent>(entity).name = name;
            registry.get<PointLightComponent>(entity).visible = lightJson.value("visible", true);
            if (entJson.contains("transform"))
                readTransform(entJson.at("transform"), registry.get<TransformComponent>(entity));
        } else {
            entt::entity entity = scene.createEntity(name);
            if (entJson.contains("transform"))
                readTransform(entJson.at("transform"), registry.get<TransformComponent>(entity));
        }
    }

    return true;
}

bool saveScene(const std::string& path, const Scene& scene, MaterialManager& materialManager) {
    const entt::registry& registry = scene.getRegistry();

    json root;
    root["version"] = 1;
    json entities = json::array();

    for (auto entity : registry.view<const TransformComponent>()) {
        const auto& transform = registry.get<const TransformComponent>(entity);
        std::string name = "Entity";
        if (const auto* tag = registry.try_get<const TagComponent>(entity)) name = tag->name;

        if (const auto* cam = registry.try_get<const CameraComponent>(entity)) {
            root["camera"] = json{
                {"model", cam->cameraModel == CameraModel::Physical ? "Physical" : "Pinhole"},
                {"transform", writeTransform(transform)},
                {"params", writeCameraParams(cam->cameraParams)},
            };
            continue;
        }

        json entJson{{"name", name}, {"transform", writeTransform(transform)}};

        if (const auto* light = registry.try_get<const PointLightComponent>(entity)) {
            entJson["pointLight"] = json{
                {"colour", writeVec4(light->colour)},
                {"radius", light->radius},
                {"range", light->range},
                {"visible", light->visible},
            };
        } else if (const auto* model = registry.try_get<const ModelComponent>(entity)) {
            if (model->sourcePath.empty()) continue;   // procedurally-built model, nothing to reference
            entJson["model"] = model->sourcePath;
            entJson["visible"] = model->visible;

            json materialsJson = json::array();
            for (uint32_t idx : model->model->getMaterialIndices())
                materialsJson.push_back(writeMaterialOverride(materialManager.getMaterial(idx)));
            entJson["materials"] = materialsJson;
        }

        entities.push_back(entJson);
    }

    root["entities"] = entities;

    const std::string resolved = vengine::resolveAssetPath(path);
    std::ofstream file(resolved);
    if (!file.is_open()) {
        std::cerr << "[SceneSerializer] cannot write scene: " << resolved << '\n';
        return false;
    }
    file << root.dump(2) << '\n';
    return true;
}

} // namespace vengine
