#include "VulkanGraphicsDescriptors.h"

#include "VulkanGraphicsMaterials.h"

#include <stdexcept>

namespace vgfx
{
    UniformBufferDescriptor::UniformBufferDescriptor(
        std::vector<std::unique_ptr<UniformBuffer>>&& uniformBuffers,
        const LayoutBindingConfig& layoutBindingConfig)
        : Descriptor(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, layoutBindingConfig)
        , m_uniformBuffers(std::move(uniformBuffers))
    {
    }

    void UniformBufferDescriptor::write(size_t setIndex, VkWriteDescriptorSet* pWriteSet)
    {
        Descriptor::write(setIndex, pWriteSet);

        VkWriteDescriptorSet& writeSet = *pWriteSet;
        writeSet.dstArrayElement = 0;

        auto& uniformBuffer = *m_uniformBuffers[setIndex].get();
        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.buffer = uniformBuffer.getHandle(setIndex);
        bufferInfo.offset = 0;
        bufferInfo.range = uniformBuffer.getSize();

        writeSet.pBufferInfo = &bufferInfo;
    }

    CombinedImageSamplerDescriptor::CombinedImageSamplerDescriptor(
        std::unique_ptr<CombinedImageSampler> spCombinedImageSampler,
        const LayoutBindingConfig& layoutBindingConfig)
        : Descriptor(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, layoutBindingConfig)
        , m_spCombinedImageSampler(std::move(spCombinedImageSampler))
    {

    }

    void CombinedImageSamplerDescriptor::write(
        size_t setIndex,
        VkWriteDescriptorSet* pWriteSet)
    {
        Descriptor::write(setIndex, pWriteSet);

        VkWriteDescriptorSet& writeSet = *pWriteSet;
        // TODO should dstArrayElement not be hard coded?
        writeSet.dstArrayElement = 0;

        // TODO should imageLayout not be hard coded?
        m_imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        m_imageInfo.imageView = m_spCombinedImageSampler->getImageView().getHandle();
        m_imageInfo.sampler = m_spCombinedImageSampler->getSampler().getHandle();

        writeSet.pImageInfo = &m_imageInfo;
    }

    DescriptorSetBuffer::DescriptorSetBuffer(
        Context& context,
        std::vector<std::unique_ptr<Descriptor>>&& descriptors,
        uint32_t bufferCount)
        : m_context(context)
        , m_descriptors(std::move(descriptors))
        , m_descriptorWrites(m_descriptors.size())
        , m_descriptorSets(bufferCount)
    {
        std::vector<VkDescriptorSetLayoutBinding> descriptorBindings;
        descriptorBindings.reserve(m_descriptors.size());
        uint32_t index = 0u;
        for (const auto& descriptor : m_descriptors) {
            VkDescriptorSetLayoutBinding binding = {};

            binding.binding = index;
            ++index;

            binding.descriptorType = descriptor->getType();
            binding.descriptorCount = descriptor->getCount();
            binding.stageFlags = descriptor->getStageFlags();
            binding.pImmutableSamplers = nullptr; // Optional

            descriptorBindings.push_back(binding);
        }

        VkDescriptorSetLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(descriptorBindings.size());
        layoutInfo.pBindings = descriptorBindings.data();

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

    DescriptorSetBuffer::~DescriptorSetBuffer()
    {
        if (m_descriptorSetLayout) {
            vkDestroyDescriptorSetLayout(
                m_context.getLogicalDevice(),
                m_descriptorSetLayout,
                m_context.getAllocationCallbacks());
        }
    }

    void DescriptorSetBuffer::init(DescriptorPool& pool)
    { 
        pool.allocateDescriptorSets(m_descriptorSetLayout, &m_descriptorSets);

        for (size_t index = 0u; index < m_descriptorSets.size(); ++index) {
            writeDescriptorSet(index);
        }
    }

    void DescriptorSetBuffer::writeDescriptorSet(size_t descriptorSetIndex)
    {
        VkDescriptorSet descriptorSet = m_descriptorSets[descriptorSetIndex];
        for (size_t dindex = 0u; dindex < m_descriptors.size(); ++dindex) {
            auto& spDescriptor = m_descriptors[dindex];
            VkWriteDescriptorSet& descriptorWrite = m_descriptorWrites[dindex];
            descriptorWrite.dstBinding = static_cast<uint32_t>(dindex);
            descriptorWrite.dstSet = descriptorSet;

            spDescriptor->write(descriptorSetIndex, &descriptorWrite);
        }

        vkUpdateDescriptorSets(
            m_context.getLogicalDevice(),
            static_cast<uint32_t>(m_descriptorWrites.size()),
            m_descriptorWrites.data(), 0, nullptr);
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
        VkDescriptorSetLayout layout,
        std::vector<VkDescriptorSet>* pDescriptorSets)
    {
        size_t count = pDescriptorSets->size();
        std::vector<VkDescriptorSetLayout> layouts(count, layout);
        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_descriptorPool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(count);
        allocInfo.pSetLayouts = layouts.data();

        VkResult result = vkAllocateDescriptorSets(m_context.getLogicalDevice(), &allocInfo, pDescriptorSets->data());
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate descriptor sets!");
        }
    }

    // Add to pool sizes for each Descriptor in the DescriptorSet
    DescriptorPoolBuilder& DescriptorPoolBuilder::addMaterialDescriptorSets(const Material& material)
    {
        for (size_t i = 0; i < material.getDescriptorSetCount(); ++i) {
            const DescriptorSetBuffer& descSetBuffer = material.getDescriptorSet(i);
            for (const auto& spDescriptor : descSetBuffer.getDescriptors()) {
                VkDescriptorPoolSize& poolSize = m_descriptorPoolSizes[spDescriptor->getType()];
                poolSize.type = spDescriptor->getType();
                poolSize.descriptorCount += (spDescriptor->getCount() * static_cast<uint32_t>(descSetBuffer.getCopyCount()));
            }
        }

        return *this;
    }

    DescriptorPoolBuilder& DescriptorPoolBuilder::setCreateFlags(VkDescriptorPoolCreateFlags flags)
    {
        m_flags = flags;
        return *this;
    }

    std::unique_ptr<DescriptorPool> DescriptorPoolBuilder::createPool(
        Context& context,
        uint32_t maxSets)
    {
        std::vector<VkDescriptorPoolSize> descriptorPoolSizes;
        for (const auto& itr : m_descriptorPoolSizes) {
            descriptorPoolSizes.push_back(itr.second);
        }

        return std::make_unique<DescriptorPool>(context, descriptorPoolSizes, maxSets, m_flags);
    }
}
