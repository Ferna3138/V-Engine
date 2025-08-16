#include "Model.hpp"

#include "Utils/Utils.hpp"

// libs
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#include <cassert>
#include <unordered_map>


namespace std{
    template <>
    struct hash<Model::Vertex> {
        size_t operator()(const Model::Vertex &vertex) const {
            size_t seed = 0;
            hashCombine(seed, vertex.position, vertex.colour, vertex.normal, vertex.uv);
            return seed;
        }
    };
}


Model::Model(Device& _device, const Model::Builder &builder) : device{_device} {
    createVertexBuffers(builder.vertices);
    createIndexBuffers(builder.indices);

    materialsIndices = builder.materialsIndices; 
    materials = builder.materials; 
    textures = builder.textures; // texture file paths

}

Model::~Model() { }

std::unique_ptr<Model> Model::createModelFromFile(Device& device, const std::string &filepath) {
    Builder builder{};
    builder.loadModel(filepath);
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

    return attributeDescriptions;
}



void Model::Builder::loadModel(const std::string& filename) {
    tinyobj::ObjReader reader;
    reader.ParseFromFile(filename);
    if(!reader.Valid()) {
        throw std::runtime_error("Failed to load model: " + filename);
        assert(reader.Valid());
    }

    fs::path objPath = fs::absolute(filename);
    fs::path objDir  = objPath.parent_path();
    fs::path texDir  = objDir / "textures";

    // Collecting the material in the scene
    for(const auto& material : reader.GetMaterials()) {
        MaterialObj m;
        m.ambient       = glm::vec3(material.ambient[0], material.ambient[1], material.ambient[2]);
        m.diffuse       = glm::vec3(material.diffuse[0], material.diffuse[1], material.diffuse[2]);
        m.specular      = glm::vec3(material.specular[0], material.specular[1], material.specular[2]);
        m.emission      = glm::vec3(material.emission[0], material.emission[1], material.emission[2]);
        m.transmittance = glm::vec3(material.transmittance[0], material.transmittance[1], material.transmittance[2]);
        m.dissolve      = material.dissolve;
        m.ior           = material.ior;
        m.shininess     = material.shininess;
        m.illum         = material.illum;

        auto resolveTexture = [&](const std::string& texName) -> int {
            if (texName.empty()) return -1;
            fs::path texFile = texDir / fs::path(texName).filename();
            if (fs::exists(texFile)) {
                textures.push_back(texFile.string());
                return static_cast<int>(textures.size()) - 1;
            } else {
                // fallback: use raw path (might be absolute or relative)
                textures.push_back(texName);
                return static_cast<int>(textures.size()) - 1;
            }
        };

        // Load diffuse texture
        m.diffuseTexID = resolveTexture(material.diffuse_texname);

        // Load specular map
        m.specularTexID = resolveTexture(material.specular_texname);

        // Load normal map (bump_texname is used for normal in OBJ/MTL)
        m.normalTexID = resolveTexture(material.bump_texname);

        materials.emplace_back(m);
    }

    // If there were none, add a default
    if(materials.empty())
        materials.emplace_back(MaterialObj());

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

    // Fixing material indices
    for (auto& mi : materialsIndices) {
        if (mi < 0 || mi >= static_cast<int>(materials.size())) {
            mi = 0;
        }
    }

    // Compute normals when none were provided.
    if (attrib.normals.empty()) {
        for (size_t i = 0; i < indices.size(); i += 3) {
            Vertex& v0 = vertices[indices[i + 0]];
            Vertex& v1 = vertices[indices[i + 1]];
            Vertex& v2 = vertices[indices[i + 2]];

            glm::vec3 n = glm::normalize(glm::cross((v1.position - v0.position),
                                                    (v2.position - v0.position)));
            v0.normal = n;
            v1.normal = n;
            v2.normal = n;
        }
    }
}

