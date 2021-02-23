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

template<> struct std::hash<vgfx::Vertex> {
    size_t operator()(vgfx::Vertex const& vertex) const {
        return (
            (hash<glm::vec3>()(vertex.pos) ^
            (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
            (hash<glm::vec2>()(vertex.texCoord) << 1);
    }
};

namespace vgfx
{
    static std::unique_ptr<VertexBuffer> CreateVertexBuffer(
        Context& context,
        CommandBufferFactory& commandBufferFactory,
        CommandQueue& commandQueue,
        VertexBuffer::Config& config,
        const std::vector<Vertex>& vertices)
    {
        config.vertexAttrDescriptions.push_back(
            VertexBuffer::AttributeDescription(
                VK_FORMAT_R32G32_SFLOAT,
                offsetof(Vertex, pos)));

        config.vertexAttrDescriptions.push_back(
            VertexBuffer::AttributeDescription(
                VK_FORMAT_R32G32B32_SFLOAT,
                offsetof(Vertex, color)));

        config.vertexAttrDescriptions.push_back(
            VertexBuffer::AttributeDescription(
                VK_FORMAT_R32G32_SFLOAT,
                offsetof(Vertex, texCoord)));

        return std::make_unique<VertexBuffer>(
            context,
            commandBufferFactory,
            commandQueue,
            config,
            vertices.data(),
            vertices.size() * sizeof(Vertex));
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
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        std::unordered_map<Vertex, uint32_t> uniqueVertices;

        for (const auto& shape : shapes) {
            for (const auto& index : shape.mesh.indices) {
                Vertex vertex = {
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

                vertices.push_back(vertex);
                if (uniqueVertices.count(vertex) == 0) {
                    uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                    vertices.push_back(vertex);
                }

                indices.push_back(static_cast<uint32_t>(uniqueVertices.size()));
            }
        }

        VertexBuffer::Config vertexBufferConfig(
            sizeof(Vertex),
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

        std::unique_ptr<VertexBuffer> spVertexBuffer =
            CreateVertexBuffer(
                context,
                commandBufferFactory,
                commandQueue,
                vertexBufferConfig,
                vertices);

        IndexBuffer::Config indexBufferConfig(VK_INDEX_TYPE_UINT32);
        std::unique_ptr<IndexBuffer> spIndexBuffer =
            std::make_unique<IndexBuffer>(
                context,
                commandBufferFactory,
                commandQueue,
                indexBufferConfig,
                indices.data(),
                static_cast<uint32_t>(indices.size()));
    }

    void BuildImageSamplerDescriptorSet(
        std::unique_ptr<CombinedImageSampler> spCombinedImageSampler,
        VkShaderStageFlags shaderStageFlags,
        std::vector<std::unique_ptr<Descriptor>>* pBindings)
    {
        Descriptor::LayoutBindingConfig bindingConfig(shaderStageFlags);

        std::unique_ptr<CombinedImageSamplerDescriptor> spCombinedImageSamplerDescriptor =
            std::make_unique<CombinedImageSamplerDescriptor>(
                std::move(spCombinedImageSampler),
                bindingConfig);

        pBindings->push_back(std::move(spCombinedImageSamplerDescriptor));
    }

    void BuildUniformBufferDescriptorSet(
        std::unique_ptr<UniformBuffer> spUniformBuffer,
        VkShaderStageFlags shaderStageFlags,
        std::vector<std::unique_ptr<Descriptor>>* pBindings)
    {
        Descriptor::LayoutBindingConfig bindingConfig(shaderStageFlags);

        std::unique_ptr<UniformBufferDescriptor> spUniformBufferDescriptor =
            std::make_unique<UniformBufferDescriptor>(
                std::move(spUniformBuffer),
                bindingConfig);

        pBindings->push_back(std::move(spUniformBufferDescriptor));
    }

    Drawable& ModelDatabase::getOrCreateDrawable(
        Context& context,
        const std::string& modelPath,
        uint32_t swapChainImageCount,
        CommandBufferFactory& commandBufferFactory,
        CommandQueue& commandQueue)
    {
        ModelConfigMap::iterator modelCfgItr = m_modelConfigMap.find(modelPath);
        if (modelCfgItr == m_modelConfigMap.end()) {
            throw std::runtime_error("Unknown model path!");
        }

        MaterialsDatabase::MaterialInfo materialInfo = modelCfgItr->second.materialInfo;
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

            ImageSamplingConfigs::iterator imageSamplingCfgItr = modelCfgItr->second.imageSamplingConfigs.begin();
            for (size_t i = 0; i < materialInfo.descriptorBindings.size(); ++i) {
                const auto& descriptorBindingInfo = materialInfo.descriptorBindings[i];
                if (descriptorBindingInfo.first == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
                    if (imageSamplingCfgItr == modelCfgItr->second.imageSamplingConfigs.end()) {
                        assert(false && "Not enough sampling configs to match up with the material descriptor bindings!");
                    }

                    ImageData& imageData =
                        getOrLoadImage(
                            materials.front().diffuse_texname.c_str(),
                            imageSamplingCfgItr->first, // ImageView::Config
                            imageSamplingCfgItr->second, // Sampler::Config
                            context,
                            commandBufferFactory,
                            commandQueue);

                    ++imageSamplingCfgItr;

                    BuildImageSamplerDescriptorSet(
                        std::make_unique<CombinedImageSampler>(
                            *imageData.spImageView.get(),
                            *imageData.spSampler.get()),
                        descriptorBindingInfo.second,
                        &imageSamplerDescriptors);

                } else if (descriptorBindingInfo.first == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
                    UniformBuffer::Config ubConfig(swapChainImageCount);
                    std::unique_ptr<UniformBuffer> spUniformBuffer = std::make_unique<UniformBuffer>(context, ubConfig);

                    BuildUniformBufferDescriptorSet(
                        std::move(spUniformBuffer),
                        descriptorBindingInfo.second,
                        &uboDescriptors);
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
    }

    Drawable* ModelDatabase::findDrawable(const std::string& modelPath, const Material& material)
    {
        const auto& findIt = m_drawableDatabase.find(modelPath);
        if (findIt == m_drawableDatabase.end()) {
            for (const auto& spDrawable : findIt->second) {
                if (spDrawable->getMaterial().getId() == material.getId()) {
                    return spDrawable.get();
                }
            }
        }
        return nullptr;
    }

    ModelDatabase::ImageData& ModelDatabase::getOrLoadImage(
        const std::string& path,
        const ImageView::Config& imageViewConfig,
        const Sampler::Config& samplerConfig,
        Context& context,
        CommandBufferFactory& commandBufferFactory,
        CommandQueue& commandQueue)
    {
        ImageData& imageData = m_imageDatabase[path];
        if (imageData.spImage != nullptr) {
            assert(imageData.spImageView != nullptr && imageData.spSampler != nullptr);
            return imageData;
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

        imageData.spImage =
            std::make_unique<Image>(
                context,
                commandBufferFactory,
                commandQueue,
                imageConfig,
                pPixels,
                imageSize);

        stbi_image_free(pPixels);

        imageData.spImageView =
            std::make_unique<ImageView>(context, imageViewConfig, *imageData.spImage.get());

        imageData.spSampler =
            std::make_unique<Sampler>(context, samplerConfig);

        return imageData;
    }
}
