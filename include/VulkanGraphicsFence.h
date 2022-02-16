#pragma once

#include "VulkanGraphicsContext.h"

#include <vulkan/vulkan.h>

namespace vgfx
{
    class Fence
    {
    public:
        Fence(Context& context);
        ~Fence();

        VkFence getHandle() { return m_fence;  }
    private:
        Context& m_context;
        VkFence m_fence;
    };
}

