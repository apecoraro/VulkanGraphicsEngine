#ifndef VGFX_MODEL_LIBRARY_H
#define VGFX_MODEL_LIBRARY_H

#include "VulkanGraphicsCommandBuffers.h"
#include "VulkanGraphicsContext.h"
#include "VulkanGraphicsDescriptors.h"
#include "VulkanGraphicsDrawable.h"
#include "VulkanGraphicsImage.h"
#include "VulkanGraphicsImageView.h"
#include "VulkanGraphicsIndexBuffer.h"
#include "VulkanGraphicsMaterials.h"
#include "VulkanGraphicsSampler.h"
#include "VulkanGraphicsVertexBuffer.h"

#include <unordered_map>
#include <memory>
#include <string>

namespace vgfx
{
    class ModelLibrary
    {
    public:
        using ModelPath = std::string;

        ModelLibrary() = default;

        Drawable& getOrCreateDrawable(
            Context& context,
            const std::string& modelPath,
            const Material& material,
            DescriptorPool& descriptorPool,
            CommandBufferFactory& commandBufferFactory,
            CommandQueue commandQueue);
 
        // Default index buffer config for all models/drawables created by this.
        static IndexBuffer::Config& GetDefaultIndexBufferConfig();

        Image& getOrLoadImage(
            const std::string& path,
            Context& context,
            CommandBufferFactory& commandBufferFactory,
            CommandQueue& commandQueue);

        ImageView& getOrCreateImageView(
            const ImageView::Config& config,
            Context& context,
            const Image& image);

        Sampler& getOrCreateSampler(
            const Sampler::Config& config,
            Context& context);

    private:
        static VertexBuffer::Config DefaultVertexBufferConfig;
        static IndexBuffer::Config DefaultIndexBufferConfig;

        Drawable* findDrawable(const std::string& modelPath, const Material& material);

        using FilePath = std::string;

        using DrawableLibrary = std::unordered_map<FilePath, std::vector<std::unique_ptr<Drawable>>>;
        DrawableLibrary m_drawableLibrary;

        using ImageLibrary = std::unordered_map<FilePath, std::unique_ptr<Image>>;
        ImageLibrary m_imageLibrary;

        using ImageViewLibrary = std::map<ImageView::Config, std::unique_ptr<ImageView>>;
        ImageViewLibrary m_imageViewLibrary;

        using SamplerLibrary = std::map<Sampler::Config, std::unique_ptr<Sampler>>;
        SamplerLibrary m_samplerLibrary;
    };
}
#endif
