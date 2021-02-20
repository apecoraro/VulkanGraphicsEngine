#ifndef VGFX_DESCRIPTORS_H
#define VGFX_DESCRIPTORS_H

#include "VulkanGraphicsContext.h"
#include "VulkanGraphicsCombinedImageSampler.h"
#include "VulkanGraphicsUniformBuffer.h"

#include <map>
#include <vector>

#include <vulkan/vulkan.h>

namespace vgfx
{
    class Descriptor
    {
    public:
        struct LayoutBindingConfig
        {
            // Mask of stages that access this descriptor.
            VkShaderStageFlags shaderStageFlags = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
            // Corresponds to the descriptorCount field of VkDescriptorSetLayoutBinding,
            // indicates this Descriptor is an array with arrayElementCount elements.
            uint32_t arrayElementCount = 1;
            const VkSampler* pImmutableSamplers = nullptr;

            LayoutBindingConfig(
                VkShaderStageFlags shaderStage,
                uint32_t arrayElemCount = 1)
                : shaderStageFlags(shaderStage)
                , arrayElementCount(arrayElemCount)
            {

            }
        };

        Descriptor(
            VkDescriptorType type,
            const LayoutBindingConfig& layoutBindingConfig)
            : m_type(type)
            , m_layoutBindingConfig(layoutBindingConfig)
        {

        }
        virtual ~Descriptor() = default;

        VkDescriptorType getType() const { return m_type; }
        uint32_t getCount() const { return m_layoutBindingConfig.arrayElementCount;  }
        VkShaderStageFlags getStageFlags() const{ return m_layoutBindingConfig.shaderStageFlags;  }

        virtual void write(size_t setIndex, VkWriteDescriptorSet* pWriteSet)
        {
            VkWriteDescriptorSet& writeSet = *pWriteSet;
            writeSet.descriptorType = m_type;
            writeSet.descriptorCount = m_layoutBindingConfig.arrayElementCount;
        }

    protected:
        VkDescriptorType m_type = VK_DESCRIPTOR_TYPE_MAX_ENUM;
        LayoutBindingConfig m_layoutBindingConfig;
    };

    class UniformBufferDescriptor : public Descriptor
    {
    public:
        // VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
        UniformBufferDescriptor(
            std::vector<std::unique_ptr<UniformBuffer>>&& uniformBuffers,
            const LayoutBindingConfig& layoutBindingConfig);

        virtual ~UniformBufferDescriptor() = default;

        void write(size_t setIndex, VkWriteDescriptorSet* pWriteSet) override;

    private:
        std::vector<std::unique_ptr<UniformBuffer>> m_uniformBuffers;
    };

    class CombinedImageSamplerDescriptor : public Descriptor
    {
    public:
        // VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
        CombinedImageSamplerDescriptor(
            std::unique_ptr<CombinedImageSampler> spCombinedImageSampler,
            const LayoutBindingConfig& layoutBindingConfig);

        virtual ~CombinedImageSamplerDescriptor() = default;

        void write(size_t setIndex, VkWriteDescriptorSet* pWriteSet) override;

    private:
        std::unique_ptr<CombinedImageSampler> m_spCombinedImageSampler;
        VkDescriptorImageInfo m_imageInfo = {};
    };

    // Wraps a VkDescriptorPool.
    class DescriptorPool
    {
    public:
        DescriptorPool(
            Context& context,
            const std::vector<VkDescriptorPoolSize>& descriptorPoolSizes,
            uint32_t maxSets,
            VkDescriptorPoolCreateFlags flags);
        ~DescriptorPool();

        void allocateDescriptorSets(
            VkDescriptorSetLayout layout,
            std::vector<VkDescriptorSet>* pDescriptorSets);
    private:
        Context& m_context;
        VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    };

    // Represents one or more instances of a VkDescriptorSet where each
    // VkDescriptorSet has the same layout.
    class DescriptorSetBuffer
    {
    public:
        DescriptorSetBuffer(
            Context& context,
            std::vector<std::unique_ptr<Descriptor>>&& descriptors,
            uint32_t bufferCount = 1);
        virtual ~DescriptorSetBuffer();

        VkDescriptorSetLayout getLayout() const { return m_descriptorSetLayout; }

        void init(DescriptorPool& pool);

        virtual size_t getCopyCount() const { return m_descriptorSets.size();  }
        virtual const std::vector<std::unique_ptr<Descriptor>>& getDescriptors() const { return m_descriptors;  }

        const std::vector<VkDescriptorSet> getDescriptorSetCopies() const {
            return m_descriptorSets;
        }
    private:
        void writeDescriptorSet(size_t descriptorSetIndex);

        Context& m_context;
        std::vector<std::unique_ptr<Descriptor>> m_descriptors;
        std::vector<VkWriteDescriptorSet> m_descriptorWrites;
        std::vector<VkDescriptorSet> m_descriptorSets;
        VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    };

    // Forward declaration.
    class Material;

    // Builds a DescriptorPool
    class DescriptorPoolBuilder
    {
    public:
        DescriptorPoolBuilder() = default;

        // Add to pool sizes for each Descriptor in the DescriptorSet
        DescriptorPoolBuilder& addMaterialDescriptorSets(const Material& material);
        DescriptorPoolBuilder& setCreateFlags(VkDescriptorPoolCreateFlags flags);

        std::unique_ptr<DescriptorPool> createPool(
            Context& context,
            uint32_t maxSets);

    private:
        std::map<VkDescriptorType, VkDescriptorPoolSize> m_descriptorPoolSizes;
        VkDescriptorPoolCreateFlags m_flags = 0;
    };
}

#endif
