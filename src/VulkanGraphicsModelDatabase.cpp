#include "VulkanGraphicsModelDatabase.h"

#include "VulkanGraphicsDescriptors.h"
#include "VulkanGraphicsImageView.h"
#include "VulkanGraphicsIndexBuffer.h"
#include "VulkanGraphicsUniformBuffer.h"
#include "VulkanGraphicsVertexBuffer.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <unordered_map>

template<> struct std::hash<vgfx::VertexXyzRgbUv> {
    size_t operator()(vgfx::VertexXyzRgbUv const& vertex) const {
        return (
            (hash<glm::vec3>()(vertex.pos) ^
            (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
            (hash<glm::vec2>()(vertex.texCoord) << 1);
    }
};

namespace vgfx
{
    VertexBuffer::Config ModelDatabase::DefaultVertexBufferConfig(
            sizeof(VertexXyzRgbUv),
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST); // TODO do all Obj models use this?

    IndexBuffer::Config ModelDatabase::DefaultIndexBufferConfig(VK_INDEX_TYPE_UINT32);

    static std::unique_ptr<VertexBuffer> CreateVertexBuffer(
        Context& context,
        CommandBufferFactory& commandBufferFactory,
        CommandQueue& commandQueue,
        const VertexBuffer::Config& config,
        const std::vector<VertexXyzRgbUv>& vertices)
    {
        return std::make_unique<VertexBuffer>(
            context,
            commandBufferFactory,
            commandQueue,
            config,
            vertices.data(),
            vertices.size() * sizeof(VertexXyzRgbUv));
    }

    static void LoadVertices(
        const tinyobj::attrib_t& attrib,
        const std::vector<tinyobj::shape_t>& shapes,
        Context& context,
        CommandBufferFactory& commandBufferFactory,
        CommandQueue& commandQueue,
        std::unique_ptr<VertexBuffer>* pspVertexBuffer,
        std::unique_ptr<IndexBuffer>* pspIndexBuffer,
        std::string* pDiffuseTexturePath)
    {
        std::vector<VertexXyzRgbUv> vertices;
        std::vector<uint32_t> indices;
        std::unordered_map<VertexXyzRgbUv, uint32_t> uniqueVertices;

        for (const auto& shape : shapes) {
            for (const auto& index : shape.mesh.indices) {
                VertexXyzRgbUv vertex = {
                    {
                        attrib.vertices[3 * index.vertex_index + 0],
                        attrib.vertices[3 * index.vertex_index + 1],
                        attrib.vertices[3 * index.vertex_index + 2]
                    },
                    {
                        1.0f, 1.0f, 1.0f
                    },
                    {
                        attrib.texcoords[2 * index.texcoord_index + 0],
                        // tinyobjloader uses bottom of image as origin, so switch it to top for our case.
                        1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
                    },
                };

                if (uniqueVertices.count(vertex) == 0) {
                    uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                    vertices.push_back(vertex);
                }

                indices.push_back(static_cast<uint32_t>(uniqueVertices.size()));
            }
        }

        std::unique_ptr<VertexBuffer> spVertexBuffer =
            CreateVertexBuffer(
                context,
                commandBufferFactory,
                commandQueue,
                ModelDatabase::GetDefaultVertexBufferConfig(),
                vertices);

        std::unique_ptr<IndexBuffer> spIndexBuffer =
            std::make_unique<IndexBuffer>(
                context,
                commandBufferFactory,
                commandQueue,
                ModelDatabase::GetDefaultIndexBufferConfig(),
                indices.data(),
                static_cast<uint32_t>(indices.size()));
    }

    std::unique_ptr<UniformBufferDescriptor>
    BuildUniformBufferDescriptorSet(
        UniformBuffer& uniformBuffer,
        VkShaderStageFlags shaderStageFlags)
    {
        Descriptor::LayoutBindingConfig bindingConfig(shaderStageFlags);

        return std::make_unique<UniformBufferDescriptor>(uniformBuffer, bindingConfig);
    }

    Drawable& ModelDatabase::getOrCreateDrawable(
        Context& context,
        const std::string& modelPath,
        uint32_t swapChainImageCount,
        CommandBufferFactory& commandBufferFactory,
        CommandQueue commandQueue)
    {
        ModelConfigMap::iterator modelCfgItr = m_modelConfigMap.find(modelPath);
        if (modelCfgItr == m_modelConfigMap.end()) {
            throw std::runtime_error("Unknown model path!");
        }

        auto& materialInfo = modelCfgItr->second.materialInfo;
        Material& material = MaterialsDatabase::GetOrLoadMaterial(context, materialInfo);
        Drawable* pDrawable = findDrawable(modelPath, material);
        if (pDrawable != nullptr) {
            return *pDrawable;
        }

        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn, err;

        if (!tinyobj::LoadObj(
                &attrib, &shapes, &materials, &warn, &err,
                modelPath.c_str())) {
            throw std::runtime_error(warn + err);
        }

        std::unique_ptr<VertexBuffer> spVertexBuffer;
        std::unique_ptr<IndexBuffer> spIndexBuffer;
        std::string diffuseTexturePath;
        LoadVertices(
            attrib,
            shapes,
            context,
            commandBufferFactory,
            commandQueue,
            &spVertexBuffer,
            &spIndexBuffer,
            &diffuseTexturePath);

        // If this material is new then it's descriptor sets need to created and attached.
        if (material.getDescriptorSetCount() == 0) {
            std::vector<std::unique_ptr<Descriptor>> imageSamplerDescriptors;
            std::vector<std::unique_ptr<Descriptor>> uboDescriptors;

            for (size_t i = 0; i < materialInfo.descriptorBindings.size(); ++i) {
                const auto& pDescriptorBinding = materialInfo.descriptorBindings[i];
                if (pDescriptorBinding->m_spDescriptor == nullptr
                    && pDescriptorBinding->getDescriptorType() == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
                    const MaterialsDatabase::CombinedImageSamplerDescriptorBinding*
                        pImageSamplerDescBinding =
                            static_cast<const MaterialsDatabase::CombinedImageSamplerDescriptorBinding*>(pDescriptorBinding);
                    assert(pImageSamplerDescBinding->imageType == MaterialsDatabase::ImageType::DIFFUSE);
                    // Currently only support diffuse.
                    Image& image =
                        getOrLoadImage(
                            materials.front().diffuse_texname.c_str(),
                            context,
                            commandBufferFactory,
                            commandQueue);

                    ImageView& imageView =
                        getOrCreateImageView(pImageSamplerDescBinding->imageViewConfig.value(), context, image);

                    Sampler& sampler =
                        getOrCreateSampler(pImageSamplerDescBinding->samplerConfig.value(), context);

                    CombinedImageSampler imageSampler(imageView, sampler);
                    imageSamplerDescriptors.push_back(
                        std::move(std::make_unique<CombinedImageSamplerDescriptor>(
                            imageSampler, pImageSamplerDescBinding->descriptorConfig)));
                } else if (pDescriptorBinding->descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
                    const MaterialsDatabase::UniformBufferDescriptorBinding* pUniformBufferDescBinding =
                            static_cast<const MaterialsDatabase::UniformBufferDescriptorBinding*>(pDescriptorBinding);

                    uboDescriptors.push_back(
                        std::move(
                            BuildUniformBufferDescriptorSet(
                                pUniformBufferDescBinding->uniformBuffer,
                                pUniformBufferDescBinding->shaderStageFlags)));
                }
            }

            std::vector<std::unique_ptr<DescriptorSetBuffer>> descriptorSets;
            descriptorSets.reserve(imageSamplerDescriptors.size() + uboDescriptors.size());

            if (!imageSamplerDescriptors.empty()) {
                std::unique_ptr<DescriptorSetBuffer> spDescriptorSetBuffer =
                    std::make_unique<DescriptorSetBuffer>(
                        context,
                        std::move(imageSamplerDescriptors));
                descriptorSets.push_back(std::move(spDescriptorSetBuffer));
            }

            if (!uboDescriptors.size()) {
                std::unique_ptr<DescriptorSetBuffer> spDescriptorSetBuffer =
                    std::make_unique<DescriptorSetBuffer>(
                        context,
                        std::move(uboDescriptors),
                        swapChainImageCount);
                descriptorSets.push_back(std::move(spDescriptorSetBuffer));
            }

            material.attachDescriptorSets(std::move(descriptorSets));
        }

        auto& models = m_drawableDatabase[modelPath];
        models.push_back(std::move(
            std::make_unique<Drawable>(
                context,
                std::move(spVertexBuffer),
                std::move(spIndexBuffer),
                material)));

        return *models.back().get();
    }

    VertexBuffer::Config& ModelDatabase::GetDefaultVertexBufferConfig()
    {
        // Initialize the vertex attribute descriptions if not already.
        if (DefaultVertexBufferConfig.vertexAttrDescriptions.empty()) {
            DefaultVertexBufferConfig.vertexAttrDescriptions.push_back(
                VertexBuffer::AttributeDescription(
                    VK_FORMAT_R32G32B32_SFLOAT,
                    offsetof(VertexXyzRgbUv, pos)));

            DefaultVertexBufferConfig.vertexAttrDescriptions.push_back(
                VertexBuffer::AttributeDescription(
                    VK_FORMAT_R32G32B32_SFLOAT,
                    offsetof(VertexXyzRgbUv, color)));

            DefaultVertexBufferConfig.vertexAttrDescriptions.push_back(
                VertexBuffer::AttributeDescription(
                    VK_FORMAT_R32G32_SFLOAT,
                    offsetof(VertexXyzRgbUv, texCoord)));
        }

        return DefaultVertexBufferConfig;
    }

    IndexBuffer::Config& ModelDatabase::GetDefaultIndexBufferConfig()
    {
        return DefaultIndexBufferConfig;
    }

    Drawable* ModelDatabase::findDrawable(const std::string& modelPath, const Material& material)
    {
        const auto& findIt = m_drawableDatabase.find(modelPath);
        if (findIt != m_drawableDatabase.end()) {
            for (const auto& spDrawable : findIt->second) {
                if (spDrawable->getMaterial().getId() == material.getId()) {
                    return spDrawable.get();
                }
            }
        }
        return nullptr;
    }

    Image& ModelDatabase::getOrLoadImage(
        const std::string& path,
        Context& context,
        CommandBufferFactory& commandBufferFactory,
        CommandQueue& commandQueue)
    {
        auto& spImage = m_imageDatabase[path];
        if (spImage != nullptr) {
            return *spImage.get();
        }

        int32_t texWidth = 0;
        int32_t texHeight = 0;
        int32_t texChannels = 0;
        stbi_uc* pPixels = stbi_load(
            path.c_str(),
            &texWidth, &texHeight, &texChannels,
            STBI_rgb_alpha);

        VkDeviceSize imageSize = texWidth * texHeight * 4;

        Image::Config imageConfig(
            texWidth,
            texHeight,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

        spImage =
            std::make_unique<Image>(
                context,
                commandBufferFactory,
                commandQueue,
                imageConfig,
                pPixels,
                imageSize);

        stbi_image_free(pPixels);

        return *spImage.get();
    }

    ImageView& ModelDatabase::getOrCreateImageView(const ImageView::Config& config, Context& context, const vgfx::Image& image)
    {
        auto& spImageView = m_imageViewDatabase[config];
        if (spImageView != nullptr) {
            return *spImageView.get();
        }

        spImageView = std::make_unique<ImageView>(context, config, image);

        return *spImageView.get();
    }

    Sampler& ModelDatabase::getOrCreateSampler(const Sampler::Config& config, Context& context)
    {
        auto& spSampler = m_samplerDatabase[config];
        if (spSampler != nullptr) {
            return *spSampler.get();
        }

        spSampler = std::make_unique<Sampler>(context, config);

        return *spSampler.get();
    }
}
