#ifndef VGFX_DESCRIPTOR_POOL_BUILDER_H
#define VGFX_DESCRIPTOR_POOL_BUILDER_H

#include "VulkanGraphicsDescriptors.h"
#include "VulkanGraphicsMaterials.h"

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
