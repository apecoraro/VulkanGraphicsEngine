#include "VulkanGraphicsVertexBuffer.h"

#include "VulkanGraphicsOneTimeCommands.h"

#include <cassert>

namespace vgfx
{
    VertexBuffer::VertexBuffer(
        Context& context,
        CommandBufferFactory& commandBufferFactory,
        CommandQueue& commandQueue,
        const Config& config,
        const VertexData* pVertexData,
        size_t vertexDataSizeBytes)
        : m_context(context)
        , m_vertexStride(config.vertexStride)
        , m_vertexAttrDescriptions(config.vertexAttrDescriptions)
    {
        if (config.sharingMode == VK_SHARING_MODE_EXCLUSIVE) {
            m_buffer =
                context.getMemoryAllocator().createBuffer(
                    vertexDataSizeBytes,
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                    VmaMemoryUsage::VMA_MEMORY_USAGE_GPU_ONLY);
        } else {
            assert(!config.queueFamilyIndices.empty());
            m_buffer =
                context.getMemoryAllocator().createSharedBuffer(
                    vertexDataSizeBytes,
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                    config.queueFamilyIndices,
                    VmaMemoryUsage::VMA_MEMORY_USAGE_GPU_ONLY);
        }

        OneTimeCommandsHelper helper(
            context,
            commandBufferFactory,
            commandQueue);

        helper.copyDataToBuffer(m_buffer, pVertexData, vertexDataSizeBytes); 
    }

    void VertexBuffer::destroy()
    {
        if (m_buffer.isValid()) {
            m_context.getMemoryAllocator().destroyBuffer(m_buffer);
        }
    }
}
