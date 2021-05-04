#include "VulkanGraphicsBuffer.h"

#include <cassert>

namespace vgfx
{
    static VkBufferUsageFlags TypeToUsage(Buffer::Type type)
    {
        switch (type) {
        case Buffer::Type::StorageBuffer:
        case Buffer::Type::DynamicStorageBuffer:
            return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        case Buffer::Type::UniformBuffer:
        case Buffer::Type::DynamicUniformBuffer:
            return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        default:
            assert(false);
            return VK_BUFFER_USAGE_FLAG_BITS_MAX_ENUM;
        }
    }

    Buffer::Buffer(
        Context& context,
        Type type,
        const Config& config)
        : DescriptorUpdater(static_cast<VkDescriptorType>(type))
        , m_context(context)
        , m_bufferSize(config.bufferSize)
    {
        const char* pBufferName = nullptr;
        if (!config.bufferName.empty()) {
            pBufferName = config.bufferName.c_str();
        }
        if (config.sharingMode == VK_SHARING_MODE_EXCLUSIVE) {
            m_buffer =
                context.getMemoryAllocator().createBuffer(
                    config.bufferSize,
                    TypeToUsage(type),
                    config.memoryUsage,
                    pBufferName);
        } else {
            assert(!config.queueFamilyIndices.empty());
            m_buffer =
                context.getMemoryAllocator().createSharedBuffer(
                    config.bufferSize,
                    TypeToUsage(type),
                    config.queueFamilyIndices,
                    config.memoryUsage,
                    pBufferName);
        }

        m_bufferInfo.buffer = getHandle();
        m_bufferInfo.offset = 0;
        m_bufferInfo.range = getSize();
    }

    void Buffer::destroy()
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

