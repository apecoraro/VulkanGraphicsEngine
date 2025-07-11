#include "VulkanGraphicsCamera.h"

#include "VulkanGraphicsBuffer.h"

namespace vgfx
{
    Camera::Camera(
        Context& context,
        const VkViewport& viewport,
        size_t frameBufferingCount)
        : m_rasterizerConfig({
            VK_POLYGON_MODE_FILL,
            VK_CULL_MODE_BACK_BIT,
            VK_FRONT_FACE_COUNTER_CLOCKWISE })
        , m_viewport(viewport)
    {
        Buffer::Config cameraMatrixBufferCfg(sizeof(glm::mat4));
        // Need one buffer each for each frame in flight and one for the current frame
        for (uint32_t index = 0; index < (frameBufferingCount + 1); ++index) {
            m_cameraMatrixBuffers.emplace_back(
                std::make_unique<Buffer>(
                    context,
                    Buffer::Type::UniformBuffer,
                    cameraMatrixBufferCfg));
        }
    }

    void Camera::update()
    {
        auto& currentBuffer = *m_cameraMatrixBuffers[m_currentBufferIndex];
        currentBuffer.update(&m_proj, sizeof(glm::mat4));

        ++m_currentBufferIndex;
        if (m_currentBufferIndex == m_cameraMatrixBuffers.size()) {
            m_currentBufferIndex = 0u;
        }
    }
}