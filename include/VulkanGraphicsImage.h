#ifndef QUANTUM_GFX_IMAGE_H
#define QUANTUM_GFX_IMAGE_H

#include "VulkanGraphicsContext.h"

#include <cstdint>

#include <vulkan/vulkan.h>

namespace vgfx
{
    class Image
    {
    public:
        struct Config
        {
            Config(
                uint32_t width,
                uint32_t height,
                VkFormat format,
                VkImageTiling tiling,
                VkImageUsageFlags usage)
            {
                imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
                imageInfo.imageType = VK_IMAGE_TYPE_2D;
                imageInfo.extent.width = width;
                imageInfo.extent.height = height;
                imageInfo.extent.depth = 1;
                imageInfo.mipLevels = 1;
                imageInfo.arrayLayers = 1;
                imageInfo.format = format;
                imageInfo.tiling = tiling;
                imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                imageInfo.usage = usage;
                imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
                imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            }

            VkImageCreateInfo imageInfo = {};
        };
        Image(
            Context& context,
            const Config& config);
        ~Image();

        uint32_t getWidth() const { return m_extent.width; }
        uint32_t getHeight() const { return m_extent.height; }
        uint32_t getDepth() const { return m_extent.depth; }
        VkFormat getFormat() const { return m_format; }

        VkImage getHandle() const { return m_image.handle; }
    private:
        Context& m_context;
        VkExtent3D m_extent = {};
        VkFormat m_format = VK_FORMAT_UNDEFINED;

        MemoryAllocator::Image m_image;
    };
}
#endif
