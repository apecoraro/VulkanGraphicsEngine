#include "VulkanGraphicsDepthStencilBuffer.h"

#include <cassert>
#include <stdexcept>

namespace vgfx
{
    static Image::Config GetImageConfig(
        uint32_t width,
        uint32_t height,
        VkFormat format)
    {
        Image::Config depthImageCfg(
            width,
            height,
            format,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
        return depthImageCfg;
    }

    DepthStencilBuffer::DepthStencilBuffer(
        Context& context,
        const Config& config)
        : m_spImage(new Image(context, GetImageConfig(config.width, config.height, config.format)))
    {
    }
}

