#pragma once

#include "Renderer/Device.hpp"
#include "Renderer/Buffer.hpp"

// Libs
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/glm.hpp"

// Std
#include <cstring>
#include <memory>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

class Model{
    public:
        struct Vertex{
            glm::vec3 position;
            glm::vec3 colour;
            glm::vec3 normal;
            glm::vec2 uv;

            static std::vector<VkVertexInputBindingDescription> getBindingDescriptions();
            static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();

            bool operator == (const Vertex& other) const {
                return position == other.position && colour == other.colour && normal == other.normal && uv == other.uv;
            }
        };

        struct MaterialObj {
            glm::vec3 ambient       = glm::vec3(0.1f, 0.1f, 0.1f);
            glm::vec3 diffuse       = glm::vec3(0.7f, 0.7f, 0.7f);
            glm::vec3 specular      = glm::vec3(1.0f, 1.0f, 1.0f);
            glm::vec3 transmittance = glm::vec3(0.0f, 0.0f, 0.0f);
            glm::vec3 emission      = glm::vec3(0.0f, 0.0f, 0.10);
            float     shininess     = 0.f;
            float     ior           = 1.0f;  // index of refraction
            float     dissolve      = 1.f;   // 1 == opaque; 0 == fully transparent
                                            // illumination model (see http://www.fileformat.info/format/material/)
            int illum     = 0;
            int diffuseTexID = -1;
            int specularTexID = -1;
            int normalTexID = -1;
        };

        struct Builder{
            std::vector<Vertex> vertices{};
            std::vector<uint32_t> indices{};
            std::vector<MaterialObj> materials;
            std::vector<std::string> textures;
            std::vector<int32_t>     materialsIndices;
            void loadModel(const std::string &filepath);
        };

        Model(Device& _device, const Model::Builder &builder);
        ~Model();

        Model(const Model&) = delete;
        Model &operator = (const Model&) = delete;

        static std::unique_ptr<Model> createModelFromFile(Device& device, const std::string &filepath);

        void bind(VkCommandBuffer commandBuffer);
        void draw(VkCommandBuffer commandBuffer);

    private:
        void createVertexBuffers(const std::vector<Vertex> &vertices);
        void createIndexBuffers(const std::vector<uint32_t> &indices);

        Device& device;

        bool hasIndexBuffer = false;
        
        uint32_t vertexCount;
        uint32_t indexCount;
        
        std::unique_ptr<Buffer> vertexBuffer;
        std::unique_ptr<Buffer> indexBuffer;
        
        // CPU-side only, used by ECS / TextureManager
        std::vector<MaterialObj> materials;
        std::vector<std::string> textures;
        std::vector<int32_t> materialsIndices;
};