#include "Descriptors.hpp"

// std
#include <cassert>
#include <stdexcept>


// *************** Descriptor Set Layout Builder *********************

DescriptorSetLayout::Builder &DescriptorSetLayout::Builder::addBinding(uint32_t binding,
        VkDescriptorType descriptorType,
        VkShaderStageFlags stageFlags,
        uint32_t count,
        VkDescriptorBindingFlags bindingFlags_) {
            
    assert(bindings.count(binding) == 0 && "Binding already in use");
    VkDescriptorSetLayoutBinding layoutBinding{};
    layoutBinding.binding = binding;
    layoutBinding.descriptorType = descriptorType;
    layoutBinding.descriptorCount = count;
    layoutBinding.stageFlags = stageFlags;
    bindings[binding] = layoutBinding;

    // store flags per binding
    bindingFlags[binding] = bindingFlags_;

  return *this;
}

std::unique_ptr<DescriptorSetLayout> DescriptorSetLayout::Builder::build() const {
    return std::make_unique<DescriptorSetLayout>(device, bindings, bindingFlags);
}

// *************** Descriptor Set Layout *********************

DescriptorSetLayout::DescriptorSetLayout(Device& _device, std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> _bindings, std::unordered_map<uint32_t, VkDescriptorBindingFlags> _bindingFlags) : device{_device}, bindings{_bindings}, bindingFlags{_bindingFlags} {
    //std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings{};
    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings;
    std::vector<VkDescriptorBindingFlags> bindingFlagsVec;
    setLayoutBindings.reserve(bindings.size());
    bindingFlagsVec.reserve(bindings.size());

    // push bindings and flags in the same order
    for (auto kv : bindings) {
        setLayoutBindings.push_back(kv.second);
        // lookup flag for this binding (Builder should have stored it)
        // if not found, default to 0
    }

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo{};
    descriptorSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutInfo.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
    descriptorSetLayoutInfo.pBindings = setLayoutBindings.data();


    // If you filled bindingFlagsVec:
    if (!bindingFlagsVec.empty()) {
        VkDescriptorSetLayoutBindingFlagsCreateInfo flagsCreateInfo{};
        flagsCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
        flagsCreateInfo.bindingCount = static_cast<uint32_t>(bindingFlagsVec.size());
        flagsCreateInfo.pBindingFlags = bindingFlagsVec.data();
        descriptorSetLayoutInfo.pNext = &flagsCreateInfo;

        // When using update-after-bind, you generally also set this flag on layout create info:
        descriptorSetLayoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    } else {
        descriptorSetLayoutInfo.pNext = nullptr;
    }



    if (vkCreateDescriptorSetLayout(
            device.device(),
            &descriptorSetLayoutInfo,
            nullptr,
            &descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout!");
    }
}

DescriptorSetLayout::~DescriptorSetLayout() {
  vkDestroyDescriptorSetLayout(device.device(), descriptorSetLayout, nullptr);
}

// *************** Descriptor Pool Builder *********************

DescriptorPool::Builder &DescriptorPool::Builder::addPoolSize(
    VkDescriptorType descriptorType, uint32_t count) {
  poolSizes.push_back({descriptorType, count});
    return *this;
}

DescriptorPool::Builder &DescriptorPool::Builder::setPoolFlags(
    VkDescriptorPoolCreateFlags flags) {
  poolFlags = flags;
    return *this;
}
DescriptorPool::Builder &DescriptorPool::Builder::setMaxSets(uint32_t count) {
  maxSets = count;
    return *this;
}

std::unique_ptr<DescriptorPool> DescriptorPool::Builder::build() const {
    return std::make_unique<DescriptorPool>(device, maxSets, poolFlags, poolSizes);
}

// *************** Descriptor Pool *********************

DescriptorPool::DescriptorPool(Device& _device, uint32_t maxSets, VkDescriptorPoolCreateFlags poolFlags, const std::vector<VkDescriptorPoolSize> &poolSizes) : device{_device} {
    VkDescriptorPoolCreateInfo descriptorPoolInfo{};
    descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    descriptorPoolInfo.pPoolSizes = poolSizes.data();
    descriptorPoolInfo.maxSets = maxSets;
    descriptorPoolInfo.flags = poolFlags;

    if (vkCreateDescriptorPool(device.device(), &descriptorPoolInfo, nullptr, &descriptorPool) !=
        VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool!");
    }
}

DescriptorPool::~DescriptorPool() {
    vkDestroyDescriptorPool(device.device(), descriptorPool, nullptr);
}

bool DescriptorPool::allocateDescriptor(
    const VkDescriptorSetLayout descriptorSetLayout, VkDescriptorSet &descriptor, uint32_t variableDescriptorCount) const {
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.pSetLayouts = &descriptorSetLayout;
    allocInfo.descriptorSetCount = 1;


    VkDescriptorSetVariableDescriptorCountAllocateInfo varDescCountAlloc{};
    if (variableDescriptorCount > 0) {
        varDescCountAlloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
        varDescCountAlloc.descriptorSetCount = 1;
        varDescCountAlloc.pDescriptorCounts = &variableDescriptorCount;
        allocInfo.pNext = &varDescCountAlloc;
    } else {
        allocInfo.pNext = nullptr;
    }


    // Might want to create a "DescriptorPoolManager" class that handles this case, and builds
    // a new pool whenever an old pool fills up. But this is beyond our current scope
    if (vkAllocateDescriptorSets(device.device(), &allocInfo, &descriptor) != VK_SUCCESS) {
        return false;
    }
    return true;
}

void DescriptorPool::freeDescriptors(std::vector<VkDescriptorSet> &descriptors) const {
  vkFreeDescriptorSets(
      device.device(),
      descriptorPool,
      static_cast<uint32_t>(descriptors.size()),
      descriptors.data());
}

void DescriptorPool::resetPool() {
    vkResetDescriptorPool(device.device(), descriptorPool, 0);
}

// *************** Descriptor Writer *********************

DescriptorWriter::DescriptorWriter(DescriptorSetLayout &setLayout, DescriptorPool &pool)
    : setLayout{setLayout}, pool{pool} {}

DescriptorWriter& DescriptorWriter::writeBuffer(uint32_t binding, VkDescriptorBufferInfo *bufferInfo, uint32_t descriptorCount, uint32_t dstArrayElement) { assert(setLayout.bindings.count(binding) == 1 && "Layout does not contain specified binding");

    auto &bindingDescription = setLayout.bindings[binding];

    // allow descriptorCount <= bindingDescription.descriptorCount
    assert(descriptorCount <= bindingDescription.descriptorCount && "Trying to write more descriptors than binding supports");

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.descriptorType = bindingDescription.descriptorType;
    write.dstBinding = binding;
    write.dstArrayElement = dstArrayElement;
    write.pBufferInfo = bufferInfo;
    write.descriptorCount = descriptorCount;

    writes.push_back(write);
    return *this;
}

DescriptorWriter &DescriptorWriter::writeImage(
    uint32_t binding, VkDescriptorImageInfo *imageInfo, uint32_t descriptorCount, uint32_t dstArrayElement) {
    assert(setLayout.bindings.count(binding) == 1 && "Layout does not contain specified binding");

    auto &bindingDescription = setLayout.bindings[binding];

    // allow descriptorCount <= bindingDescription.descriptorCount
    assert(descriptorCount <= bindingDescription.descriptorCount && "Trying to write more descriptors than binding supports");

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.descriptorType = bindingDescription.descriptorType;
    write.dstBinding = binding;
    write.dstArrayElement = dstArrayElement;
    write.pImageInfo = imageInfo;
    write.descriptorCount = descriptorCount;

    writes.push_back(write);
    return *this;
}

bool DescriptorWriter::build(VkDescriptorSet &set) {
    bool success = pool.allocateDescriptor(setLayout.getDescriptorSetLayout(), set);
    if (!success) {
        return false;
    }
    overwrite(set);
    return true;
}

void DescriptorWriter::overwrite(VkDescriptorSet &set) {
    for (auto &write : writes) {
        write.dstSet = set;
    }
    vkUpdateDescriptorSets(pool.device.device(), writes.size(), writes.data(), 0, nullptr);
}
