#ifndef VGFX_IMAGE_H
#define VGFX_IMAGE_H

#include "VulkanGraphicsContext.h"

#include <cassert>
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
                uint32_t mipLevels = 1u,
                VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT)
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
                imageInfo.samples = samples;
                imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            }

            VkImageCreateInfo imageInfo = {};
        };
        Image(Context& context, const Config& config);

        // Creates an image and, if it is configured to have more than one mip map level,
        // then generates the mip map levels (REQUIRES VK_IMAGE_USAGE_TRANSFER_SRC_BIT).
        Image(
            Context& context,
            CommandBufferFactory& commandBufferFactory,
            CommandQueue& commandQueue,
            const Config& config,
            const void* pImageData,
            size_t imageDataSize);

        // Use this to store the metadata for swap chain images and allow them to
        // be used like other Images, but since they are created by the Windowing
        // system we don't want them to be free'd.
        Image(
            Context& context,
            VkImage imageHandle,
            const Config& config)
            : m_context(context)
            , m_extent(config.imageInfo.extent)
            , m_format(config.imageInfo.format)
            , m_mipLevels(config.imageInfo.mipLevels)
            , m_sampleCount(config.imageInfo.samples)
            , m_handle(imageHandle)
        {
        }

        // This copy constructor only valid for copying swap chain image wrappers.
        Image(const Image& copy)
            : m_context(copy.m_context)
            , m_extent(copy.m_extent)
            , m_format(copy.m_format)
            , m_mipLevels(copy.m_mipLevels)
            , m_sampleCount(copy.m_sampleCount)
            , m_handle(copy.getHandle())
        {
        }

        ~Image();

        uint32_t getWidth() const { return m_extent.width; }
        uint32_t getHeight() const { return m_extent.height; }
        uint32_t getDepth() const { return m_extent.depth; }
        VkFormat getFormat() const { return m_format; }
        uint32_t getMipLevels() const { return m_mipLevels; }
        VkSampleCountFlagBits getSampleCount() const { return m_sampleCount; }

        VkImage getHandle() const { return m_handle != VK_NULL_HANDLE ? m_handle : m_image.handle; }
    private:
        Context& m_context;
        VkExtent3D m_extent = {};
        VkFormat m_format = VK_FORMAT_UNDEFINED;
        uint32_t m_mipLevels = 0u;
        VkSampleCountFlagBits m_sampleCount = VK_SAMPLE_COUNT_1_BIT;

        MemoryAllocator::Image m_image;
        VkImage m_handle = VK_NULL_HANDLE;
    };
}
#endif
