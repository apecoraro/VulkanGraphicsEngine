#pragma once

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
            VkCommandBuffer commandBuffer,
            size_t swapChainIndex);
    public:
        std::vector<Drawable*> m_drawables;
    };
}

