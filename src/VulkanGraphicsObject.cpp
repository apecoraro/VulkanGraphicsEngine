#include "VulkanGraphicsObject.h"

#include "VulkanGraphicsDrawable.h"

namespace vgfx
{
    void Object::addDrawable(Drawable& drawable)
    {
        m_drawables.push_back(&drawable);
    }

    void Object::recordDrawCommands(
        VkCommandBuffer commandBuffer,
        size_t swapChainIndex)
    {
        for (auto& pDrawable : m_drawables) {
            pDrawable->recordDrawCommands(commandBuffer, swapChainIndex);
        }
    }
}
