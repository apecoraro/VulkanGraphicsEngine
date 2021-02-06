#ifndef QUANTUM_GFX_INDEX_BUFFER_H
#define QUANTUM_GFX_INDEX_BUFFER_H

#include "VulkanGraphicsContext.h"

#include "VulkanGraphicsCommandBuffers.h"

#include <cstdint>
#include <vector>

#include <vulkan/vulkan.h>

namespace vgfx
{
    class IndexBuffer
    {
    public:
        struct Config
        {
            VkIndexType indexType = VK_INDEX_TYPE_UINT16;
            bool hasPrimitiveRestartValues = false;
            VkSharingMode sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            // If VK_SHARING_MODE_CONCURRENT then queueFamilyIndices is required, which
            // is list of queue families that will access this buffer.
            std::vector<uint32_t> queueFamilyIndices;

            Config(
                VkIndexType idxType,
                bool hasPrimRestartVals=false)
                : indexType(idxType)
                , hasPrimitiveRestartValues(hasPrimRestartVals)
            {

            }
        };

        IndexBuffer(
            Context& context,
            CommandBufferFactory& commandBufferFactory,
            const CommandQueue& commandQueue,
            const Config& config,
            const void* pIndices,
            uint32_t numIndices);

        ~IndexBuffer() {
            destroy();
        }

        VkIndexType getType() const { return m_indexType; }

        VkBuffer getHandle() { return m_buffer.handle; }

        uint32_t getCount() const { return m_numIndices; }

        bool getHasPrimitiveRestartValues() const { return m_hasPrimitiveRestartValues; }

    private:
        void destroy();

        Context& m_context;

        VkIndexType m_indexType = VK_INDEX_TYPE_MAX_ENUM;
        uint32_t m_numIndices = 0u;

        bool m_hasPrimitiveRestartValues = false;

        MemoryAllocator::Buffer m_buffer;
    };
}

#endif