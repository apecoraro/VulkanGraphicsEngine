#include "VulkanGraphicsUniformBuffer.h"

#include <cassert>

namespace vgfx
{
    UniformBuffer::UniformBuffer(
        Context& context,
        const Config& config)
        : m_context(context)
        , m_bufferSize(config.bufferSize)
        , m_copyCount(config.copyCount)
        , m_buffers(config.copyCount)
        , m_pMappedPtrs(config.copyCount, nullptr)
    {
        for (size_t i = 0; i < m_buffers.size(); ++i) {
            if (config.sharingMode == VK_SHARING_MODE_EXCLUSIVE) {
                m_buffers[i] =
                    context.getMemoryAllocator().createBuffer(
                        config.bufferSize,
                        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                        config.memoryUsage);
            } else {
                assert(!config.queueFamilyIndices.empty());
                m_buffers[i] =
                    context.getMemoryAllocator().createSharedBuffer(
                        config.bufferSize,
                        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                        config.queueFamilyIndices,
                        config.memoryUsage);
            }
        }
    }

    void UniformBuffer::destroy()
    {
        for (size_t i = 0; i < m_buffers.size(); ++i) {
            if (m_buffers[i].isValid()) {
                m_context.getMemoryAllocator().destroyBuffer(m_buffers[i]);
            }
        }
    }
}

