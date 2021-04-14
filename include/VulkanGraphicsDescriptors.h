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
    class DescriptorSetLayout
    {
    public:
        struct DescriptorBinding
        {
            // The number of copies that this descriptors needs, generally if this descriptor is
            // going to be updated every frame then there needs to be one copy for each swap chain
            // image. If never updated then only one copy.
            size_t copyCount = 1u;
            VkDescriptorType descriptorType = VK_DESCRIPTOR_TYPE_MAX_ENUM;
            // Mask of stages that access this descriptor.
            VkShaderStageFlags shaderStageFlags = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
            // Corresponds to the descriptorCount field of VkDescriptorSetLayoutBinding,
            // indicates this Descriptor is an array with arrayElementCount elements.
            uint32_t arrayElementCount = 1u;
            const VkSampler* pImmutableSamplers = nullptr;

            DescriptorBinding(
                size_t copies,
                VkDescriptorType descType,
                VkShaderStageFlags shaderStage,
                uint32_t arrayElemCount = 1u,
                const VkSampler* pImmSplrs = nullptr)
                : copyCount(copies)
                , descriptorType(descType)
                , shaderStageFlags(shaderStage)
                , arrayElementCount(arrayElemCount)
                , pImmutableSamplers(pImmSplrs)
            {
            }
        };

        using BindingIndex = uint32_t; // VkDescriptorSetLayoutBinding::binding
        using DescriptorBindings = std::map<BindingIndex, DescriptorBinding>;

        DescriptorSetLayout(
            Context& context,
            const DescriptorBindings& descriptorBindings);

        ~DescriptorSetLayout();

        VkDescriptorSetLayout getHandle() const { return m_descriptorSetLayout; }

        const DescriptorBindings& getDescriptorBindings() const
        {
            return m_descriptorBindings;
        }
    private:
        Context& m_context;
        DescriptorBindings m_descriptorBindings;
        VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    };

    class Descriptor
    {
    public:
        Descriptor(VkDescriptorType type)
            : m_type(type)
        {
        }

        virtual ~Descriptor() = default;

        virtual void write(VkWriteDescriptorSet* pWriteSet)
        {
            VkWriteDescriptorSet& writeSet = *pWriteSet;
            writeSet.descriptorType = m_type;
            writeSet.descriptorCount = 1;
        }

    protected:
        VkDescriptorType m_type = VK_DESCRIPTOR_TYPE_MAX_ENUM;
    };

    class UniformBufferDescriptor : public Descriptor
    {
    public:
        // VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
        UniformBufferDescriptor(UniformBuffer& uniformBuffer)
            : Descriptor(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
            , m_uniformBuffer(uniformBuffer)
        {
        }

        virtual ~UniformBufferDescriptor() = default;

        void write(VkWriteDescriptorSet* pWriteSet) override;

    private:
        UniformBuffer& m_uniformBuffer;
        VkDescriptorBufferInfo m_bufferInfo = {};
    };

    class CombinedImageSamplerDescriptor : public Descriptor
    {
    public:
        CombinedImageSamplerDescriptor(const CombinedImageSampler& combinedImageSampler)
            : Descriptor(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
            , m_combinedImageSampler(combinedImageSampler)
        {
        }

        virtual ~CombinedImageSamplerDescriptor() = default;

        void write(VkWriteDescriptorSet* pWriteSet) override;

    private:
        CombinedImageSampler m_combinedImageSampler;
        VkDescriptorImageInfo m_imageInfo = {};
    };

    class DescriptorSet
    {
    public:
        DescriptorSet(VkDescriptorSet descSet)
            : m_descriptorSet(descSet)
        {
        }

        void update(Context& context, const std::map<uint32_t, std::unique_ptr<Descriptor>>& descriptors);

        VkDescriptorSet getHandle() const { return m_descriptorSet; }
    private:
        std::vector<VkWriteDescriptorSet> m_descriptorWrites;
        VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;
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
            const DescriptorSetLayout& layout,
            uint32_t count,
            std::vector<DescriptorSet>* pDescriptorSets);
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
            size_t bufferCopies);
        ~DescriptorSetBuffer() = default;

        void init(
            const DescriptorSetLayout& layout,
            DescriptorPool& pool);

        virtual size_t getCopyCount() const { return m_copies.size();  }

        const DescriptorSet& getDescriptorSet(size_t index) const {
            return m_copies[index];
        }

    private:
        std::vector<DescriptorSet> m_copies;
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
