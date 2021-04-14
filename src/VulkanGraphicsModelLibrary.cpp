#include "VulkanGraphicsModelLibrary.h"

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
    VertexBuffer::Config ModelLibrary::DefaultVertexBufferConfig(
            sizeof(VertexXyzRgbUv),
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST); // TODO do all Obj models use this?

    IndexBuffer::Config ModelLibrary::DefaultIndexBufferConfig(VK_INDEX_TYPE_UINT32);

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
                ModelLibrary::GetDefaultVertexBufferConfig(),
                vertices);

        std::unique_ptr<IndexBuffer> spIndexBuffer =
            std::make_unique<IndexBuffer>(
                context,
                commandBufferFactory,
                commandQueue,
                ModelLibrary::GetDefaultIndexBufferConfig(),
                indices.data(),
                static_cast<uint32_t>(indices.size()));
    }

    Drawable& ModelLibrary::getOrCreateDrawable(
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
        Material& material = MaterialsLibrary::GetOrLoadMaterial(context, materialInfo);
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

        std::map<Material::ImageType, const Image*> images;
        if (!material.getImageTypes().empty()) {

            for (auto imageType : material.getImageTypes()) {
                if (imageType == Material::ImageType::DIFFUSE) {
                    Image& image =
                        getOrLoadImage(
                            materials.front().diffuse_texname,
                            context,
                            commandBufferFactory,
                            commandQueue);
                    images[imageType] = &image;
                }
            }
        }
        auto& models = m_drawableLibrary[modelPath];
        models.push_back(std::move(
            std::make_unique<Drawable>(
                context,
                std::move(spVertexBuffer),
                std::move(spIndexBuffer),
                material,
                images)));

        return *models.back().get();
    }

    VertexBuffer::Config& ModelLibrary::GetDefaultVertexBufferConfig()
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

    IndexBuffer::Config& ModelLibrary::GetDefaultIndexBufferConfig()
    {
        return DefaultIndexBufferConfig;
    }

    Drawable* ModelLibrary::findDrawable(const std::string& modelPath, const Material& material)
    {
        const auto& findIt = m_drawableLibrary.find(modelPath);
        if (findIt != m_drawableLibrary.end()) {
            for (const auto& spDrawable : findIt->second) {
                if (spDrawable->getMaterial().getId() == material.getId()) {
                    return spDrawable.get();
                }
            }
        }
        return nullptr;
    }

    Image& ModelLibrary::getOrLoadImage(
        const std::string& path,
        Context& context,
        CommandBufferFactory& commandBufferFactory,
        CommandQueue& commandQueue)
    {
        auto& spImage = m_imageLibrary[path];
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

    ImageView& ModelLibrary::getOrCreateImageView(const ImageView::Config& config, Context& context, const vgfx::Image& image)
    {
        auto& spImageView = m_imageViewLibrary[config];
        if (spImageView != nullptr) {
            return *spImageView.get();
        }

        spImageView = std::make_unique<ImageView>(context, config, image);

        return *spImageView.get();
    }

    Sampler& ModelLibrary::getOrCreateSampler(const Sampler::Config& config, Context& context)
    {
        auto& spSampler = m_samplerLibrary[config];
        if (spSampler != nullptr) {
            return *spSampler.get();
        }

        spSampler = std::make_unique<Sampler>(context, config);

        return *spSampler.get();
    }
}
