#include "VulkanGraphicsUniformBuffer.h"

#include <cassert>

namespace vgfx
{
    UniformBuffer::UniformBuffer(
        Context& context,
        const Config& config)
        : m_context(context)
        , m_bufferSize(config.bufferSize)
    {
        if (config.sharingMode == VK_SHARING_MODE_EXCLUSIVE) {
            m_buffer =
                context.getMemoryAllocator().createBuffer(
                    config.bufferSize,
                    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                    config.memoryUsage);
        } else {
            assert(!config.queueFamilyIndices.empty());
            m_buffer =
                context.getMemoryAllocator().createSharedBuffer(
                    config.bufferSize,
                    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                    config.queueFamilyIndices,
                    config.memoryUsage);
        }
    }

    void UniformBuffer::destroy()
    {
        if (m_buffer.isValid()) {
            if (m_pMappedPtr != nullptr) {
                m_context.getMemoryAllocator().unmapBuffer(m_buffer);
                m_pMappedPtr = nullptr;
            }
            m_context.getMemoryAllocator().destroyBuffer(m_buffer);
        }
    }
}

