#ifndef VGFX_DESCRIPTOR_POOL_BUILDER_H
#define VGFX_DESCRIPTOR_POOL_BUILDER_H

#include "VulkanGraphicsCompute.h"
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
        DescriptorPoolBuilder& addComputeShaderDescriptorSets(const ComputeShader& computeShader);
        DescriptorPoolBuilder& setCreateFlags(VkDescriptorPoolCreateFlags flags);

        std::unique_ptr<DescriptorPool> createPool(Context& context);

    private:
        void addDescriptorSets(const DescriptorSetLayouts& descriptorSetLayouts);

        std::map<VkDescriptorType, VkDescriptorPoolSize> m_descriptorPoolSizes;
        VkDescriptorPoolCreateFlags m_flags = 0;
        uint32_t m_maxSets = 0u;
    };
}

#endif
