#pragma once

#include "Renderer/Device.hpp"

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include "json.hpp"
#include <fstream>
#include <functional>

using json = nlohmann::json;
using FrameGraphRenderPassFn = std::function<void(VkCommandBuffer)>;

enum class FrameGraphResourceType {
    Attachment,
    Texture,
    Buffer,
    Reference
};

struct FrameGraphResourceDesc {
    FrameGraphResourceType type;
    std::string name;
    std::string format;       // raw string, e.g. "VK_FORMAT_B8G8R8A8_UNORM"; empty if unspecified
    uint32_t resolutionWidth = 0;
    uint32_t resolutionHeight = 0;
    std::string loadOp;       // raw string, e.g. "VK_ATTACHMENT_LOAD_OP_CLEAR"; empty if unspecified
};

struct FrameGraphResource {
    FrameGraphResourceType type;
    std::string name;
    std::string format;
    uint32_t resolutionWidth = 0;
    uint32_t resolutionHeight = 0;
    std::string loadOp;
    uint32_t producerNode = UINT32_MAX;
    uint32_t outputHandle = UINT32_MAX;
    int refCount = 0;

    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory imageMemory = VK_NULL_HANDLE;
    VkImageView imageView = VK_NULL_HANDLE;
    VkFormat vkFormat = VK_FORMAT_UNDEFINED;

    // Layout the owning image is currently in, tracked while execute() walks the
    // graph so it can auto-insert the transitions each edge implies.
    VkImageLayout currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
};


struct FrameGraphNode {
    std::string name;
    std::vector<uint32_t> inputs;   // indices into FrameGraph's resource list
    std::vector<uint32_t> outputs;
    std::vector<uint32_t> edges;    // indices of nodes that depend on this one

    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
};

class FrameGraph {
    public:
        uint32_t addNode(const std::string& name,
                          const std::vector<FrameGraphResourceDesc>& inputs,
                          const std::vector<FrameGraphResourceDesc>& outputs);

        void parse(const std::string& jsonFilePath);
        void compile();

        const std::vector<uint32_t>& getExecutionOrder() const { return sortedNodeOrder; }
        const FrameGraphNode& getNode(uint32_t index) const { return nodes[index]; }
        size_t getNodeCount() const { return nodes.size(); }

        uint32_t findNode(const std::string& name) const;      // UINT32_MAX if absent
        VkRenderPass getNodeRenderPass(const std::string& name) const;


        void createRenderPasses(Device& device);   // call after createResources()

        void registerRenderPass(const std::string& nodeName, FrameGraphRenderPassFn fn);
        void execute(VkCommandBuffer commandBuffer);

        // Names the resource that leaves the graph to be presented. execute()
        // transitions it to TRANSFER_SRC_OPTIMAL at the end so the caller can
        // blit it into the swapchain image.
        void setPresentOutput(const std::string& resourceName);
        struct OutputImage { VkImage image = VK_NULL_HANDLE; uint32_t width = 0; uint32_t height = 0; };
        OutputImage getPresentOutput() const;

        void createResources(Device& device);
        ~FrameGraph();

    private:

        struct AttachmentEntry { uint32_t resourceIndex; VkAttachmentLoadOp loadOp; };
        std::vector<AttachmentEntry> gatherAttachmentEntries(const FrameGraphNode& node);

        // Barrier the owning image of `edgeInput` from its tracked currentLayout
        // to whatever `consumerType` (attachment vs sampled) needs, before the
        // consuming node runs.
        void insertInputBarrier(VkCommandBuffer cb, FrameGraphResource& owning, FrameGraphResourceType consumerType);

        void computeEdges();
        void topologicalSort();

        uint32_t presentOutputResource = UINT32_MAX;

        Device* ownerDevice = nullptr;

        std::vector<FrameGraphNode> nodes;
        std::vector<FrameGraphResource> resources;
        std::unordered_map<std::string, uint32_t> outputNameToResource;
        std::vector<uint32_t> sortedNodeOrder;
        
        std::unordered_map<std::string, FrameGraphRenderPassFn> renderPassCallbacks;

};