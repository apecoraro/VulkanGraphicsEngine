#pragma once

#include "VulkanGraphicsCommandBufferFactory.h"
#include "VulkanGraphicsContext.h"
#include "VulkanGraphicsDescriptors.h"
#include "VulkanGraphicsDrawable.h"
#include "VulkanGraphicsImage.h"
#include "VulkanGraphicsImageView.h"
#include "VulkanGraphicsIndexBuffer.h"
#include "VulkanGraphicsEffects.h"
#include "VulkanGraphicsSampler.h"
#include "VulkanGraphicsVertexBuffer.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace vgfx
{
    class ModelLibrary
    {
    public:

        ModelLibrary() = default;

        struct ModelDesc
        {
            std::string modelPathOrShapeName;
            using Images = std::unordered_map<MeshEffect::ImageType, std::string>;
            // Use imageOverrides to override the images specified by the model, or provide them for a shape.
            Images imageOverrides;
        };

        Drawable& getOrCreateDrawable(
            Context& context,
            const ModelDesc& model,
            const MeshEffect& meshEffect,
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

        bool getModelData(
            const std::string& modelPathOrShapeName,
            VertexBuffer** ppVertexBuffer,
            IndexBuffer** ppIndexBuffer,
            ModelDesc::Images* pModelImages) const;

        Drawable* findDrawable(const std::string& modelPath, const MeshEffect& meshEffect);

        using FilePath = std::string;

        using DrawableLibrary = std::unordered_map<FilePath, std::map<MeshEffectId, std::unique_ptr<Drawable>>>;
        DrawableLibrary m_drawableLibrary;

        using ImageLibrary = std::unordered_map<FilePath, std::unique_ptr<Image>>;
        ImageLibrary m_imageLibrary;

        using ImageViewLibrary = std::map<ImageView::Config, std::unique_ptr<ImageView>>;
        ImageViewLibrary m_imageViewLibrary;

        using SamplerLibrary = std::map<Sampler::Config, std::unique_ptr<Sampler>>;
        SamplerLibrary m_samplerLibrary;

        struct ModelData
        {
            std::unique_ptr<VertexBuffer> spVertexBuffer;
            std::unique_ptr<IndexBuffer> spIndexBuffer;
            ModelDesc::Images modelImages;
        };
        using ModelDataLibrary = std::unordered_map<std::string, ModelData>;
        ModelDataLibrary m_modelDataLibrary;
    };
}

