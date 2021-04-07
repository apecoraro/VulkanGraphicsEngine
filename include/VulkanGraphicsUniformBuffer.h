#ifndef VGFX_UNIFORM_BUFFER_H
#define VGFX_UNIFORM_BUFFER_H

#include "VulkanGraphicsContext.h"

#include <cassert>
#include <cstdint>
#include <vector>

#include <vulkan/vulkan.h>

namespace vgfx
{
    class UniformBuffer
    {
    public:
        struct Config
        {
            // Size of memory required for this UniformBuffer.
            uint32_t bufferSize = 0u;
            // The number of copies of this UniformBuffer to create.
            uint32_t copyCount = 1u; 
            VmaMemoryUsage memoryUsage = VmaMemoryUsage::VMA_MEMORY_USAGE_CPU_TO_GPU;
            VkSharingMode sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            // If VK_SHARING_MODE_CONCURRENT then queueFamilyIndices is required, which
            // is list of queue families that will access this buffer.
            std::vector<uint32_t> queueFamilyIndices;

            Config(
                uint32_t bufSize,
                uint32_t cpyCount = 1u) : bufferSize(bufSize), copyCount(cpyCount) {}
        };
        UniformBuffer(
            Context& context,
            const Config& config);

        ~UniformBuffer() {
            destroy();
        }

        bool update(
            size_t bufferIndex,
            void* pData,
            size_t sizeOfDataBytes,
            bool leaveMapped)
        {
            assert(sizeOfDataBytes <= m_bufferSize);
            if (m_pMappedPtrs[bufferIndex] == nullptr) {
                if (!m_context.getMemoryAllocator().mapBuffer(m_buffers[bufferIndex], &m_pMappedPtrs[bufferIndex])) {
                    return false;
                }
            }

            memcpy(m_pMappedPtrs[bufferIndex], pData, sizeOfDataBytes);

            if (!leaveMapped) {
                m_context.getMemoryAllocator().unmapBuffer(m_buffers[bufferIndex]);
                m_pMappedPtrs[bufferIndex] = nullptr;
            }
            return true;
        }

        VkBuffer getHandle(size_t copyIndex) { return m_buffers[copyIndex].handle;  }
        uint32_t getSize() const { return m_bufferSize; }
        uint32_t getCopyCount() const { return m_copyCount; }

    private:
        void destroy();

        Context& m_context;
        uint32_t m_bufferSize = 0u;
        uint32_t m_copyCount = 1u; 

        std::vector<MemoryAllocator::Buffer> m_buffers;
        std::vector<void*> m_pMappedPtrs;
    };
}

#endif