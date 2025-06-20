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
    void CreateVertsFromShapes(
        const tinyobj::attrib_t& attrib,
        const std::vector<tinyobj::shape_t>& shapes,
        const std::function<VertexType(const tinyobj::attrib_t&, const tinyobj::index_t&)>& createVertexFunc,
        std::vector<uint8_t>* pVerticesOut,
        std::vector<uint32_t>* pIndicesOut)
    {
        std::vector<uint8_t>& vertices = *pVerticesOut;
        std::vector<uint32_t>& indices = *pIndicesOut;
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
    }

    void CreateVertexBuffers(
        const std::vector<uint8_t>& vertices,
        const std::vector<uint32_t>& indices,
        const VertexBuffer::Config& vtxBufferCfg,
        Context& context,
        CommandBufferFactory& commandBufferFactory,
        CommandQueue& commandQueue,
        std::unique_ptr<VertexBuffer>* pspVertexBuffer,
        std::unique_ptr<IndexBuffer>* pspIndexBuffer)
    {
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
                // TODO eventually could have a way to use other index buffer configs
                ModelLibrary::GetDefaultIndexBufferConfig(),
                indices.data(),
                static_cast<uint32_t>(indices.size()));
    }

    enum class ShapeType
    {
        NONE,
        SPHERE
    };

    static bool ModelIsShape(const std::string& modelName, ShapeType* pShapeType)
    {
        static const std::string shapePrefix = "SHAPE_";
        if (modelName.find(shapePrefix) == 0) {
            std::string shapeTypeName = modelName.substr(shapePrefix.size());
            if (shapeTypeName == "SPHERE") {
                *pShapeType = ShapeType::SPHERE;
            }
            return true;
        }
        return false;
    }

    static VertexBuffer::Config CreateSphereModel(
        std::vector<uint8_t>* pVerticesOut,
        std::vector<uint32_t>* pIndicesOut)
    {
        std::vector<glm::vec3> positions;
        std::vector<glm::vec2> uv;
        std::vector<glm::vec3> normals;

        const unsigned int X_SEGMENTS = 64;
        const unsigned int Y_SEGMENTS = 64;
        const float PI = 3.14159265359f;
        for (unsigned int x = 0; x <= X_SEGMENTS; ++x)
        {
            for (unsigned int y = 0; y <= Y_SEGMENTS; ++y)
            {
                float xSegment = (float)x / (float)X_SEGMENTS;
                float ySegment = (float)y / (float)Y_SEGMENTS;
                float xPos = std::cos(xSegment * 2.0f * PI) * std::sin(ySegment * PI);
                float yPos = std::cos(ySegment * PI);
                float zPos = std::sin(xSegment * 2.0f * PI) * std::sin(ySegment * PI);

                positions.push_back(glm::vec3(xPos, yPos, zPos));
                uv.push_back(glm::vec2(xSegment, ySegment));
                normals.push_back(glm::vec3(xPos, yPos, zPos));
            }
        }

        std::vector<uint32_t>& indices = *pIndicesOut;
        bool oddRow = false;
        for (unsigned int y = 0; y < Y_SEGMENTS; ++y) {
            if (!oddRow) { // even rows: y == 0, y == 2; and so on
                for (unsigned int x = 0; x <= X_SEGMENTS; ++x) {
                    indices.push_back(y * (X_SEGMENTS + 1) + x);
                    indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
                }
            } else {
                for (int x = X_SEGMENTS; x >= 0; --x) {
                    indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
                    indices.push_back(y * (X_SEGMENTS + 1) + x);
                }
            }
            oddRow = !oddRow;
        }
 
        for (unsigned int i = 0; i < positions.size(); ++i) {
            VertexXyzRgbUvN vertex = {
                {
                    positions[i].x,
                    positions[i].y,
                    positions[i].z
                },
                {
                    1.0f, 1.0f, 1.0f
                },
                {
                    uv[i].x,
                    uv[i].y
                },
                {
                    normals[i].x,
                    normals[i].y,
                    normals[i].z
                },
            };
            uint8_t* pVertex = reinterpret_cast<uint8_t*>(&vertex);
            pVerticesOut->insert(pVerticesOut->end(), pVertex, pVertex + sizeof(vertex));
        }
        VertexBuffer::Config config = VertexXyzRgbUvN::GetConfig();
        config.primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        return config;
    }

    static bool LoadShapeModel(
        ShapeType shapeType,
        std::vector<uint8_t>* pVerticesOut,
        std::vector<uint32_t>* pIndicesOut,
        VertexBuffer::Config* pVtxBufCfg)
    {
        if (shapeType == ShapeType::SPHERE) {
            *pVtxBufCfg = CreateSphereModel(pVerticesOut, pIndicesOut);
            return true;
        }
        return false;
    }

    Drawable& ModelLibrary::getOrCreateDrawable(
        Context& context,
        const ModelDesc& model,
        const Material& material,
        CommandBufferFactory& commandBufferFactory,
        CommandQueue commandQueue)
    {
        std::string modelPath = context.getAppConfig().dataDirectoryPath + "/" + model.modelPathOrShapeName;

        Drawable* pDrawable = findDrawable(modelPath, material);
        if (pDrawable != nullptr) {
            return *pDrawable;
        }

        // make a copy of the override paths (if any) so that we can add the model file's images if there are some.
        ModelDesc::Images modelImages = model.imageOverrides;
       
        std::vector<uint8_t> vertices;
        std::vector<uint32_t> indices;
        ShapeType shapeType = ShapeType::NONE;
        VertexBuffer::Config vertexBufferCfg;
        if (ModelIsShape(model.modelPathOrShapeName, &shapeType)) {
            if (!LoadShapeModel(shapeType, &vertices, &indices, &vertexBufferCfg)) {
                std::string error = "Unknown shape type: " + model.modelPathOrShapeName;
                throw std::runtime_error(error);
            }
        } else {
            tinyobj::attrib_t attrib;
            std::vector<tinyobj::shape_t> shapes;
            std::vector<tinyobj::material_t> materials;
            std::string warn, err;

            if (!tinyobj::LoadObj(
                    &attrib, &shapes, &materials, &warn, &err,
                    modelPath.c_str())) {
                throw std::runtime_error(warn + err);
            }

            // This is kind of crappy way to do this.
            if (VertexBuffer::ComputeVertexStride(material.getVertexShaderInputs()) ==
                    VertexBuffer::ComputeVertexStride(VertexXyzRgbUv::GetConfig().vertexAttrDescriptions)) {
                CreateVertsFromShapes<VertexXyzRgbUv>(
                    attrib,
                    shapes,
                    CreateXyzRgbUv,
                    &vertices,
                    &indices);

                vertexBufferCfg = VertexXyzRgbUv::GetConfig();
            } else if (VertexBuffer::ComputeVertexStride(material.getVertexShaderInputs()) ==
                       VertexBuffer::ComputeVertexStride(VertexXyzRgbUvN::GetConfig().vertexAttrDescriptions)) {
                CreateVertsFromShapes<VertexXyzRgbUvN>(
                    attrib,
                    shapes,
                    CreateXyzRgbUvN,
                    &vertices,
                    &indices);

                vertexBufferCfg = VertexXyzRgbUvN::GetConfig();
            } else {
                assert(false && "Unkown vertex config.");
            }

            if (!materials.empty()) {
                if (model.imageOverrides.find(Material::ImageType::Diffuse) == model.imageOverrides.end()) {
                    modelImages[Material::ImageType::Diffuse] = materials.front().diffuse_texname;
                }
                // TODO support other types of images.
            }
        }

        std::unique_ptr<VertexBuffer> spVertexBuffer;
        std::unique_ptr<IndexBuffer> spIndexBuffer;

        CreateVertexBuffers(
            vertices, indices, vertexBufferCfg,
            context, commandBufferFactory, commandQueue,
            &spVertexBuffer, &spIndexBuffer);

        std::map<Material::ImageType, std::pair<const ImageView*, const Sampler*>> imageSamplers;
        if (!material.getImageTypes().empty()) {
            for (auto imageType : material.getImageTypes()) {
                std::string texturePath = context.getAppConfig().dataDirectoryPath + "/" + modelImages[imageType];
                Image& image =
                    getOrLoadImage(
                        texturePath,
                        context,
                        commandBufferFactory,
                        commandQueue);

                ImageView& imageView =
                    (image.getOrCreateView(
                        ImageView::Config(
                            image.getFormat(), VK_IMAGE_VIEW_TYPE_2D)));

                uint32_t mipLevels =
                    vgfx::Image::ComputeMipLevels2D(image.getWidth(), image.getHeight());
                Sampler& sampler =
                    getOrCreateSampler(
                        vgfx::Sampler::Config(
                            VK_FILTER_LINEAR,
                            VK_FILTER_LINEAR,
                            VK_SAMPLER_MIPMAP_MODE_LINEAR,
                            0.0f, // min lod
                            static_cast<float>(mipLevels),
                            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                            false, 0),
                        context); // Last two parameters are for anisotropic filtering
                imageSamplers[imageType] = std::make_pair<const ImageView*, const Sampler*>(&imageView, &sampler);
            }
        }
        auto& models = m_drawableLibrary[modelPath];
        models.push_back(std::move(
            std::make_unique<Drawable>(
                context,
                std::move(spVertexBuffer),
                std::move(spIndexBuffer),
                material,
                imageSamplers)));

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
        if (pPixels == nullptr) {
            std::string error = "Failed to load image: " + path;
            throw std::runtime_error(error);
        }

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
