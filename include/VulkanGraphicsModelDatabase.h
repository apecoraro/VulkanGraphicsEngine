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
        using ImageSamplingConfigs = std::vector<std::pair<ImageView::Config, Sampler::Config>>;
        struct ModelConfig
        {
            MaterialsDatabase::MaterialInfo materialInfo;
            // Pairs of ImageView and Sampler configs corresponding to each image in the model,
            // defines how the image will be sampled. TODO add Sampler reuse.
            ImageSamplingConfigs imageSamplingConfigs;
            ModelConfig(
                const MaterialsDatabase::MaterialInfo& mi,
                const ImageSamplingConfigs& isc)
                : materialInfo(mi)
                , imageSamplingConfigs(isc)
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
            CommandQueue& commandQueue);
 
        // Default vertex and index buffer config for all models/drawables created by this.
        static VertexBuffer::Config VertexBufferConfig;
        static IndexBuffer::Config IndexBufferConfig;

    private:
        Drawable* findDrawable(const std::string& modelPath, const Material& material);

        struct ImageData
        {
            std::unique_ptr<Image> spImage;
            std::unique_ptr<ImageView> spImageView;
            std::unique_ptr<Sampler> spSampler;
        };

        ImageData& getOrLoadImage(
            const std::string& path,
            const ImageView::Config& imageViewConfig,
            const Sampler::Config& samplerConfig,
            Context& context,
            CommandBufferFactory& commandBufferFactory,
            CommandQueue& commandQueue);

        ModelConfigMap m_modelConfigMap;

        using FilePath = std::string;

        using DrawableDatabase = std::unordered_map<FilePath, std::vector<std::unique_ptr<Drawable>>>;
        DrawableDatabase m_drawableDatabase;

        using ImageDatabase = std::unordered_map<FilePath, ImageData>;
        ImageDatabase m_imageDatabase;
    };
}
#endif
