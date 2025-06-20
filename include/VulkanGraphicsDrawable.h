#pragma once

#include "VulkanGraphicsContext.h"

#include "VulkanGraphicsImage.h"
#include "VulkanGraphicsIndexBuffer.h"
#include "VulkanGraphicsMaterials.h"
#include "VulkanGraphicsRenderer.h"
#include "VulkanGraphicsSampler.h"
#include "VulkanGraphicsVertexBuffer.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

namespace vgfx
{
    // Geometry of an single drawable object (vertices and indices).
    class Drawable
    {
    public:
        Drawable(
            Context& context,
            std::unique_ptr<VertexBuffer> spVertexBuffer,
            std::unique_ptr<IndexBuffer> spIndexBuffer,
            const Material& material,
            const std::map<Material::ImageType, std::pair<const ImageView*, const Sampler*>>& imageSamplers)
            : m_spVertexBuffer(std::move(spVertexBuffer))
            , m_spIndexBuffer(std::move(spIndexBuffer))
            , m_material(material)
            , m_imageSamplers(imageSamplers)
        {
            
        }

        void draw(Renderer::DrawContext& recorder);

        const VertexBuffer& getVertexBuffer() const { return *m_spVertexBuffer.get(); }
        VertexBuffer& getVertexBuffer() { return *m_spVertexBuffer.get(); }

        const IndexBuffer& getIndexBuffer() const { return *m_spIndexBuffer.get(); }
        IndexBuffer& getIndexBuffer() { return *m_spIndexBuffer.get(); }

        const MaterialId& getMaterialId() const { return m_material.getId(); }
        const Material& getMaterial() const { return m_material; }
 
        size_t getImageCount() const { return m_imageSamplers.size(); }

        const std::pair<const ImageView*, const Sampler*>& getImageSampler(Material::ImageType imageType) const
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
        void configureDescriptorSets(Renderer::DrawContext& drawContext, std::vector<VkDescriptorSet>* pDescriptorSets);

    private:

        std::unique_ptr<VertexBuffer> m_spVertexBuffer;
        std::unique_ptr<IndexBuffer> m_spIndexBuffer;
        const Material& m_material;
        std::map<Material::ImageType, std::pair<const ImageView*, const Sampler*>> m_imageSamplers;
        glm::mat4 m_worldTransform;
        std::vector<VkDescriptorSet> m_descriptorSets;
    };
}

