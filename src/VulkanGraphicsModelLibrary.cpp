#include "VulkanGraphicsModelLibrary.h"

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

template<> struct std::hash<vgfx::VertexXyzRgbUvN> {
    size_t operator()(vgfx::VertexXyzRgbUvN const& vertex) const {
        return ((
            (hash<glm::vec3>()(vertex.pos) ^
            (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
            (hash<glm::vec2>()(vertex.texCoord) << 1) >> 1) ^
            (hash<glm::vec3>()(vertex.normal) << 1);
    }
};

namespace vgfx
{
    IndexBuffer::Config ModelLibrary::DefaultIndexBufferConfig(VK_INDEX_TYPE_UINT32);

    static std::unique_ptr<VertexBuffer> CreateVertexBuffer(
        Context& context,
        CommandBufferFactory& commandBufferFactory,
        CommandQueue& commandQueue,
        const VertexBuffer::Config& config,
        const std::vector<uint8_t>& vertices)
    {
        return std::make_unique<VertexBuffer>(
            context,
            commandBufferFactory,
            commandQueue,
            config,
            vertices.data(),
            vertices.size());
    }

    static VertexXyzRgbUv CreateXyzRgbUv(const tinyobj::attrib_t& attrib, const tinyobj::index_t& index)
    {
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

        return vertex;
    }

    static VertexXyzRgbUvN CreateXyzRgbUvN(const tinyobj::attrib_t& attrib, const tinyobj::index_t& index)
    {
        VertexXyzRgbUvN vertex = {
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
            {
                attrib.normals[3 * index.normal_index + 0],
                attrib.normals[3 * index.normal_index + 1],
                attrib.normals[3 * index.normal_index + 2]
            },
        };

        return vertex;
    }

    template<class VertexType>
    void LoadVertices(
        const tinyobj::attrib_t& attrib,
        const std::vector<tinyobj::shape_t>& shapes,
        const std::function<VertexType(const tinyobj::attrib_t&, const tinyobj::index_t&)>& createVertexFunc,
        const VertexBuffer::Config& vtxBufferCfg,
        Context& context,
        CommandBufferFactory& commandBufferFactory,
        CommandQueue& commandQueue,
        std::unique_ptr<VertexBuffer>* pspVertexBuffer,
        std::unique_ptr<IndexBuffer>* pspIndexBuffer)
    {
        std::vector<uint8_t> vertices;
        std::vector<uint32_t> indices;
        std::unordered_map<VertexType, uint32_t> uniqueVertices;

        for (const auto& shape : shapes) {
            for (const auto& index : shape.mesh.indices) {
                VertexType vertex = createVertexFunc(attrib, index);

                if (uniqueVertices.count(vertex) == 0) {
                    //uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                    uint8_t* pVertex = reinterpret_cast<uint8_t*>(&vertex);
                    vertices.insert(vertices.end(), pVertex, pVertex + sizeof(VertexType));
                }

                //indices.push_back(uniqueVertices[vertex]);
                indices.push_back(static_cast<uint32_t>(indices.size()));
            }
        }

        *pspVertexBuffer =
            CreateVertexBuffer(
                context,
                commandBufferFactory,
                commandQueue,
                vtxBufferCfg,
                vertices);

        *pspIndexBuffer =
            std::make_unique<IndexBuffer>(
                context,
                commandBufferFactory,
                commandQueue,
                // TODO eventually could have a way to use other vertex buffer configs
                ModelLibrary::GetDefaultIndexBufferConfig(),
                indices.data(),
                static_cast<uint32_t>(indices.size()));
    }

    Drawable& ModelLibrary::getOrCreateDrawable(
        Context& context,
        const std::string& model,
        const Material& material,
        DescriptorPool& descriptorPool,
        CommandBufferFactory& commandBufferFactory,
        CommandQueue commandQueue)
    {
        std::string modelPath = context.getAppConfig().dataDirectoryPath + "/" + model;

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

        // This is kind of crappy way to do this.
        if (VertexBuffer::ComputeVertexStride(material.getVertexShaderInputs()) ==
                VertexBuffer::ComputeVertexStride(VertexXyzRgbUv::GetConfig().vertexAttrDescriptions)) {
            LoadVertices<VertexXyzRgbUv>(
                attrib,
                shapes,
                CreateXyzRgbUv,
                VertexXyzRgbUv::GetConfig(),
                context,
                commandBufferFactory,
                commandQueue,
                &spVertexBuffer,
                &spIndexBuffer);
        } else if (VertexBuffer::ComputeVertexStride(material.getVertexShaderInputs()) ==
                   VertexBuffer::ComputeVertexStride(VertexXyzRgbUvN::GetConfig().vertexAttrDescriptions)) {
            LoadVertices<VertexXyzRgbUvN>(
                attrib,
                shapes,
                CreateXyzRgbUvN,
                VertexXyzRgbUvN::GetConfig(),
                context,
                commandBufferFactory,
                commandQueue,
                &spVertexBuffer,
                &spIndexBuffer);
        } else {
            assert(false && "Unkown vertex config.");
        }

        std::map<Material::ImageType, const Image*> images;
        if (!material.getImageTypes().empty()) {
            for (auto imageType : material.getImageTypes()) {
                if (imageType == Material::ImageType::Diffuse) {
                    std::string diffuseTexPath = modelPath.substr(0, modelPath.rfind('.')) + ".png";
                    if (!materials.empty()) {
                        diffuseTexPath = materials.front().diffuse_texname;
                    }
                    Image& image =
                        getOrLoadImage(
                            diffuseTexPath,
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
                descriptorPool,
                material,
                images)));

        return *models.back().get();
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
                if (spDrawable->getMaterialId() == material.getId()) {
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

        VkDeviceSize imageSize = static_cast<int64_t>(texWidth) * static_cast<int64_t>(texHeight) * 4;

        // TODO the image config should not be hard coded.
        Image::Config imageConfig(
            texWidth,
            texHeight,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT
            | VK_IMAGE_USAGE_SAMPLED_BIT
            | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
            | VK_IMAGE_USAGE_STORAGE_BIT,
            Image::ComputeMipLevels2D(texWidth, texHeight));

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
