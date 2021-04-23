#ifndef VGFX_IMAGE_H
#define VGFX_IMAGE_H

#include "VulkanGraphicsContext.h"

#include <cstdint>

#include <vulkan/vulkan.h>

namespace vgfx
{
    class Image
    {
    public:
        static uint32_t ComputeMipLevels2D(uint32_t width, uint32_t height);

        struct Config
        {
            Config(
                uint32_t width,
                uint32_t height,
                VkFormat format,
                VkImageTiling tiling,
                VkImageUsageFlags usage,
                uint32_t mipLevels = 1u)
            {
                imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
                imageInfo.imageType = VK_IMAGE_TYPE_2D;
                imageInfo.extent.width = width;
                imageInfo.extent.height = height;
                imageInfo.extent.depth = 1u;
                imageInfo.arrayLayers = 1u;
                imageInfo.format = format;
                imageInfo.tiling = tiling;
                imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                imageInfo.usage = usage;
                imageInfo.mipLevels = mipLevels;
                imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
                imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            }

            VkImageCreateInfo imageInfo = {};
        };
        Image(
            Context& context,
            const Config& config);

        // Creates an image and, if it is configured to have more than one mip map level,
        // then generates the mip map levels (REQUIRES VK_IMAGE_USAGE_TRANSFER_SRC_BIT).
        Image(
            Context& context,
            vgfx::CommandBufferFactory& commandBufferFactory,
            vgfx::CommandQueue& commandQueue,
            const Config& config,
            const void* pImageData,
            size_t imageDataSize);

        ~Image();

        uint32_t getWidth() const { return m_extent.width; }
        uint32_t getHeight() const { return m_extent.height; }
        uint32_t getDepth() const { return m_extent.depth; }
        VkFormat getFormat() const { return m_format; }
        uint32_t getMipLevels() const { return m_mipLevels; }

        VkImage getHandle() const { return m_image.handle; }
    private:
        Context& m_context;
        VkExtent3D m_extent = {};
        VkFormat m_format = VK_FORMAT_UNDEFINED;
        uint32_t m_mipLevels = 0u;

        MemoryAllocator::Image m_image;
    };
}
#endif
