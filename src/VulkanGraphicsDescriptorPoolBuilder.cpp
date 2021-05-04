#include "VulkanGraphicsDescriptorPoolBuilder.h"

#include "VulkanGraphicsDescriptors.h"

#include <stdexcept>

namespace vgfx
{
    DescriptorPoolBuilder& DescriptorPoolBuilder::addMaterialDescriptorSets(const Material& material)
    {
        addDescriptorSets(material.getDescriptorSetLayouts());
        return *this;
    }

    DescriptorPoolBuilder& DescriptorPoolBuilder::addComputeShaderDescriptorSets(const ComputeShader& computeShader)
    {
        addDescriptorSets(computeShader.getDescriptorSetLayouts());
        return *this;
    }

    void DescriptorPoolBuilder::addDescriptorSets(const DescriptorSetLayouts& descriptorSetLayouts)
    {
        for (const auto& descSetLayoutInfo : descriptorSetLayouts) {
            for (const auto& descBindingCfg : descSetLayoutInfo.spDescriptorSetLayout->getDescriptorBindings()) {
                VkDescriptorPoolSize& poolSize = m_descriptorPoolSizes[descBindingCfg.second.descriptorType];
                poolSize.type = descBindingCfg.second.descriptorType;
                poolSize.descriptorCount +=
                    (descBindingCfg.second.arrayElementCount * static_cast<uint32_t>(descSetLayoutInfo.copyCount));
            }
        }
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
