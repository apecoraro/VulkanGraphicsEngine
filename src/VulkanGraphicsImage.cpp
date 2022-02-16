#include "VulkanGraphicsImage.h"

#include "VulkanGraphicsImageDownsampler.h"
#include "VulkanGraphicsOneTimeCommands.h"

#include <algorithm>
#include <cassert>
#include <cmath>

uint32_t vgfx::Image::ComputeMipLevels2D(uint32_t width, uint32_t height)
{
    return static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;
}

vgfx::Image::Image(
    Context& context,
    const Config& config)
    : m_context(context)
    , m_extent(config.imageInfo.extent)
    , m_format(config.imageInfo.format)
    , m_mipLevels(config.imageInfo.mipLevels)
    , m_sampleCount(config.imageInfo.samples)
{
    auto& memoryAllocator = context.getMemoryAllocator();

    VmaMemoryUsage imageMemoryUsage = VmaMemoryUsage::VMA_MEMORY_USAGE_GPU_ONLY;

    m_image = memoryAllocator.createImage(config.imageInfo, imageMemoryUsage);
}

vgfx::Image::Image(
    Context& context,
    CommandBufferFactory& commandBufferFactory,
    CommandQueue& commandQueue,
    const Config& config,
    const void* pImageData,
    size_t imageDataSize)
    : Image(context, config)
{

    if (config.imageInfo.mipLevels > 1u) {
        // make sure we can read from the image so that we can generate the mip levels.
        assert(config.imageInfo.usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    }

    OneTimeCommandsHelper helper(
        context,
        commandBufferFactory,
        commandQueue);

    OneTimeCommandsHelper::GenerateMips genMips =
        context.areShaderSubgroupsSupported() && context.isDescriptorIndexingSupported() ?
            OneTimeCommandsHelper::GenerateMips::No : OneTimeCommandsHelper::GenerateMips::Yes;
    helper.copyDataToImage(*this, pImageData, imageDataSize, genMips);
    if (genMips == OneTimeCommandsHelper::GenerateMips::No) {
        ImageDownsampler& downsampler = context.getOrCreateImageDownsampler();
        downsampler.execute(*this, helper);
    }
}

vgfx::Image::~Image()
{
    m_imageViews.clear();

    if (m_image.isValid()) {
        m_context.getMemoryAllocator().destroyImage(m_image);
    }
}

vgfx::ImageView& vgfx::Image::getOrCreateView(const ImageView::Config& config) const
{
    const auto& findIt = m_imageViews.find(config);
    if (findIt == m_imageViews.end()) {
        return *(m_imageViews[config] = std::move(std::make_unique<ImageView>(m_context, config, *this))).get();
    }
    return *findIt->second.get();
}
