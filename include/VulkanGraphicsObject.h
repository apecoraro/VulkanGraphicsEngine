#ifndef VGFX_OBJECT_H
#define VGFX_OBJECT_H

#include "VulkanGraphicsDrawable.h"

#include <memory>

namespace vgfx
{
    // Represents an Object in the scene, has a world transformation, can be composed of
    // multiple Drawables each with their own transformation relative to this.
    class Object
    {
    public:
        Object() = default;
        ~Object() = default;

        void addDrawable(Drawable& pDrawable);

        // Call update on each Drawable
        void update();

        void recordDrawCommands(
            size_t swapChainIndex,
            VkPipelineLayout pipelineLayout,
            VkCommandBuffer commandBuffer);
    public:
        std::vector<Drawable*> m_drawables;
    };
}

#endif
