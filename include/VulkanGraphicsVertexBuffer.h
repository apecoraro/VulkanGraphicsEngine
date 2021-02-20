#ifndef VGFX_VERTEX_BUFFER_H
#define VGFX_VERTEX_BUFFER_H

#include "VulkanGraphicsContext.h"

#include "VulkanGraphicsCommandBuffers.h"

#include <cstdint>
#include <vector>

#include <vulkan/vulkan.h>

namespace vgfx
{
    class VertexBuffer
    {
    public:
        struct AttributeDescription
        {
            VkFormat format = VK_FORMAT_UNDEFINED;
            uint32_t offset = 0u;
            AttributeDescription(VkFormat fmt, uint32_t ofs) : format(fmt), offset(ofs) {}
        };

        struct Config
        {
            uint32_t vertexStride = 0u; 
            VkPrimitiveTopology primitiveTopology;
            VkSharingMode sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            // If VK_SHARING_MODE_CONCURRENT then queueFamilyIndices is required, which
            // is list of queue families that will access this buffer.
            std::vector<uint32_t> queueFamilyIndices;
            std::vector<AttributeDescription> vertexAttrDescriptions;

            Config(
                uint32_t vtxStride,
                VkPrimitiveTopology primTopo)
                : vertexStride(vtxStride)
                , primitiveTopology(primTopo)
            {
            }
        };

        using VertexData = void;

        VertexBuffer(
            Context& context,
            CommandBufferFactory& commandBufferFactory,
            const CommandQueue& commandQueue,
            const Config& config,
            const VertexData* pVertexData,
            size_t vertexDataSizeBytes);

        ~VertexBuffer() {
            destroy();
        }

        uint32_t getVertexStride() const { return m_vertexStride; }
        const std::vector<AttributeDescription>& getVertexAttributes() const { return m_vertexAttrDescriptions; }

        VkBuffer getHandle() { return m_buffer.handle; }

    private:
        void destroy();

        Context& m_context;

        uint32_t m_vertexStride;
        std::vector<AttributeDescription> m_vertexAttrDescriptions;

        MemoryAllocator::Buffer m_buffer;
    };
}

#endif