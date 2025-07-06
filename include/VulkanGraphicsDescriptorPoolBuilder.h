#pragma once

#include "VulkanGraphicsCompute.h"
#include "VulkanGraphicsDescriptors.h"
#include "VulkanGraphicsEffects.h"

#include <map>
#include <vector>

#include <vulkan/vulkan.h>

namespace vgfx
{
    // Builds a DescriptorPool
    class DescriptorPoolBuilder
    {
    public:
        DescriptorPoolBuilder() = default;

        DescriptorPoolBuilder& addDescriptors(VkDescriptorType type, uint32_t count);
        DescriptorPoolBuilder& addDescriptorSets(const DescriptorSetLayouts& descriptorSetLayouts, uint32_t perTypeScaleFactor);
        DescriptorPoolBuilder& addMaxSets(uint32_t count);
        // Add to pool sizes for each Descriptor in the DescriptorSet
        DescriptorPoolBuilder& setCreateFlags(VkDescriptorPoolCreateFlags flags);

        std::unique_ptr<DescriptorPool> createPool(Context& context);

    private:

        std::map<VkDescriptorType, VkDescriptorPoolSize> m_descriptorPoolSizes;
        VkDescriptorPoolCreateFlags m_flags = 0;
        uint32_t m_maxSets = 0u;
    };
}

