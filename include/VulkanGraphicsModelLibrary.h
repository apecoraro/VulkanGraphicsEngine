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
            using Images = std::unordered_map<ImageType, std::string>;
            // Use imagesOverrides to override the imagesOverrides specified by the model, or provide them for a shape.
            Images imagesOverrides;
        };

        Drawable& getOrCreateDrawable(
            Context& context,
            const ModelDesc& model,
            CommandBufferFactory& commandBufferFactory);
 
        // Default index buffer config for all models/drawables created by this.
        static IndexBuffer::Config& GetDefaultIndexBufferConfig();

        Image& getOrLoadImage(
            const std::string& path,
            Context& context,
            CommandBufferFactory& commandBufferFactory);

        ImageView& getOrCreateImageView(
            const ImageView::Config& config,
            Context& context,
            const Image& image);

    private:
        static VertexBuffer::Config DefaultVertexBufferConfig;
        static IndexBuffer::Config DefaultIndexBufferConfig;

        bool getModelData(
            const std::string& modelPathOrShapeName,
            VertexBuffer** ppVertexBuffer,
            IndexBuffer** ppIndexBuffer,
            ModelDesc::Images* pModelImages) const;

        Drawable* findDrawable(const std::string& modelPath);

        using FilePath = std::string;

        using DrawableLibrary = std::unordered_map<FilePath, std::unique_ptr<Drawable>>;
        DrawableLibrary m_drawableLibrary;

        using ImageLibrary = std::unordered_map<FilePath, std::unique_ptr<Image>>;
        ImageLibrary m_imageLibrary;

        using ImageViewLibrary = std::map<ImageView::Config, std::unique_ptr<ImageView>>;
        ImageViewLibrary m_imageViewLibrary;

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

