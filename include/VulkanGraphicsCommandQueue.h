#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

namespace vgfx
{
    struct CommandQueue
    {
        uint32_t queueFamilyIndex = -1;
        VkQueue queue = VK_NULL_HANDLE;
        CommandQueue(uint32_t qfi, VkQueue q) : queueFamilyIndex(qfi), queue(q) { }
    };
}

