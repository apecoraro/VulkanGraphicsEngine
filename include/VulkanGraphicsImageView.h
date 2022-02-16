#pragma once

#include "VulkanGraphicsContext.h"

#include <vulkan/vulkan.h>

namespace vgfx
{
    class Image;

    class ImageView
    {
    public:
        struct Config
        {
            Config() = default;

            Config(
                VkFormat format, // renderTargetFormat must be compatible with image's renderTargetFormat.
                VkImageViewType viewType,
                uint32_t baseMipLevel = 0u,
                uint32_t mipMapLevels = 1u)
            { 
                imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                imageViewInfo.image = VK_NULL_HANDLE;
                imageViewInfo.format = format;
                imageViewInfo.viewType = viewType,
                imageViewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
                imageViewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
                imageViewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
                imageViewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
                auto formatToAspectMask = [](VkFormat fmt) -> VkImageAspectFlags {
                    if (fmt >= VK_FORMAT_D16_UNORM && fmt <= VK_FORMAT_D32_SFLOAT_S8_UINT) {
                        return VK_IMAGE_ASPECT_DEPTH_BIT;
                    } else {
                        return VK_IMAGE_ASPECT_COLOR_BIT;
                    }
                };
                imageViewInfo.subresourceRange.aspectMask = formatToAspectMask(format);
                imageViewInfo.subresourceRange.baseMipLevel = baseMipLevel;
                imageViewInfo.subresourceRange.levelCount = mipMapLevels;
                imageViewInfo.subresourceRange.baseArrayLayer = 0u;
                imageViewInfo.subresourceRange.layerCount = 1u;
            }

            bool operator<(const ImageView::Config& rhs) const
            {
                return this->imageViewInfo.format < rhs.imageViewInfo.format;
            }

            VkImageViewCreateInfo imageViewInfo = {};
        };

        ImageView(Context& context, const Config& config, const Image& image);

        ~ImageView()
        {
            destroy();
        }

        const Image& getImage() const { return m_image;  }

        VkFormat getFormat() const { return m_format; }

        VkImageView getHandle() const { return m_imageView;  }

    private:
        void destroy();

        Context& m_context;
        const Image& m_image;
        VkFormat m_format = VK_FORMAT_UNDEFINED; // ImageView format may differ from Image's format.
        VkImageView m_imageView = VK_NULL_HANDLE;
    };
}
