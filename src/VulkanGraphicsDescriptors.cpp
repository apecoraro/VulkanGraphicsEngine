#include "VulkanGraphicsDescriptors.h"

#include <stdexcept>

namespace vgfx
{
    DescriptorSetLayout::DescriptorSetLayout(
        Context& context,
        const DescriptorBindings& descriptorBindings)
        : m_context(context)
        , m_descriptorBindings(descriptorBindings)
    {
        std::vector<VkDescriptorSetLayoutBinding> descriptorLayoutBindings;
        descriptorLayoutBindings.reserve(m_descriptorBindings.size());

        std::vector<VkDescriptorBindingFlags> bindingFlags;
        bindingFlags.reserve(m_descriptorBindings.size());

        for (const auto& descBindingCfg : m_descriptorBindings) {
            VkDescriptorSetLayoutBinding binding = {};

            binding.binding = descBindingCfg.first;

            binding.descriptorType = descBindingCfg.second.descriptorType;
            binding.descriptorCount = descBindingCfg.second.arrayElementCount;
            binding.stageFlags = descBindingCfg.second.shaderStageFlags;
            binding.pImmutableSamplers = descBindingCfg.second.pImmutableSamplers;

            descriptorLayoutBindings.push_back(binding);

            if (descBindingCfg.second.mode == BindMode::SupportPartialBinding) {
                bindingFlags.push_back(VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT);
            } else {
                bindingFlags.push_back(0);
            }
        }

        VkDescriptorSetLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(descriptorBindings.size());
        layoutInfo.pBindings = descriptorLayoutBindings.data();

        VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo = {};
        bindingFlagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
        bindingFlagsInfo.bindingCount = static_cast<uint32_t>(descriptorBindings.size());
        bindingFlagsInfo.pBindingFlags = bindingFlags.data();

        layoutInfo.pNext = &bindingFlagsInfo;

        VkResult result =
            vkCreateDescriptorSetLayout(
                context.getLogicalDevice(),
                &layoutInfo,
                context.getAllocationCallbacks(),
                &m_descriptorSetLayout);

        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to create descriptor set layout!");
        }
    }

    DescriptorSetLayout::~DescriptorSetLayout()
    {
        if (m_descriptorSetLayout) {
            vkDestroyDescriptorSetLayout(
                m_context.getLogicalDevice(),
                m_descriptorSetLayout,
                m_context.getAllocationCallbacks());
        }
    }

    void DescriptorSetUpdater::bindDescriptor(
        uint32_t bindingIndex,
        DescriptorUpdater& updater)
    {
        VkWriteDescriptorSet descriptorWrite = {};
        descriptorWrite.dstBinding = bindingIndex;
        updater.update(&descriptorWrite);

        m_descriptorWrites.push_back(descriptorWrite);
    }

    void DescriptorSetUpdater::updateDescriptorSet(
        Context& context,
        VkDescriptorSet descriptorSet)
    {
        for (VkWriteDescriptorSet& descriptorWrite: m_descriptorWrites)
        {
            descriptorWrite.dstSet = descriptorSet;
        }

        vkUpdateDescriptorSets(
            context.getLogicalDevice(),
            static_cast<uint32_t>(m_descriptorWrites.size()),
            m_descriptorWrites.data(), 0, nullptr);

        m_descriptorWrites.clear();
    }

    DescriptorPool::DescriptorPool(
        Context& context,
        const std::vector<VkDescriptorPoolSize>& descriptorPoolSizes,
        uint32_t maxSets,
        VkDescriptorPoolCreateFlags flags)
        : m_context(context)
    {
        VkDescriptorPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(descriptorPoolSizes.size());
        poolInfo.pPoolSizes = descriptorPoolSizes.data();
        poolInfo.maxSets = maxSets;
        poolInfo.flags = flags;

        VkResult result = vkCreateDescriptorPool(
            context.getLogicalDevice(),
            &poolInfo,
            context.getAllocationCallbacks(),
            &m_descriptorPool);

        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to create descriptor pool!");
        }
    }

    DescriptorPool::~DescriptorPool()
    {
        if (m_descriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(
                m_context.getLogicalDevice(),
                m_descriptorPool,
                m_context.getAllocationCallbacks());
        }
    }

    void DescriptorPool::allocateDescriptorSets(
        const DescriptorSetLayout& layout,
        uint32_t count,
        VkDescriptorSet* pDescriptorSetHandles)
    {
        std::vector<VkDescriptorSetLayout> layouts(count, layout.getHandle());
        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_descriptorPool;
        allocInfo.descriptorSetCount = count;
        allocInfo.pSetLayouts = layouts.data();

        VkResult result = vkAllocateDescriptorSets(m_context.getLogicalDevice(), &allocInfo, pDescriptorSetHandles);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate descriptor sets!");
        }
    }

    void DescriptorPool::freeDescriptorSets(std::vector<VkDescriptorSet>& descriptorSetHandles)
    {
        VkResult result = vkFreeDescriptorSets(m_context.getLogicalDevice(), m_descriptorPool, static_cast<uint32_t>(descriptorSetHandles.size()), descriptorSetHandles.data());
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate descriptor sets!");
        }
    }

    void DescriptorPool::reset(VkDescriptorPoolResetFlags flags)
    {
        VkResult result = vkResetDescriptorPool(m_context.getLogicalDevice(), m_descriptorPool, flags);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate descriptor sets!");
        }
    }
}
