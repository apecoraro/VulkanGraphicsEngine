#include "VulkanGraphicsIndexBuffer.h"

#include "VulkanGraphicsOneTimeCommands.h"

#include <cassert>

namespace vgfx
{
    VkDeviceSize IndexTypeSizeBytes(VkIndexType indexType)
    {
        switch (indexType)
        {
        case VK_INDEX_TYPE_UINT16:
            return sizeof(uint16_t);
        case VK_INDEX_TYPE_UINT32:
            return sizeof(uint32_t);
        case VK_INDEX_TYPE_UINT8_EXT:
            return sizeof(uint8_t);
        default:
            assert(false && "Invalid index type!");
            return 0u;
        }
    }

    IndexBuffer::IndexBuffer(
        Context& context,
        CommandBufferFactory& commandBufferFactory,
        CommandQueue& commandQueue,
        const Config& config,
        const void* pIndices,
        uint32_t numIndices)
        : m_context(context)
        , m_indexType(config.indexType)
        , m_numIndices(numIndices)
        , m_hasPrimitiveRestartValues(config.hasPrimitiveRestartValues)
    {
        VkDeviceSize dataSizeBytes = numIndices * IndexTypeSizeBytes(m_indexType);
        if (config.sharingMode == VK_SHARING_MODE_EXCLUSIVE) {
            m_buffer =
                context.getMemoryAllocator().createBuffer(
                    dataSizeBytes,
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                    VmaMemoryUsage::VMA_MEMORY_USAGE_GPU_ONLY);
        } else {
            assert(!config.queueFamilyIndices.empty());
            m_buffer =
                context.getMemoryAllocator().createSharedBuffer(
                    dataSizeBytes,
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                    config.queueFamilyIndices,
                    VmaMemoryUsage::VMA_MEMORY_USAGE_GPU_ONLY);
        }

        OneTimeCommandsHelper helper(
            context,
            commandBufferFactory,
            commandQueue);

        helper.copyDataToBuffer(m_buffer, pIndices, dataSizeBytes);
    }

    void IndexBuffer::destroy()
    {
        if (m_buffer.isValid()) {
            m_context.getMemoryAllocator().destroyBuffer(m_buffer);
        }
    }
}