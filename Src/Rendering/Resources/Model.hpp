#pragma once

#include "Rendering/RHI/Device.hpp"
#include "Rendering/RHI/Buffer.hpp"
#include "Rendering/Resources/MaterialManager.hpp"
#include "Rendering/Resources/TextureManager.hpp"

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
            int materialIndex{-1};   // global MaterialManager index
            glm::vec3 tangent{0.f};

            static std::vector<VkVertexInputBindingDescription> getBindingDescriptions();
            static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();

        };

        struct Builder{
            std::vector<Vertex> vertices{};
            std::vector<uint32_t> indices{};
            // Global MaterialManager indices this file's materials were registered
            // at, in parse order - lets callers (e.g. the Inspector) enumerate just
            // this model's materials, by name, rather than every material loaded.
            std::vector<uint32_t> materialIndices{};

            void loadObj(const std::string &filepath, TextureManager& textureManager, MaterialManager& materialManager);
            void loadGltf(const std::string &filepath, TextureManager& textureManager, MaterialManager& materialManager);
            void loadModel(const std::string &filepath, TextureManager& textureManager, MaterialManager& materialManager);
        };

        Model(Device& _device, const Model::Builder &builder);
        ~Model();

        Model(const Model&) = delete;
        Model &operator = (const Model&) = delete;

        // Returns nullptr (and logs) if no file exists at filepath; throws on a
        // malformed model file.
        static std::unique_ptr<Model> createModelFromFile(Device& device, TextureManager& textureManager,
                                                            MaterialManager& materialManager, const std::string &filepath);

        void bind(VkCommandBuffer commandBuffer);
        void draw(VkCommandBuffer commandBuffer);

        const std::vector<uint32_t>& getMaterialIndices() const { return materialIndices; }

    private:
        void createVertexBuffers(const std::vector<Vertex> &vertices);
        void createIndexBuffers(const std::vector<uint32_t> &indices);

        Device& device;


        bool hasIndexBuffer = false;

        uint32_t vertexCount;
        uint32_t indexCount;


        std::unique_ptr<Buffer> vertexBuffer;
        std::unique_ptr<Buffer> indexBuffer;

        std::vector<uint32_t> materialIndices;

};