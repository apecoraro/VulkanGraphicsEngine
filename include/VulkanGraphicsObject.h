#ifndef VGFX_OBJECT_H
#define VGFX_OBJECT_H

#include <vector>

#include <vulkan/vulkan.h>

namespace vgfx
{
    class Drawable;
    // Represents an Object in the scene, has a world transformation, can be composed of
    // multiple Drawables each with their own transformation relative to this.
    class Object
    {
    public:
        Object() = default;
        ~Object() = default;

        void addDrawable(Drawable& pDrawable);

        void recordDrawCommands(
            size_t swapChainIndex,
            VkCommandBuffer commandBuffer);
    public:
        std::vector<Drawable*> m_drawables;
    };
}

#endif
