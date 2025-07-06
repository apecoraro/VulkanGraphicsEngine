#pragma once

#include "VulkanGraphicsContext.h"

#include <map>
#include <memory>
#include <vector>

#include <vulkan/vulkan.h>

namespace vgfx
{
    class DescriptorSetLayout
    {
    public:
        enum class BindMode
        {
            Normal,
            SupportPartialBinding, // https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/VkPhysicalDeviceDescriptorIndexingFeatures.html
        };

        struct DescriptorBinding
        {
            VkDescriptorType descriptorType = VK_DESCRIPTOR_TYPE_MAX_ENUM;
            // Mask of stages that access this descriptor.
            VkShaderStageFlags shaderStageFlags = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
            // Corresponds to the descriptorCount field of VkDescriptorSetLayoutBinding,
            // indicates this Descriptor is an array with arrayElementCount elements.
            uint32_t arrayElementCount = 1u;

            BindMode mode;

            const VkSampler* pImmutableSamplers = nullptr;

            DescriptorBinding(
                VkDescriptorType descType,
                VkShaderStageFlags shaderStage,
                uint32_t arrayElemCount = 1u,
                BindMode bindMode = BindMode::Normal,
                const VkSampler* pImmSplrs = nullptr)
                : descriptorType(descType)
                , shaderStageFlags(shaderStage)
                , arrayElementCount(arrayElemCount)
                , mode(bindMode)
                , pImmutableSamplers(pImmSplrs)
            {
            }

            DescriptorBinding() = default;
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

    using DescriptorSetLayouts = std::vector<std::unique_ptr<DescriptorSetLayout>>;

    class DescriptorUpdater
    {
    public:
        DescriptorUpdater(VkDescriptorType type)
            : m_type(type)
        {
        }

        virtual ~DescriptorUpdater() = default;

        virtual void update(VkWriteDescriptorSet* pWriteSet) const
        {
            VkWriteDescriptorSet& writeSet = *pWriteSet;
            writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeSet.descriptorType = m_type;
            // Subclasses that want to update more than one descriptor can override this value.
            writeSet.descriptorCount = 1;
        }

    protected:
        VkDescriptorType m_type = VK_DESCRIPTOR_TYPE_MAX_ENUM;
    };

    class DescriptorSetUpdater
    {
    public:
        DescriptorSetUpdater()
        {
        }

        void bindDescriptor(uint32_t bindingIndex, DescriptorUpdater& updater);
        void updateDescriptorSet(Context& context, VkDescriptorSet descriptorSet);

    private:
        std::vector<VkWriteDescriptorSet> m_descriptorWrites;
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
            VkDescriptorSet* pDescriptorSetHandles);

        void freeDescriptorSets(
            std::vector<VkDescriptorSet>& descriptorSetHandles);

        void reset(VkDescriptorPoolResetFlags flags=0u);
    private:
        Context& m_context;
        VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
        uint32_t m_maxSets = 0u;
    }; 
}

