#include "VulkanGraphicsObject.h"

namespace vgfx
{
    void Object::addDrawable(Drawable& drawable)
    {
        m_drawables.push_back(&drawable);
    }

    void Object::update()
    {
        for (auto& pDrawable : m_drawables) {
            pDrawable->update();
        }
    }

    void Object::recordDrawCommands(
        size_t swapChainIndex,
        VkPipelineLayout pipelineLayout,
        VkCommandBuffer commandBuffer)
    {
        for (auto& pDrawable : m_drawables) {
            pDrawable->recordDrawCommands(swapChainIndex, pipelineLayout, commandBuffer);
        }
    }
}
