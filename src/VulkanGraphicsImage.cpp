#include "VulkanGraphicsImage.h"

#include "VulkanGraphicsOneTimeCommands.h"

vgfx::Image::Image(
    Context& context,
    const Config& config)
    : m_context(context)
    , m_extent(config.imageInfo.extent)
    , m_format(config.imageInfo.format)
{
    auto& memoryAllocator = context.getMemoryAllocator();

    VmaMemoryUsage imageMemoryUsage = VmaMemoryUsage::VMA_MEMORY_USAGE_GPU_ONLY;

    m_image = memoryAllocator.createImage(config.imageInfo, imageMemoryUsage);
}

vgfx::Image::Image(
    Context& context,
    vgfx::CommandBufferFactory& commandBufferFactory,
    vgfx::CommandQueue& commandQueue,
    const Config& config,
    const void* pImageData,
    size_t imageDataSize)
    : Image(context, config)
{
    OneTimeCommandsHelper helper(
        context,
        commandBufferFactory,
        commandQueue);

    helper.copyDataToImage(*this, pImageData, imageDataSize);
}

vgfx::Image::~Image()
{
    if (m_image.isValid()) {
        m_context.getMemoryAllocator().destroyImage(m_image);
    }
}
