#ifndef VGFX_DESCRIPTORS_H
#define VGFX_DESCRIPTORS_H

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

    struct DescriptorSetLayoutInfo
    {
        // The number of copies that DescriptorSets created from this layout need to have,
        // generally if it is updated every frame then there needs to be one copy for each swap chain
        // image that can be "in flight" simultaneously. If never updated then only one copy.
        std::unique_ptr<DescriptorSetLayout> spDescriptorSetLayout;
        size_t copyCount = 1u;
        DescriptorSetLayoutInfo(
            std::unique_ptr<DescriptorSetLayout> spDescSetLayout,
            size_t copies = 1u)
            : spDescriptorSetLayout(std::move(spDescSetLayout))
            , copyCount(copies)
        {
        }
    };

    using DescriptorSetLayouts = std::vector<DescriptorSetLayoutInfo>;

    struct DescriptorSetLayoutBindingInfo
    {
        DescriptorSetLayout::DescriptorBindings descriptorSetLayoutBindings;
        size_t copyCount = 1u;
        DescriptorSetLayoutBindingInfo(
            const DescriptorSetLayout::DescriptorBindings& bindings,
            size_t copies = 1u)
            : descriptorSetLayoutBindings(bindings)
            , copyCount(copies)
        {
        }
    };

    class DescriptorUpdater
    {
    public:
        DescriptorUpdater(VkDescriptorType type)
            : m_type(type)
        {
        }

        virtual ~DescriptorUpdater() = default;

        virtual void write(VkWriteDescriptorSet* pWriteSet)
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

    class DescriptorSet
    {
    public:
        DescriptorSet(VkDescriptorSet descSet)
            : m_descriptorSet(descSet)
        {
        }

        void update(Context& context, const std::map<uint32_t, DescriptorUpdater*>& descriptors);

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

        DescriptorSet& getDescriptorSet(size_t index) {
            return m_copies[index];
        }

    private:
        std::vector<DescriptorSet> m_copies;
    };
}

#endif
