#include "VulkanGraphicsDescriptorPoolBuilder.h"

#include "VulkanGraphicsDescriptors.h"

#include <stdexcept>

namespace vgfx
{
    // Add to pool sizes for each Descriptor in the DescriptorSets of this Material
    DescriptorPoolBuilder& DescriptorPoolBuilder::addMaterialDescriptorSets(const Material& material)
    {
        for (const auto& descSetLayoutInfo : material.getDescriptorSetLayouts()) {
            for (const auto& descBindingCfg : descSetLayoutInfo.spDescriptorSetLayout->getDescriptorBindings()) {
                VkDescriptorPoolSize& poolSize = m_descriptorPoolSizes[descBindingCfg.second.descriptorType];
                poolSize.type = descBindingCfg.second.descriptorType;
                poolSize.descriptorCount +=
                    (descBindingCfg.second.arrayElementCount * static_cast<uint32_t>(descSetLayoutInfo.copyCount));
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
