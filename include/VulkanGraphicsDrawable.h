#pragma once

#include "VulkanGraphicsContext.h"

#include "VulkanGraphicsImage.h"
#include "VulkanGraphicsIndexBuffer.h"
#include "VulkanGraphicsEffects.h"
#include "VulkanGraphicsRenderer.h"
#include "VulkanGraphicsSampler.h"
#include "VulkanGraphicsVertexBuffer.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include <glm/ext/matrix_transform.hpp>
#include <vulkan/vulkan.h>

namespace vgfx
{
    enum class ImageType
    {
        Diffuse,
    };
    using ImageSampler = std::pair<const ImageView*, const Sampler*>;
    using ImageSamplers = std::map<ImageType, ImageSampler>;
    // Geometry of an single drawable object (vertices and indices).
    class Drawable
    {
    public:
        Drawable(
            Context& context,
            VertexBuffer& vertexBuffer,
            IndexBuffer& indexBuffer,
            ImageSamplers& imageSamplers)
            : m_vertexBuffer(vertexBuffer)
            , m_indexBuffer(indexBuffer)
            , m_imageSamplers(imageSamplers)
        {
        }

        void draw(DrawContext& recorder);

        const VertexBuffer& getVertexBuffer() const { return m_vertexBuffer; }
        VertexBuffer& getVertexBuffer() { return m_vertexBuffer; }

        const IndexBuffer& getIndexBuffer() const { return m_indexBuffer; }
        IndexBuffer& getIndexBuffer() { return m_indexBuffer; }

        void setMeshEffect(MeshEffect* pMeshEffect) { m_pMeshEffect = pMeshEffect; }
        const MeshEffect* getMeshEffect() const { return m_pMeshEffect; }

        const glm::mat4& getWorldTransform() const { return m_worldTransform; }
        void setWorldTransform(const glm::mat4& worldTransform) { m_worldTransform = worldTransform; }

        void setImageSampler(ImageType type, const ImageSampler& imageSampler)
        {
            m_imageSamplers[type] = imageSampler;
        }

        ImageSampler& getImageSampler(ImageType imageType)
        {
            const auto& findIt = m_imageSamplers.find(imageType);
            if (findIt == m_imageSamplers.end()) {
                return (m_imageSamplers[imageType] = std::make_pair<const ImageView*, const Sampler*>(nullptr, nullptr));
            }
            return findIt->second;
        }

    protected:
        void configureDescriptorSets(DrawContext& drawContext, std::vector<VkDescriptorSet>* pDescriptorSets);

    private:

        VertexBuffer& m_vertexBuffer;
        IndexBuffer& m_indexBuffer;
        const MeshEffect* m_pMeshEffect = nullptr;
        glm::mat4 m_worldTransform = glm::identity<glm::mat4>();
        std::vector<VkDescriptorSet> m_descriptorSets;
        ImageSamplers m_imageSamplers;
    };
}

