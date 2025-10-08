#include "VulkanGraphicsVertexBuffer.h"

#include "VulkanGraphicsOneTimeCommands.h"

#include <cassert>

namespace vgfx
{
    uint32_t VertexBuffer::ComputeVertexStride(const std::vector<AttributeDescription>& vtxAttrs)
    {
        assert(!vtxAttrs.empty());

        const auto& attr = vtxAttrs.back();

        switch (attr.format) {
        case VK_FORMAT_R32G32B32A32_SFLOAT:
            return attr.offset + (sizeof(float) * 4);
        case VK_FORMAT_R32G32B32_SFLOAT:
            return attr.offset + (sizeof(float) * 3);
        case VK_FORMAT_R32G32_SFLOAT:
            return attr.offset + (sizeof(float) * 2);
        }
     
        return 0u;
    }

    VertexBuffer::VertexBuffer(
        Context& context,
        CommandBufferFactory& commandBufferFactory,
        const Config& config,
        const VertexData* pVertexData,
        size_t vertexDataSizeBytes)
        : m_context(context)
        , m_config(config)
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

        OneTimeCommandsHelper helper(context, commandBufferFactory);

        helper.copyDataToBuffer(m_buffer, pVertexData, vertexDataSizeBytes); 
    }

    void VertexBuffer::destroy()
    {
        if (m_buffer.isValid()) {
            m_context.getMemoryAllocator().destroyBuffer(m_buffer);
        }
    }


    const VertexBuffer::Config& VertexXyzRgbUv::GetConfig()
    {
        static VertexBuffer::Config s_vertexXyzRgbUvConfig(
            sizeof(VertexXyzRgbUv),
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
   
        // Initialize the vertex attribute descriptions if not already.
        if (s_vertexXyzRgbUvConfig.vertexAttrDescriptions.empty()) {
            s_vertexXyzRgbUvConfig.vertexAttrDescriptions.push_back(
                VertexBuffer::AttributeDescription(
                    VK_FORMAT_R32G32B32_SFLOAT,
                    offsetof(VertexXyzRgbUv, pos)));

            s_vertexXyzRgbUvConfig.vertexAttrDescriptions.push_back(
                VertexBuffer::AttributeDescription(
                    VK_FORMAT_R32G32B32_SFLOAT,
                    offsetof(VertexXyzRgbUv, color)));

            s_vertexXyzRgbUvConfig.vertexAttrDescriptions.push_back(
                VertexBuffer::AttributeDescription(
                    VK_FORMAT_R32G32_SFLOAT,
                    offsetof(VertexXyzRgbUv, texCoord)));
        }

        return s_vertexXyzRgbUvConfig;
    }

    const VertexBuffer::Config& VertexXyzRgbUvN::GetConfig()
    {
        static VertexBuffer::Config s_vertexXyzRgbUvNConfig(
            sizeof(VertexXyzRgbUvN),
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
        // Initialize the vertex attribute descriptions if not already.
        if (s_vertexXyzRgbUvNConfig.vertexAttrDescriptions.empty()) {
            s_vertexXyzRgbUvNConfig.vertexAttrDescriptions.push_back(
                VertexBuffer::AttributeDescription(
                    VK_FORMAT_R32G32B32_SFLOAT,
                    offsetof(VertexXyzRgbUvN, pos)));

            s_vertexXyzRgbUvNConfig.vertexAttrDescriptions.push_back(
                VertexBuffer::AttributeDescription(
                    VK_FORMAT_R32G32B32_SFLOAT,
                    offsetof(VertexXyzRgbUvN, color)));

            s_vertexXyzRgbUvNConfig.vertexAttrDescriptions.push_back(
                VertexBuffer::AttributeDescription(
                    VK_FORMAT_R32G32_SFLOAT,
                    offsetof(VertexXyzRgbUvN, texCoord)));

            s_vertexXyzRgbUvNConfig.vertexAttrDescriptions.push_back(
                VertexBuffer::AttributeDescription(
                    VK_FORMAT_R32G32B32_SFLOAT,
                    offsetof(VertexXyzRgbUvN, normal)));
        }

        return s_vertexXyzRgbUvNConfig;
    }
}
