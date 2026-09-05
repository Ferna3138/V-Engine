#include "Rendering/Resources/Model.hpp"

#include "Foundation/Core/Utils.hpp"
#include "Foundation/Platform/AssetPath.hpp"

// libs
#define TINYGLTF3_IMPLEMENTATION
#include <tiny_gltf_v3.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#include <cassert>
#include <cmath>
#include <algorithm>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_map>

#include <glm/gtx/string_cast.hpp>
#include <iostream>




Model::Model(Device& _device, const Model::Builder &builder) : device{_device}, materialIndices{builder.materialIndices} {
    createVertexBuffers(builder.vertices);
    createIndexBuffers(builder.indices);
}

Model::~Model() { }

std::unique_ptr<Model> Model::createModelFromFile(Device& device, TextureManager& textureManager,
                                                   MaterialManager& materialManager, const std::string &filepath) {
    // Resolve against the project root so the model loads regardless of the
    // working directory the terminal / debugger starts us in.
    std::string resolvedPath = vengine::resolveAssetPath(filepath);
    if (!fs::exists(resolvedPath)) {
        // Missing model is not fatal: report it and let the caller carry on
        // without this model (returns nullptr).
        std::cerr << "No model found at location " << resolvedPath << std::endl;
        return nullptr;
    }

    Builder builder{};
    builder.loadModel(resolvedPath, textureManager, materialManager);
    return std::make_unique<Model>(device, builder);
}

void Model::createVertexBuffers(const std::vector<Vertex> &vertices) {
    vertexCount = static_cast<uint32_t>(vertices.size());
    assert(vertexCount >= 3 && "Vertex count must be at least 3");
    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertexCount;

    uint32_t vertexSize = sizeof(vertices[0]);
    Buffer stagingBuffer {
        device,
        vertexSize,
        vertexCount,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    };

    stagingBuffer.map();
    stagingBuffer.writeToBuffer((void*)vertices.data());

    vertexBuffer = std::make_unique<Buffer>(
        device,
        vertexSize,
        vertexCount,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,  
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    device.copyBuffer(stagingBuffer.getBuffer(), vertexBuffer-> getBuffer(), bufferSize);
}


void Model::createIndexBuffers(const std::vector<uint32_t> &indices) {
    indexCount = static_cast<uint32_t>(indices.size());
    hasIndexBuffer = indexCount > 0;

    if (!hasIndexBuffer) {
        return;
    }

    VkDeviceSize bufferSize = sizeof(indices[0]) * indexCount;
    uint32_t indexSize = sizeof(indices[0]);

    Buffer stagingBuffer {
        device,
        indexSize,
        indexCount,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    };
    stagingBuffer.map();
    stagingBuffer.writeToBuffer((void*)indices.data());


    indexBuffer = std::make_unique<Buffer>(
        device,
        indexSize,
        indexCount,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);


    device.copyBuffer(stagingBuffer.getBuffer(), indexBuffer->getBuffer(), bufferSize);
}



void Model::draw(VkCommandBuffer commandBuffer) {
    if (hasIndexBuffer) {
        vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
    } else {
        vkCmdDraw(commandBuffer, vertexCount, 1, 0, 0);
    }
}

void Model::bind(VkCommandBuffer commandBuffer){
    VkBuffer buffers[] = {vertexBuffer->getBuffer()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);

    if(hasIndexBuffer) {
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
    }
}

std::vector<VkVertexInputBindingDescription> Model::Vertex::getBindingDescriptions() {
    std::vector<VkVertexInputBindingDescription> bindingDescriptions(1);
    bindingDescriptions[0].binding = 0;
    bindingDescriptions[0].stride = sizeof(Vertex);
    bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return bindingDescriptions;
}

std::vector<VkVertexInputAttributeDescription> Model::Vertex::getAttributeDescriptions() {
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};

    attributeDescriptions.push_back({0,0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position)});
    attributeDescriptions.push_back({1,0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, colour)});
    attributeDescriptions.push_back({2,0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)});
    attributeDescriptions.push_back({3,0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv)});
    attributeDescriptions.push_back({4,0, VK_FORMAT_R32_SINT, offsetof(Vertex, materialIndex)});
    attributeDescriptions.push_back({7,0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, tangent)});


    return attributeDescriptions;
}

void Model::Builder::loadModel(const std::string& filepath, TextureManager& textureManager, MaterialManager& materialManager) {
    fs::path ext = fs::path(filepath).extension();
    if (ext == ".obj")
        loadObj(filepath, textureManager, materialManager);
    else if (ext == ".gltf" || ext == ".glb")
        loadGltf(filepath, textureManager, materialManager);
    else
        throw std::runtime_error("Unsupported model format: " + ext.string());
}

void Model::Builder::loadObj(const std::string& filename, TextureManager& textureManager, MaterialManager& materialManager) {
    std::vector<int32_t> materialsIndices;

    tinyobj::ObjReader reader;
    reader.ParseFromFile(filename);
    if(!reader.Valid()) {
        throw std::runtime_error("Failed to load model: " + filename);
        assert(reader.Valid());
    }

    fs::path objPath = fs::absolute(filename);
    fs::path objDir  = objPath.parent_path();
    fs::path texDir  = objDir / "textures";

    // Resolves a named OBJ texture to a bindless slot + the path used, or
    // {false, defaultIndex, ""} if absent/missing on disk.
    auto resolveTexture = [&](const std::string& texName, uint32_t defaultIndex, VkFormat format)
            -> std::tuple<bool, uint32_t, std::string> {
        if (texName.empty()) return {false, defaultIndex, ""};
        fs::path texFile = texDir / fs::path(texName).filename();
        if (fs::exists(texFile)) {
            std::string path = texFile.string();
            return {true, textureManager.addTexture(path, format), path};
        }
        return {false, defaultIndex, ""};
    };

    // reader.GetMaterials() is already deduped by tinyobjloader (one entry per
    // unique material in the file), so a straight 1:1 registration is correct.
    std::vector<uint32_t> localToGlobal;
    for(const auto& material : reader.GetMaterials()) {
        Material m;
        m.name = material.name;
        m.baseColour = glm::vec3(material.diffuse[0], material.diffuse[1], material.diffuse[2]);

        auto [hasAlbedo, albedoIdx, albedoPath] = resolveTexture(material.diffuse_texname, 1, VK_FORMAT_R8G8B8A8_SRGB);
        m.useAlbedoTexture = hasAlbedo;
        m.albedoTexIndex = albedoIdx;
        m.albedoTexturePath = albedoPath;

        // OBJ is Phong: no metalness, and specular is a colour/intensity map with
        // no metallic-roughness equivalent, so there's no OBJ concept of an MR
        // texture - only the Blinn-Phong shininess (Ns), converted to a scalar
        // roughness (standard exponent -> roughness fit) with metal fixed at 0.
        m.useMRTexture = false;
        m.metallic = 0.f;
        m.roughness = std::clamp(std::sqrt(2.0f / (std::max(material.shininess, 0.0f) + 2.0f)), 0.045f, 1.0f);

        auto [hasNormal, normalIdx, normalPath] = resolveTexture(material.bump_texname, 2, VK_FORMAT_R8G8B8A8_UNORM);
        m.useNormalTexture = hasNormal;
        m.normalTexIndex = normalIdx;
        m.normalTexturePath = normalPath;

        uint32_t globalIdx = materialManager.addMaterial(std::move(m));
        localToGlobal.push_back(globalIdx);
        materialIndices.push_back(globalIdx);
    }

    // Default material used when a face has no material assigned (material_id
    // == -1) or when the obj has no materials at all. Created lazily so files
    // that always assign a material don't register an unused one.
    std::optional<uint32_t> defaultGlobalIdx;
    auto getDefaultMaterial = [&]() -> uint32_t {
        if (!defaultGlobalIdx) {
            Material m;
            m.name = "Default Material";
            defaultGlobalIdx = materialManager.addMaterial(std::move(m));
            materialIndices.push_back(*defaultGlobalIdx);
        }
        return *defaultGlobalIdx;
    };

    const tinyobj::attrib_t& attrib = reader.GetAttrib();

    for(const auto& shape : reader.GetShapes()) {
        vertices.reserve(shape.mesh.indices.size() + vertices.size());
        indices.reserve(shape.mesh.indices.size() + indices.size());
        materialsIndices.insert(materialsIndices.end(), shape.mesh.material_ids.begin(), shape.mesh.material_ids.end());

        for(const auto& index : shape.mesh.indices) {
        Vertex    vertex = {};
        const float* vp = &attrib.vertices[3 * index.vertex_index];
        vertex.position = {*(vp + 0), *(vp + 1), *(vp + 2)};

        if(!attrib.normals.empty() && index.normal_index >= 0) {
            const float* np = &attrib.normals[3 * index.normal_index];
            vertex.normal = {*(np + 0), *(np + 1), *(np + 2)};
        }

        if(!attrib.texcoords.empty() && index.texcoord_index >= 0) {
            const float* tp = &attrib.texcoords[2 * index.texcoord_index + 0];
            vertex.uv = {*tp, 1.0f - *(tp + 1)};
        }

        if(!attrib.colors.empty()) {
            const float* vc = &attrib.colors[3 * index.vertex_index];
            vertex.colour = {*(vc + 0), *(vc + 1), *(vc + 2)};
        }

        vertices.push_back(vertex);
        indices.push_back(static_cast<int>(indices.size()));
        }
    }

    for (size_t t = 0; t < materialsIndices.size(); t++) {
        int32_t matId = materialsIndices[t];
        uint32_t globalIdx = (matId >= 0 && static_cast<size_t>(matId) < localToGlobal.size())
            ? localToGlobal[matId]
            : getDefaultMaterial();
        for (int k = 0; k < 3; k++) {
            vertices[3*t + k].materialIndex = static_cast<int>(globalIdx);
        }
    }

    // Compute normals when none were provided.
    // Compute normals when none were provided.
    if (attrib.normals.empty()) {
        for (size_t t = 0; t < indices.size() / 3; t++) {
            Vertex& v0 = vertices[indices[3*t + 0]];
            Vertex& v1 = vertices[indices[3*t + 1]];
            Vertex& v2 = vertices[indices[3*t + 2]];

            glm::vec3 n = glm::normalize(glm::cross((v1.position - v0.position),
                                                    (v2.position - v0.position)));
            v0.normal = n;
            v1.normal = n;
            v2.normal = n;
        }
    }

    // Compute tangents (OBJ never provides these, regardless of whether normals exist).
    for (size_t t = 0; t < indices.size() / 3; t++) {
        Vertex& v0 = vertices[3*t + 0];
        Vertex& v1 = vertices[3*t + 1];
        Vertex& v2 = vertices[3*t + 2];

        glm::vec3 edge1 = v1.position - v0.position;
        glm::vec3 edge2 = v2.position - v0.position;
        glm::vec2 deltaUV1 = v1.uv - v0.uv;
        glm::vec2 deltaUV2 = v2.uv - v0.uv;

        float denom = deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y;
        glm::vec3 tangent(1.f, 0.f, 0.f);
        if (std::abs(denom) > 1e-8f) {
            float f = 1.0f / denom;
            tangent = f * (deltaUV2.y * edge1 - deltaUV1.y * edge2);
            tangent = glm::normalize(tangent);
        }

        v0.tangent = tangent;
        v1.tangent = tangent;
        v2.tangent = tangent;
    }
}


std::vector<glm::vec2> readVec2FloatAccessor(const tg3_model* model, int32_t accessorIndex) {
    std::vector<glm::vec2> out;
    if (accessorIndex < 0) return out;

    const tg3_accessor& acc = model->accessors[accessorIndex];
    assert(acc.component_type == TG3_COMPONENT_TYPE_FLOAT && acc.type == TG3_TYPE_VEC2);

    const tg3_buffer_view& bv = model->buffer_views[acc.buffer_view];
    const tg3_buffer& buf = model->buffers[bv.buffer];
    int32_t stride = tg3_accessor_byte_stride(&acc, &bv);

    const uint8_t* base = buf.data.data + bv.byte_offset + acc.byte_offset;
    out.resize(acc.count);
    for (uint64_t i = 0; i < acc.count; i++) {
        const float* f = reinterpret_cast<const float*>(base + i * stride);
        out[i] = glm::vec2(f[0], f[1]);
    }
    return out;
}

std::vector<glm::vec3> readVec3FloatAccessor(const tg3_model* model, int32_t accessorIndex) {
    std::vector<glm::vec3> out;
    if (accessorIndex < 0) return out;

    const tg3_accessor& acc = model->accessors[accessorIndex];
    assert(acc.component_type == TG3_COMPONENT_TYPE_FLOAT && acc.type == TG3_TYPE_VEC3);

    const tg3_buffer_view& bv = model->buffer_views[acc.buffer_view];
    const tg3_buffer& buf = model->buffers[bv.buffer];
    int32_t stride = tg3_accessor_byte_stride(&acc, &bv);   // resolves byte_stride==0 to the tightly-packed size for you

    const uint8_t* base = buf.data.data + bv.byte_offset + acc.byte_offset;
    out.resize(acc.count);
    for (uint64_t i = 0; i < acc.count; i++) {
        const float* f = reinterpret_cast<const float*>(base + i * stride);
        out[i] = glm::vec3(f[0], f[1], f[2]);
    }
    return out;
}

std::vector<uint32_t> readIndexAccessor(const tg3_model* model, int32_t accessorIndex) {
    const tg3_accessor& acc = model->accessors[accessorIndex];
    const tg3_buffer_view& bv = model->buffer_views[acc.buffer_view];
    const tg3_buffer& buf = model->buffers[bv.buffer];
    int32_t stride = tg3_accessor_byte_stride(&acc, &bv);
    const uint8_t* base = buf.data.data + bv.byte_offset + acc.byte_offset;

    std::vector<uint32_t> out(acc.count);
    for (uint64_t i = 0; i < acc.count; i++) {
        const uint8_t* p = base + i * stride;
        switch (acc.component_type) {
            case TG3_COMPONENT_TYPE_UNSIGNED_BYTE:  out[i] = *p; break;
            case TG3_COMPONENT_TYPE_UNSIGNED_SHORT: out[i] = *reinterpret_cast<const uint16_t*>(p); break;
            case TG3_COMPONENT_TYPE_UNSIGNED_INT:   out[i] = *reinterpret_cast<const uint32_t*>(p); break;
            default: throw std::runtime_error("Unsupported index component type");
        }
    }
    return out;
}


int32_t findAttribute(const tg3_primitive& prim, const char* name) {
    for (uint32_t i = 0; i < prim.attributes_count; i++) {
        if (tg3_str_equals_cstr(prim.attributes[i].key, name))
            return prim.attributes[i].value;
    }
    return -1;   // attribute absent on this primitive — NORMAL and TEXCOORD_0 are optional per spec
}

glm::mat4 nodeLocalTransform(const tg3_node& node) {
    if (node.has_matrix) {
        glm::dmat4 m;
        std::memcpy(&m, node.matrix, sizeof(m));   // glTF matrices are already column-major doubles
        return glm::mat4(m);
    }
    glm::vec3 t(node.translation[0], node.translation[1], node.translation[2]);
    glm::vec3 s(node.scale[0], node.scale[1], node.scale[2]);
    glm::quat r(
        static_cast<float>(node.rotation[3]),  // w
        static_cast<float>(node.rotation[0]),  // x
        static_cast<float>(node.rotation[1]),  // y
        static_cast<float>(node.rotation[2])); // z
    return glm::translate(glm::mat4(1.f), t) * glm::mat4_cast(r) * glm::scale(glm::mat4(1.f), s);
}

void collectMeshNodes(const tg3_model* model, int32_t nodeIndex, const glm::mat4& parentTransform,
                       std::vector<std::pair<int32_t, glm::mat4>>& outMeshNodes) {
    const tg3_node& node = model->nodes[nodeIndex];
    glm::mat4 world = parentTransform * nodeLocalTransform(node);

    if (node.mesh >= 0)
        outMeshNodes.emplace_back(node.mesh, world);

    for (uint32_t i = 0; i < node.children_count; i++)
        collectMeshNodes(model, node.children[i], world, outMeshNodes);
}


void Model::Builder::loadGltf(const std::string& filepath, TextureManager& textureManager, MaterialManager& materialManager) {
    tinygltf3::Model model;
    tinygltf3::ErrorStack errors;

    fs::path gltfDir = fs::path(filepath).parent_path();

    // Resolves a glTF texture reference to a bindless slot + the path used, or
    // {false, defaultIndex, ""} if absent/embedded/missing on disk.
    auto resolveGltfTexture = [&](int32_t textureIndex, uint32_t defaultIndex, VkFormat format)
            -> std::tuple<bool, uint32_t, std::string> {
        if (textureIndex < 0) return {false, defaultIndex, ""};
        const tg3_texture& tex = model->textures[textureIndex];
        if (tex.source < 0) return {false, defaultIndex, ""};
        const tg3_image& img = model->images[tex.source];
        if (img.uri.len == 0) return {false, defaultIndex, ""};   // embedded image — not handled yet, falls back safely
        std::string uri(img.uri.data, img.uri.len);
        fs::path imgPath = gltfDir / uri;
        if (!fs::exists(imgPath)) return {false, defaultIndex, ""};
        std::string path = imgPath.string();
        return {true, textureManager.addTexture(path, format), path};
    };


    if (tinygltf3::parse_file(model, errors, filepath.c_str()) != TG3_OK) {
        throw std::runtime_error("Failed to load glTF: " + filepath);
    }

    // Dedup by glTF material index (-1 = "no material assigned") across the
    // WHOLE file: the same material can appear on primitives in different
    // meshes/nodes, and must map to exactly one MaterialManager entry, not one
    // per primitive that references it.
    std::unordered_map<int32_t, uint32_t> gltfMatToGlobal;
    auto resolveMaterial = [&](int32_t gltfMatIndex) -> uint32_t {
        if (auto it = gltfMatToGlobal.find(gltfMatIndex); it != gltfMatToGlobal.end())
            return it->second;

        Material m;
        if (gltfMatIndex >= 0) {
            const tg3_material& mat = model->materials[gltfMatIndex];
            const tg3_pbr_metallic_roughness& pmr = mat.pbr_metallic_roughness;
            m.name = std::string(mat.name.data, mat.name.len);

            m.baseColour = glm::vec3(pmr.base_color_factor[0], pmr.base_color_factor[1], pmr.base_color_factor[2]);
            m.metallic = static_cast<float>(pmr.metallic_factor);
            m.roughness = static_cast<float>(pmr.roughness_factor);

            auto [hasAlbedo, albedoIdx, albedoPath] = resolveGltfTexture(pmr.base_color_texture.index, 1, VK_FORMAT_R8G8B8A8_SRGB);
            m.useAlbedoTexture = hasAlbedo;
            m.albedoTexIndex = albedoIdx;
            m.albedoTexturePath = albedoPath;

            auto [hasNormal, normalIdx, normalPath] = resolveGltfTexture(mat.normal_texture.index, 2, VK_FORMAT_R8G8B8A8_UNORM);
            m.useNormalTexture = hasNormal;
            m.normalTexIndex = normalIdx;
            m.normalTexturePath = normalPath;

            // glTF metallic-roughness: G = roughness, B = metalness.
            auto [hasMR, mrIdx, mrPath] = resolveGltfTexture(pmr.metallic_roughness_texture.index, 0, VK_FORMAT_R8G8B8A8_UNORM);
            m.useMRTexture = hasMR;
            m.mrTexIndex = mrIdx;
            m.mrTexturePath = mrPath;
        } else {
            m.name = "Default Material";
        }

        uint32_t globalIdx = materialManager.addMaterial(std::move(m));
        gltfMatToGlobal.emplace(gltfMatIndex, globalIdx);
        materialIndices.push_back(globalIdx);
        return globalIdx;
    };

    std::vector<std::pair<int32_t, glm::mat4>> meshNodes;
    int32_t sceneIdx = model->default_scene >= 0 ? model->default_scene : 0;
    const tg3_scene& scene = model->scenes[sceneIdx];
    for (uint32_t i = 0; i < scene.nodes_count; i++)
        collectMeshNodes(model.get(), scene.nodes[i], glm::mat4(1.0f), meshNodes);

    for (auto& [meshIndex, worldTransform] : meshNodes) {
        glm::mat3 normalMatrix = glm::mat3(glm::transpose(glm::inverse(worldTransform)));
        const tg3_mesh& mesh = model->meshes[meshIndex];

        for (uint32_t p = 0; p < mesh.primitives_count; p++) {
            const tg3_primitive& prim = mesh.primitives[p];

            uint32_t materialIndex = resolveMaterial(prim.material);

            auto positions = readVec3FloatAccessor(model.get(), findAttribute(prim, "POSITION"));
            auto normals   = readVec3FloatAccessor(model.get(), findAttribute(prim, "NORMAL"));   // may come back empty — attribute is optional
            auto texcoords = readVec2FloatAccessor(model.get(), findAttribute(prim, "TEXCOORD_0")); // ditto — you write this one, same shape as Vec3 reader

            uint32_t vertexBase = static_cast<uint32_t>(vertices.size());   // remember this BEFORE appending — every index below is offset by it

            for (size_t i = 0; i < positions.size(); i++) {
                Vertex v{};
                v.position = glm::vec3(worldTransform * glm::vec4(positions[i], 1.0f));
                v.normal   = normals.empty() ? glm::vec3(0.f) : glm::normalize(normalMatrix * normals[i]);
                v.uv       = texcoords.empty() ? glm::vec2(0.f) : texcoords[i];
                v.materialIndex = static_cast<int>(materialIndex);

                vertices.push_back(v);
            }

                        auto primIndices = readIndexAccessor(model.get(), prim.indices);
            for (uint32_t idx : primIndices)
                indices.push_back(vertexBase + idx);   // offset so multiple primitives/meshes share one flat buffer correctly

            // <-- new tangent-computation block goes here, still inside the `for (uint32_t p ...)` loop
            size_t indexBase = indices.size() - primIndices.size();
            for (size_t t = 0; t < primIndices.size() / 3; t++) {
                uint32_t i0 = indices[indexBase + 3*t + 0];
                uint32_t i1 = indices[indexBase + 3*t + 1];
                uint32_t i2 = indices[indexBase + 3*t + 2];
                Vertex& v0 = vertices[i0];
                Vertex& v1 = vertices[i1];
                Vertex& v2 = vertices[i2];

                glm::vec3 edge1 = v1.position - v0.position;
                glm::vec3 edge2 = v2.position - v0.position;
                glm::vec2 deltaUV1 = v1.uv - v0.uv;
                glm::vec2 deltaUV2 = v2.uv - v0.uv;

                float denom = deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y;
                glm::vec3 tangent(1.f, 0.f, 0.f);
                if (std::abs(denom) > 1e-8f) {
                    float f = 1.0f / denom;
                    tangent = f * (deltaUV2.y * edge1 - deltaUV1.y * edge2);
                    tangent = glm::normalize(tangent);
                }

                v0.tangent = tangent;
                v1.tangent = tangent;
                v2.tangent = tangent;
            }
        }
    }
}