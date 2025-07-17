#pragma once

#include "VulkanGraphicsPipeline.h"

#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>

namespace vgfx
{
    class Context;
    class Buffer;

    class Camera
    {
    public:
        Camera(Context& context, const VkViewport& viewport, size_t frameBufferingCount);
        Camera(
            Context& context,
            size_t frameBufferingCount,
            const glm::mat4& view,
            const glm::mat4& proj,
            const VkViewport& viewport)
            : Camera(context, viewport, frameBufferingCount)
        {
            m_view = view;
            m_proj = proj;
        }

        const glm::mat4& getView() const { return m_view; }

        void setView(const glm::mat4& view)
        {
            m_view = view;
        }

        const glm::mat4& getProj() const { return m_proj; }

        void setProjection(const glm::mat4& proj)
        {
            m_proj = proj;
            m_projBufferNeedsUpdateCount = m_cameraMatrixBuffers.size();
        }

        Buffer& getProjectionBuffer() { return *m_cameraMatrixBuffers[m_currentBufferIndex].get(); }

        Pipeline::RasterizerConfig getRasterizerConfig() const {
            return m_rasterizerConfig;
        }

        VkViewport getViewport() const {
            return m_viewport;
        }

        void update();

    private:
        std::vector<std::unique_ptr<Buffer>> m_cameraMatrixBuffers;
        size_t m_currentBufferIndex = 0u;

        glm::mat4 m_view = glm::identity<glm::mat4>();
        glm::mat4 m_proj = glm::identity<glm::mat4>();
        size_t m_projBufferNeedsUpdateCount = 0;
        VkViewport m_viewport = {};
        Pipeline::RasterizerConfig m_rasterizerConfig;
    };
}
