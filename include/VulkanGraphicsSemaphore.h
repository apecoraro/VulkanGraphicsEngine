#ifndef QUANTUM_GFX_SEMAPHORE_H
#define QUANTUM_GFX_SEMAPHORE_H

#include "VulkanGraphicsContext.h"

#include <vulkan/vulkan.h>

namespace vgfx
{
    class Semaphore
    {
    public:
        Semaphore(Context& context);
        ~Semaphore();

        VkSemaphore getHandle() { return m_semaphore;  }

    private:
        Context& m_context;

        VkSemaphore m_semaphore = VK_NULL_HANDLE;
    };
}

#endif
