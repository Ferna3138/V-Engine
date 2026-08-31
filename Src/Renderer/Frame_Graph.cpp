// Frame_Graph.cpp
#include "Frame_Graph.hpp"
#include "Utils/AssetPath.hpp"
#include <stdexcept>

uint32_t FrameGraph::addNode(const std::string& name,
                              const std::vector<FrameGraphResourceDesc>& inputDescs,
                              const std::vector<FrameGraphResourceDesc>& outputDescs) {
    uint32_t nodeIndex = static_cast<uint32_t>(nodes.size());
    nodes.push_back(FrameGraphNode{});
    FrameGraphNode& node = nodes.back();
    node.name = name;

    for (const auto& desc : inputDescs) {
        uint32_t resIndex = static_cast<uint32_t>(resources.size());
        resources.push_back(FrameGraphResource{ desc.type, desc.name });
        node.inputs.push_back(resIndex);
    }

    for (const auto& desc : outputDescs) {
        uint32_t resIndex = static_cast<uint32_t>(resources.size());
        FrameGraphResource res{ desc.type, desc.name };
        res.format = desc.format;
        res.resolutionWidth = desc.resolutionWidth;
        res.resolutionHeight = desc.resolutionHeight;
        res.loadOp = desc.loadOp;
        res.producerNode = nodeIndex;
        resources.push_back(res);
        node.outputs.push_back(resIndex);
    }

    return nodeIndex;
}

void FrameGraph::compile() {
    computeEdges();
    topologicalSort();
}

void FrameGraph::computeEdges() {
    for (uint32_t nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex) {
        FrameGraphNode& node = nodes[nodeIndex];
        for (uint32_t inputResIndex : node.inputs) {
            FrameGraphResource& inputRes = resources[inputResIndex];

            auto it = outputNameToResource.find(inputRes.name);
            if (it == outputNameToResource.end()) {
                throw std::runtime_error("Frame graph input '" + inputRes.name + "' has no producing output");
            }

            uint32_t outputResIndex = it->second;
            FrameGraphResource& outputRes = resources[outputResIndex];

            inputRes.producerNode = outputRes.producerNode;
            inputRes.outputHandle = outputResIndex;

            nodes[outputRes.producerNode].edges.push_back(nodeIndex);
        }

        // Register this node's own outputs only after resolving its inputs, so a node
        // can't accidentally link to its own output (e.g. a Reference reusing an input's name).
        for (uint32_t outputResIndex : node.outputs) {
            outputNameToResource[resources[outputResIndex].name] = outputResIndex;
        }
    }
}

void FrameGraph::topologicalSort() {
    std::vector<uint32_t> sortedReverse;
    std::vector<uint8_t> visited(nodes.size(), 0);  // 0 = unvisited, 1 = visiting, 2 = added
    std::vector<uint32_t> stack;

    for (uint32_t n = 0; n < nodes.size(); ++n) {
        stack.push_back(n);

        while (!stack.empty()) {
            uint32_t nodeHandle = stack.back();

            if (visited[nodeHandle] == 2) {
                stack.pop_back();
                continue;
            }
            if (visited[nodeHandle] == 1) {
                visited[nodeHandle] = 2;
                sortedReverse.push_back(nodeHandle);
                stack.pop_back();
                continue;
            }

            visited[nodeHandle] = 1;

            FrameGraphNode& node = nodes[nodeHandle];
            if (node.edges.empty()) {
                continue;
            }
            for (uint32_t childHandle : node.edges) {
                if (!visited[childHandle]) {
                    stack.push_back(childHandle);
                }
            }
        }
    }

    sortedNodeOrder.assign(sortedReverse.rbegin(), sortedReverse.rend());
}


static FrameGraphResourceType parseResourceType(const std::string& typeStr) {
    static const std::unordered_map<std::string, FrameGraphResourceType> typeMap = {
        {"attachment", FrameGraphResourceType::Attachment},
        {"texture", FrameGraphResourceType::Texture},
        {"buffer", FrameGraphResourceType::Buffer},
        {"reference", FrameGraphResourceType::Reference},
    };
    auto it = typeMap.find(typeStr);
    if (it == typeMap.end()) {
        throw std::runtime_error("Unknown frame graph resource type: " + typeStr);
    }
    return it->second;
}

static FrameGraphResourceDesc parseResourceDesc(const json& resJson) {
    FrameGraphResourceDesc desc{};
    desc.type = parseResourceType(resJson.at("type").get<std::string>());
    desc.name = resJson.at("name").get<std::string>();

    if (resJson.contains("format")) {
        desc.format = resJson["format"].get<std::string>();
    }
    if (resJson.contains("resolution")) {
        desc.resolutionWidth = resJson["resolution"].at(0).get<uint32_t>();
        desc.resolutionHeight = resJson["resolution"].at(1).get<uint32_t>();
    }
    if (resJson.contains("op")) {
        desc.loadOp = resJson["op"].get<std::string>();
    }
    return desc;
}

void FrameGraph::parse(const std::string& jsonFilePath) {
    // Resolve against the project root so it loads regardless of the working
    // directory the terminal / debugger starts us in.
    std::string resolvedPath = vengine::resolveAssetPath(jsonFilePath);
    std::ifstream file(resolvedPath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open frame graph JSON: " + jsonFilePath +
                                 " (resolved to " + resolvedPath + ")");
    }

    json root;
    file >> root;

    for (const auto& nodeJson : root) {
        std::string nodeName = nodeJson.at("name").get<std::string>();

        std::vector<FrameGraphResourceDesc> inputs;
        if (nodeJson.contains("inputs")) {
            for (const auto& inJson : nodeJson["inputs"]) {
                inputs.push_back(parseResourceDesc(inJson));
            }
        }

        std::vector<FrameGraphResourceDesc> outputs;
        if (nodeJson.contains("outputs")) {
            for (const auto& outJson : nodeJson["outputs"]) {
                outputs.push_back(parseResourceDesc(outJson));
            }
        }

        addNode(nodeName, inputs, outputs);
    }
}

static VkFormat parseVkFormat(const std::string& formatStr) {
    static const std::unordered_map<std::string, VkFormat> formatMap = {
        {"VK_FORMAT_D32_SFLOAT", VK_FORMAT_D32_SFLOAT},
        {"VK_FORMAT_B8G8R8A8_UNORM", VK_FORMAT_B8G8R8A8_UNORM},
        {"VK_FORMAT_R16G16B16A16_SFLOAT", VK_FORMAT_R16G16B16A16_SFLOAT},
        {"VK_FORMAT_R8G8B8A8_UNORM", VK_FORMAT_R8G8B8A8_UNORM},
        {"VK_FORMAT_R8G8B8A8_SRGB", VK_FORMAT_R8G8B8A8_SRGB},
    };
    auto it = formatMap.find(formatStr);
    if (it == formatMap.end()) {
        throw std::runtime_error("Unknown frame graph format: " + formatStr);
    }
    return it->second;
}

static bool isDepthFormat(VkFormat format) {
    return format == VK_FORMAT_D32_SFLOAT;
}

void FrameGraph::createResources(Device& device) {
    ownerDevice = &device;

    // Pass 1: create real GPU resources for every entry that actually owns one
    for (FrameGraphResource& resource : resources) {
        if (resource.outputHandle != UINT32_MAX) continue;  // input entry, done in pass 2
        if (resource.type == FrameGraphResourceType::Reference) continue;
        if (resource.type == FrameGraphResourceType::Buffer) continue;  // not handled yet

        resource.vkFormat = parseVkFormat(resource.format);
        bool isDepth = isDepthFormat(resource.vkFormat);

        VkImageUsageFlags usage = isDepth
            ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
            : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        usage |= VK_IMAGE_USAGE_SAMPLED_BIT;  // any attachment here may be read as a texture by a later node

        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = resource.vkFormat;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.extent = { resource.resolutionWidth, resource.resolutionHeight, 1 };
        imageInfo.usage = usage;

        device.createImageWithInfo(imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, resource.image, resource.imageMemory);

        VkImageAspectFlags aspect = isDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = resource.image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = resource.vkFormat;
        viewInfo.subresourceRange.aspectMask = aspect;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device.device(), &viewInfo, nullptr, &resource.imageView) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create frame graph image view: " + resource.name);
        }
    }

    // Pass 2: every input entry just references its producer's already-created resource
    for (FrameGraphResource& resource : resources) {
        if (resource.outputHandle == UINT32_MAX) continue;
        const FrameGraphResource& producerResource = resources[resource.outputHandle];
        resource.image = producerResource.image;
        resource.imageMemory = producerResource.imageMemory;
        resource.imageView = producerResource.imageView;
        resource.vkFormat = producerResource.vkFormat;
        resource.resolutionWidth = producerResource.resolutionWidth;
        resource.resolutionHeight = producerResource.resolutionHeight;
    }
}


FrameGraph::~FrameGraph() {
    if (!ownerDevice) return;
    for (FrameGraphNode& node : nodes) {
        if (node.framebuffer != VK_NULL_HANDLE) vkDestroyFramebuffer(ownerDevice->device(), node.framebuffer, nullptr);
        if (node.renderPass != VK_NULL_HANDLE) vkDestroyRenderPass(ownerDevice->device(), node.renderPass, nullptr);
    }
    for (FrameGraphResource& resource : resources) {
        if (resource.outputHandle != UINT32_MAX) continue;  // only the owning entry frees it
        if (resource.imageView != VK_NULL_HANDLE) vkDestroyImageView(ownerDevice->device(), resource.imageView, nullptr);
        if (resource.image != VK_NULL_HANDLE) vkDestroyImage(ownerDevice->device(), resource.image, nullptr);
        if (resource.imageMemory != VK_NULL_HANDLE) vkFreeMemory(ownerDevice->device(), resource.imageMemory, nullptr);
    }
}




static VkAttachmentLoadOp parseLoadOp(const std::string& opStr) {
    static const std::unordered_map<std::string, VkAttachmentLoadOp> opMap = {
        {"VK_ATTACHMENT_LOAD_OP_CLEAR", VK_ATTACHMENT_LOAD_OP_CLEAR},
        {"VK_ATTACHMENT_LOAD_OP_LOAD", VK_ATTACHMENT_LOAD_OP_LOAD},
        {"VK_ATTACHMENT_LOAD_OP_DONT_CARE", VK_ATTACHMENT_LOAD_OP_DONT_CARE},
    };
    auto it = opMap.find(opStr);
    if (it == opMap.end()) {
        throw std::runtime_error("Unknown frame graph load op: " + opStr);
    }
    return it->second;
}

void FrameGraph::createRenderPasses(Device& device) {
    for (FrameGraphNode& node : nodes) {
        std::vector<AttachmentEntry> attachmentEntries = gatherAttachmentEntries(node);
        
        if (attachmentEntries.empty())
            continue;

        std::vector<VkAttachmentDescription> attachmentDescs;
        std::vector<VkImageView> attachmentViews;
        std::vector<VkAttachmentReference> colorRefs;
        VkAttachmentReference depthRef{};
        bool hasDepth = false;
        uint32_t width = 0, height = 0;

        for (const auto& entry : attachmentEntries) {
            FrameGraphResource& res = resources[entry.resourceIndex];
            bool isDepth = isDepthFormat(res.vkFormat);
            VkImageLayout layout = isDepth ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            VkAttachmentDescription desc{};
            desc.format = res.vkFormat;
            desc.samples = VK_SAMPLE_COUNT_1_BIT;
            desc.loadOp = entry.loadOp;
            desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            desc.initialLayout = (entry.loadOp == VK_ATTACHMENT_LOAD_OP_LOAD) ? layout : VK_IMAGE_LAYOUT_UNDEFINED;
            desc.finalLayout = layout;

            uint32_t attachmentIndex = static_cast<uint32_t>(attachmentDescs.size());
            attachmentDescs.push_back(desc);
            attachmentViews.push_back(res.imageView);
            width = res.resolutionWidth;
            height = res.resolutionHeight;

            if (isDepth) {
                depthRef.attachment = attachmentIndex;
                depthRef.layout = layout;
                hasDepth = true;
            } else {
                VkAttachmentReference ref{};
                ref.attachment = attachmentIndex;
                ref.layout = layout;
                colorRefs.push_back(ref);
            }
        }

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = static_cast<uint32_t>(colorRefs.size());
        subpass.pColorAttachments = colorRefs.empty() ? nullptr : colorRefs.data();
        subpass.pDepthStencilAttachment = hasDepth ? &depthRef : nullptr;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachmentDescs.size());
        renderPassInfo.pAttachments = attachmentDescs.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        if (vkCreateRenderPass(device.device(), &renderPassInfo, nullptr, &node.renderPass) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create frame graph render pass for node: " + node.name);
        }

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = node.renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachmentViews.size());
        framebufferInfo.pAttachments = attachmentViews.data();
        framebufferInfo.width = width;
        framebufferInfo.height = height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(device.device(), &framebufferInfo, nullptr, &node.framebuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create frame graph framebuffer for node: " + node.name);
        }
    }
}

std::vector<FrameGraph::AttachmentEntry> FrameGraph::gatherAttachmentEntries(const FrameGraphNode& node) {
    std::vector<AttachmentEntry> entries;
    for (uint32_t resIndex : node.inputs) {
        if (resources[resIndex].type != FrameGraphResourceType::Attachment) continue;
        entries.push_back({ resIndex, VK_ATTACHMENT_LOAD_OP_LOAD });
    }
    for (uint32_t resIndex : node.outputs) {
        const FrameGraphResource& res = resources[resIndex];
        if (res.type != FrameGraphResourceType::Attachment) continue;  // skips Reference on purpose
        entries.push_back({ resIndex, parseLoadOp(res.loadOp) });
    }
    return entries;
}

void FrameGraph::registerRenderPass(const std::string& nodeName, FrameGraphRenderPassFn fn) {
    renderPassCallbacks[nodeName] = std::move(fn);
}

void FrameGraph::execute(VkCommandBuffer commandBuffer) {
    for (uint32_t nodeIndex : sortedNodeOrder) {
        FrameGraphNode& node = nodes[nodeIndex];
        if (node.renderPass == VK_NULL_HANDLE) continue;

        std::vector<AttachmentEntry> attachmentEntries = gatherAttachmentEntries(node);

        std::vector<VkClearValue> clearValues;
        uint32_t width = 0, height = 0;
        for (const auto& entry : attachmentEntries) {
            const FrameGraphResource& res = resources[entry.resourceIndex];
            VkClearValue clear{};
            if (isDepthFormat(res.vkFormat)) {
                clear.depthStencil = { 1.0f, 0 };
            } else {
                clear.color = { 0.02f, 0.02f, 0.02f, 1.0f };
            }
            clearValues.push_back(clear);
            width = res.resolutionWidth;
            height = res.resolutionHeight;
        }

        VkRenderPassBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        beginInfo.renderPass = node.renderPass;
        beginInfo.framebuffer = node.framebuffer;
        beginInfo.renderArea.offset = {0, 0};
        beginInfo.renderArea.extent = { width, height };
        beginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        beginInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(commandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport{0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f};
        VkRect2D scissor{{0, 0}, {width, height}};
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        auto it = renderPassCallbacks.find(node.name);
        if (it != renderPassCallbacks.end()) {
            it->second(commandBuffer);
        }

        vkCmdEndRenderPass(commandBuffer);
    }
}