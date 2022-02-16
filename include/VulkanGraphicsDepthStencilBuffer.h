#pragma once

#include "VulkanGraphicsContext.h"
#include "VulkanGraphicsImage.h"
#include "VulkanGraphicsImageView.h"

#include <memory>

#include <vulkan/vulkan.h>

namespace vgfx
{
    class DepthStencilBuffer
    {
    public:
        struct Config
        {
            uint32_t width = 0u;
            uint32_t height = 0u;
            VkFormat format = VK_FORMAT_D24_UNORM_S8_UINT;
            Config(uint32_t w, uint32_t h, VkFormat f = VK_FORMAT_D24_UNORM_S8_UINT)
                : width(w)
                , height(h)
                , format(f)
            {
            }
        };
        DepthStencilBuffer(
            Context& context,
            const Config& config);

        VkFormat getFormat() const { return m_spImage->getFormat(); }
        VkSampleCountFlagBits getSampleCount() const { return m_spImage->getSampleCount(); }
        uint32_t getWidth() const { return m_spImage->getWidth(); }
        uint32_t getHeight() const { return m_spImage->getHeight(); }
        ImageView& getOrCreateImageView(const ImageView::Config& config) const { return m_spImage->getOrCreateView(config); }

    private:
        std::unique_ptr<Image> m_spImage;
    };
}

