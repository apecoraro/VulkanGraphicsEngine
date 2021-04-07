#ifndef VGFX_SAMPLER_H
#define VGFX_SAMPLER_H

#include "VulkanGraphicsContext.h"

#include <vulkan/vulkan.h>

namespace vgfx
{
    class Sampler
    {
    public:
        struct Config
        {
            Config(
                VkFilter magFilter = VK_FILTER_LINEAR,
                VkFilter minFilter = VK_FILTER_LINEAR,
                VkSamplerMipmapMode mipMapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                VkSamplerAddressMode addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                VkSamplerAddressMode addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                VkSamplerAddressMode addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                bool anisotropyEnable = false,
                float maxAnisotropy = 0u)
            {
                samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
                samplerInfo.magFilter = magFilter;
                samplerInfo.minFilter = minFilter;

                samplerInfo.addressModeU = addressModeU;
                samplerInfo.addressModeV = addressModeV;
                samplerInfo.addressModeW = addressModeW;

                samplerInfo.anisotropyEnable = anisotropyEnable ? VK_TRUE : VK_FALSE;
                samplerInfo.maxAnisotropy = maxAnisotropy;

                samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
                samplerInfo.unnormalizedCoordinates = VK_FALSE;

                samplerInfo.compareEnable = VK_FALSE;
                samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

                samplerInfo.mipmapMode = mipMapMode;
                samplerInfo.mipLodBias = 0.0f;
                samplerInfo.minLod = 0.0f;
                samplerInfo.maxLod = 0.0f;
            }

            bool operator<(const Sampler::Config& rhs) const
            {
                return this->samplerInfo.magFilter < rhs.samplerInfo.magFilter;
            }

            VkSamplerCreateInfo samplerInfo = {};
        };

        Sampler(
            Context& context,
            const Config& config);
        ~Sampler();

        VkSampler getHandle() const { return m_sampler; }

    protected:
        Context& m_context;

        VkSampler m_sampler = VK_NULL_HANDLE;
    };
}

#endif