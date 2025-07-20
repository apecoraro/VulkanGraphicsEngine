#include "VulkanGraphicsDescriptorPoolBuilder.h"

#include "VulkanGraphicsDescriptors.h"

#include <stdexcept>

namespace vgfx
{
    DescriptorPoolBuilder& DescriptorPoolBuilder::addDescriptors(VkDescriptorType type, uint32_t count)
    {
        VkDescriptorPoolSize& poolSize = m_descriptorPoolSizes[type];
        poolSize.type = type;
        poolSize.descriptorCount += count;
        return *this;
    }

    DescriptorPoolBuilder& DescriptorPoolBuilder::addDescriptorSets(const DescriptorSetLayouts& descriptorSetLayouts, uint32_t perTypeScaleFactor)
    {
        // For each type of descriptor in this layout add perTypeScaleFactor to the pools total for that type of descriptor.
        for (const auto& spDescSetLayout : descriptorSetLayouts) {
            for (const auto& descBindingCfg : spDescSetLayout->getDescriptorBindings()) {
                VkDescriptorPoolSize& poolSize = m_descriptorPoolSizes[descBindingCfg.second.descriptorType];
                poolSize.type = descBindingCfg.second.descriptorType;
                poolSize.descriptorCount += (descBindingCfg.second.arrayElementCount * perTypeScaleFactor);
            }
        }
        return *this;
    }

    DescriptorPoolBuilder& DescriptorPoolBuilder::addMaxSets(uint32_t count)
    {
        // Increase the number of descriptor sets that can be allocated from the pool by the specified count.
        m_maxSets += count;
        return *this;
    }

    DescriptorPoolBuilder& DescriptorPoolBuilder::setCreateFlags(VkDescriptorPoolCreateFlags flags)
    {
        m_flags = flags;
        return *this;
    }

    std::unique_ptr<DescriptorPool> DescriptorPoolBuilder::createPool(Context& context)
    {
        std::vector<VkDescriptorPoolSize> descriptorPoolSizes;
        for (const auto& itr : m_descriptorPoolSizes) {
            descriptorPoolSizes.push_back(itr.second);
        }

        return std::make_unique<DescriptorPool>(context, descriptorPoolSizes, m_maxSets, m_flags);
    }
}
