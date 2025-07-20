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
    using ImageSamplerMap = std::map<MeshEffect::ImageType, std::pair<const ImageView*, const Sampler*>>;
    // Geometry of an single drawable object (vertices and indices).
    class Drawable
    {
    public:
        Drawable(
            Context& context,
            VertexBuffer& vertexBuffer,
            IndexBuffer& indexBuffer,
            const MeshEffect& meshEffect,
            const ImageSamplerMap& imageSamplers)
            : m_vertexBuffer(vertexBuffer)
            , m_indexBuffer(indexBuffer)
            , m_meshEffect(meshEffect)
            , m_imageSamplers(imageSamplers)
        {
            
        }

        void draw(DrawContext& recorder);

        const VertexBuffer& getVertexBuffer() const { return m_vertexBuffer; }
        VertexBuffer& getVertexBuffer() { return m_vertexBuffer; }

        const IndexBuffer& getIndexBuffer() const { return m_indexBuffer; }
        IndexBuffer& getIndexBuffer() { return m_indexBuffer; }

        const MeshEffect& getMeshEffect() const { return m_meshEffect; }
 
        size_t getImageCount() const { return m_imageSamplers.size(); }

        std::pair<const ImageView*, const Sampler*> getImageSampler(MeshEffect::ImageType imageType) const
        {
            const auto& findIt = m_imageSamplers.find(imageType);
            if (findIt == m_imageSamplers.end()) {
                return std::make_pair<const ImageView*, const Sampler*>(nullptr, nullptr);
            }
            return findIt->second;
        }

        const glm::mat4& getWorldTransform() const { return m_worldTransform; }
        void setWorldTransform(const glm::mat4& worldTransform) { m_worldTransform = worldTransform; }

    protected:
        void configureDescriptorSets(DrawContext& drawContext, std::vector<VkDescriptorSet>* pDescriptorSets);

    private:

        VertexBuffer& m_vertexBuffer;
        IndexBuffer& m_indexBuffer;
        const MeshEffect& m_meshEffect;
        std::map<MeshEffect::ImageType, std::pair<const ImageView*, const Sampler*>> m_imageSamplers;
        glm::mat4 m_worldTransform = glm::identity<glm::mat4>();
        std::vector<VkDescriptorSet> m_descriptorSets;
    };
}

