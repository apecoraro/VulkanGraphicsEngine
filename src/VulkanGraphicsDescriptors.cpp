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

    void DescriptorSet::update(
        Context& context,
        const std::map<uint32_t, DescriptorUpdater*>& descriptors)
    {
        m_descriptorWrites.resize(descriptors.size());
        size_t dindex = 0u;
        for (auto& [binding, pDescriptor]: descriptors) {
            VkWriteDescriptorSet& descriptorWrite = m_descriptorWrites[dindex];
            descriptorWrite = {};
            descriptorWrite.dstBinding = binding;
            descriptorWrite.dstSet = m_descriptorSet;
            pDescriptor->write(&descriptorWrite);
            ++dindex;
        }

        vkUpdateDescriptorSets(
            context.getLogicalDevice(),
            static_cast<uint32_t>(m_descriptorWrites.size()),
            m_descriptorWrites.data(), 0, nullptr);
    }

    DescriptorSetBuffer::DescriptorSetBuffer(size_t bufferCopies)
    {
        m_copies.reserve(bufferCopies);
    }

    void DescriptorSetBuffer::init(
        const DescriptorSetLayout& layout,
        DescriptorPool& pool)
    {
        pool.allocateDescriptorSets(
            layout,
            static_cast<uint32_t>(m_copies.capacity()),
            &m_copies);
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
        std::vector<DescriptorSet>* pDescriptorSets)
    {
        std::vector<VkDescriptorSetLayout> layouts(count, layout.getHandle());
        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_descriptorPool;
        allocInfo.descriptorSetCount = count;
        allocInfo.pSetLayouts = layouts.data();

        std::vector<VkDescriptorSet> descriptorSetHandles(count);
        VkResult result = vkAllocateDescriptorSets(m_context.getLogicalDevice(), &allocInfo, descriptorSetHandles.data());
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate descriptor sets!");
        }

        for (size_t i = 0; i < descriptorSetHandles.size(); ++i) {
            pDescriptorSets->push_back(DescriptorSet(descriptorSetHandles[i]));
        }
    }
}
