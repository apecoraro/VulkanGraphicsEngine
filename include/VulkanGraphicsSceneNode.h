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

        virtual void draw(Renderer::DrawContext& recorder) = 0;
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

    class CameraNode : public GroupNode
    {
    public:
        CameraNode(Context& context, size_t maxFramesInFlight);
        CameraNode(Context& context, size_t maxFramesInFlight, const glm::mat4& view, const glm::mat4& proj)
            : CameraNode(context, maxFramesInFlight)
        {
            m_view = view;
            m_proj = proj;
        }

        const glm::mat4& getView() const { return m_view; }

        void setView(const glm::mat4& view)
        {
            m_view = view;
        }

        void setProjection(const glm::mat4& proj)
        {
            m_proj = proj;
            m_projUpdated = true;
        }

        void draw(Renderer::DrawContext& drawState) override;

        Buffer& getProjectionBuffer() { return *m_cameraMatrixBuffers[m_currentBufferIndex].get(); }

    private:
        std::vector<std::unique_ptr<Buffer>> m_cameraMatrixBuffers;
        
        glm::mat4 m_view = glm::identity<glm::mat4>();
        glm::mat4 m_proj = glm::identity<glm::mat4>();
        bool m_projUpdated = true;

        size_t m_currentBufferIndex;
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

        void draw(Renderer::DrawContext& recorder) override;
    
        const RenderTarget::Config& getRenderTargetConfig() const { return m_renderTargetConfig; }
        const std::optional<const std::map<size_t, VkImageLayout>>& getInputs() const { return m_inputs; }

        void setRenderPass(std::unique_ptr<RenderPass> spRenderPass);
    private:
        RenderTarget::Config m_renderTargetConfig;
        std::optional<const std::map<size_t, VkImageLayout>> m_inputs;

        std::unique_ptr<RenderPass> m_spRenderPass;
        std::unique_ptr<RenderTarget> m_spRenderTarget;
    };

    class GraphicsNode : public SceneNode
    {
    public:
        GraphicsNode(std::unique_ptr<Pipeline>&& spGraphicsPipeline);

        void addObject(std::unique_ptr<Object>&& newObj);

        void initRenderingResources(Renderer::renderer& renderer) override;
        void draw(Renderer::DrawContext& recorder) override;

    private:
        std::unique_ptr<Pipeline> m_spGraphicsPipeline;

        std::vector<std::unique_ptr<Object>> m_graphicsObjects;
    };
}
