#pragma once

#include "VulkanGraphicsBuffer.h"
#include "VulkanGraphicsObject.h"
#include "VulkanGraphicsPipeline.h"
#include "VulkanGraphicsRenderer.h"
#include "VulkanGraphicsRenderTarget.h"

#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <memory>
#include <vector>

namespace vgfx
{
    class SceneNode
    {
    public:
        SceneNode() = default;

        virtual void draw(Renderer::DrawContext& drawContext) = 0;
    };

    class GroupNode : public SceneNode
    {
    public:
        GroupNode() = default;

        void addNode(std::unique_ptr<SceneNode>&& addNode)
        {
            m_children.emplace_back(std::move(addNode));
        }

        void draw(Renderer::DrawContext& drawState) override
        {
            for (auto& spChild : m_children) {
                spChild->draw(drawState);
            }
        }

    private:
        std::vector<std::unique_ptr<SceneNode>> m_children;
    };

    class RenderPassNode : public GroupNode
    {
    public:
        RenderPassNode(
            RenderTarget::Config renderTargetConfig,
            const std::optional<const std::map<size_t, VkImageLayout>>& inputs = std::nullopt)
            : m_renderTargetConfig(renderTargetConfig)
            , m_inputs(inputs)
        {
        }

        void draw(Renderer::DrawContext& drawContext) override;
    
        const RenderTarget::Config& getRenderTargetConfig() const { return m_renderTargetConfig; }
        const std::optional<const std::map<size_t, VkImageLayout>>& getInputs() const { return m_inputs; }

        void setRenderPass(std::unique_ptr<RenderPass> spRenderPass);
    private:
        RenderTarget::Config m_renderTargetConfig;
        std::optional<const std::map<size_t, VkImageLayout>> m_inputs;

        std::unique_ptr<RenderPass> m_spRenderPass;
        std::unique_ptr<RenderTarget> m_spRenderTarget;
    };

    class LightNode : public GroupNode
    {
    public:
        LightNode(
            const glm::vec3& position,
            const glm::vec3& color,
            const float radius = std::numeric_limits<float>::max())
            : m_position(position.x, position.y, position.z, 1.0f)
            , m_color(color)
            , m_radius(radius) { }

        void draw(Renderer::DrawContext& drawContext) override;

        const glm::vec4 getPosition() const { return m_position; }
        const glm::vec3 getColor() const { return m_color; }
        float getRadius() const { return m_radius; }

        void setPosition(const glm::vec3& position) { m_position = glm::vec4(position.x, position.y, position.z, 1.0f); }
        void setColor(const glm::vec3& color) { m_color = color; }
        void setRadius(float radius) { m_radius = radius; }

    private:
        glm::vec4 m_position = {};
        glm::vec3 m_color = {};
        float m_radius = std::numeric_limits<float>::max();
    };
}
