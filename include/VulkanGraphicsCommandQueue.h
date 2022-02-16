#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

namespace vgfx
{
    struct CommandQueue
    {
        uint32_t queueFamilyIndex = -1;
        CommandQueue(uint32_t qfi) : queueFamilyIndex(qfi) { }
        VkQueue queue = VK_NULL_HANDLE;
    };
}

