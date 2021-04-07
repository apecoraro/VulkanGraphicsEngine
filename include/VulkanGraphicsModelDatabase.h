#ifndef VGFX_MODEL_DATABASE_H
#define VGFX_MODEL_DATABASE_H

#include "VulkanGraphicsCommandBuffers.h"
#include "VulkanGraphicsContext.h"
#include "VulkanGraphicsDrawable.h"
#include "VulkanGraphicsImage.h"
#include "VulkanGraphicsMaterials.h"
#include "VulkanGraphicsSampler.h"

#include <unordered_map>
#include <memory>
#include <string>

namespace vgfx
{
    class ModelDatabase
    {
    public:
        struct ModelConfig
        {
            MaterialsDatabase::MaterialInfo materialInfo;
            ModelConfig(
                const MaterialsDatabase::MaterialInfo& mi)
                : materialInfo(mi)
            {
            }
        };

        using ModelPath = std::string;
        using ModelConfigMap = std::unordered_map<ModelPath, ModelConfig>;
        struct Config
        {
            ModelConfigMap modelConfigMap;
        };

        ModelDatabase(Config& config) : m_modelConfigMap(config.modelConfigMap) {}

        Drawable& getOrCreateDrawable(
            Context& context,
            const std::string& modelPath,
            uint32_t swapChainImageCount,
            CommandBufferFactory& commandBufferFactory,
            CommandQueue commandQueue);
 
        // Default vertex and index buffer config for all models/drawables created by this.
        static VertexBuffer::Config& GetDefaultVertexBufferConfig();
        static IndexBuffer::Config& GetDefaultIndexBufferConfig();

    private:
        static VertexBuffer::Config DefaultVertexBufferConfig;
        static IndexBuffer::Config DefaultIndexBufferConfig;

        Drawable* findDrawable(const std::string& modelPath, const Material& material);

        vgfx::Image& getOrLoadImage(
            const std::string& path,
            Context& context,
            CommandBufferFactory& commandBufferFactory,
            CommandQueue& commandQueue);

        ImageView& getOrCreateImageView(
            const ImageView::Config& config,
            Context& context,
            const vgfx::Image& image);

        Sampler& getOrCreateSampler(
            const Sampler::Config& config,
            Context& context);

        ModelConfigMap m_modelConfigMap;

        using FilePath = std::string;

        using DrawableDatabase = std::unordered_map<FilePath, std::vector<std::unique_ptr<Drawable>>>;
        DrawableDatabase m_drawableDatabase;

        using ImageDatabase = std::unordered_map<FilePath, std::unique_ptr<Image>>;
        ImageDatabase m_imageDatabase;

        using ImageViewDatabase = std::map<ImageView::Config, std::unique_ptr<ImageView>>;
        ImageViewDatabase m_imageViewDatabase;

        using SamplerDatabase = std::map<Sampler::Config, std::unique_ptr<Sampler>>;
        SamplerDatabase m_samplerDatabase;
    };
}
#endif
