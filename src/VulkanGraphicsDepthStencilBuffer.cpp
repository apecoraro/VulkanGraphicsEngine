#include "VulkanGraphicsDepthStencilBuffer.h"

#include <cassert>
#include <set>
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

    VkFormat DepthStencilBuffer::PickFormat(
        VkPhysicalDevice physicalDevice,
        PickDepthStencilFormatFunc pickFormatFunc)
    {
        VkFormat candidates[] = {
            VK_FORMAT_D16_UNORM,
            VK_FORMAT_X8_D24_UNORM_PACK32,
            VK_FORMAT_D32_SFLOAT,
            VK_FORMAT_S8_UINT,
            VK_FORMAT_D16_UNORM_S8_UINT,
            VK_FORMAT_D24_UNORM_S8_UINT,
            VK_FORMAT_D32_SFLOAT_S8_UINT,
        };
        std::set<VkFormat> supportedDepthStencilFormats;
        for (VkFormat format : candidates) {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

            if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
                supportedDepthStencilFormats.insert(format);
            }
        }

        return pickFormatFunc(supportedDepthStencilFormats);
    }

    DepthStencilBuffer::DepthStencilBuffer(
        Context& context,
        const Config& config)
        : m_spImage(new Image(context, GetImageConfig(config.width, config.height, config.format)))
    {
    }
}

