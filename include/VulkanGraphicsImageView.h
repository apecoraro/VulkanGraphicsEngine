#ifndef VGFX_IMAGE_VIEW_H
#define VGFX_IMAGE_VIEW_H

#include "VulkanGraphicsContext.h"
#include "VulkanGraphicsImage.h"

#include <vulkan/vulkan.h>

namespace vgfx
{
    class ImageView
    {
    public:
        struct Config
        {
            Config() = default;

            Config(
                VkFormat format, // format must be compatible with image's format.
                VkImageViewType viewType,
                uint32_t baseMipLevel,
                uint32_t mipMapLevels)
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
                imageViewInfo.subresourceRange.baseArrayLayer = 0;
                imageViewInfo.subresourceRange.layerCount = 1;
            }

            bool operator<(const ImageView::Config& rhs) const
            {
                return this->imageViewInfo.format < rhs.imageViewInfo.format;
            }

            VkImageViewCreateInfo imageViewInfo = {};
        };

        ImageView(
            Context& context,
            const Config& config,
            VkImage image);

        ImageView(
            Context& context,
            const Config& config,
            const vgfx::Image& image) : ImageView(context, config, image.getHandle()) {}

        ~ImageView()
        {
            destroy();
        }


        VkImageView getHandle() const { return m_imageView;  }
    private:
        void destroy();

        Context& m_context;
        VkImageView m_imageView = VK_NULL_HANDLE;
    };
}

#endif
