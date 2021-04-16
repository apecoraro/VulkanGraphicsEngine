#ifndef VGFX_UNIFORM_BUFFER_H
#define VGFX_UNIFORM_BUFFER_H

#include "VulkanGraphicsContext.h"
#include "VulkanGraphicsDescriptors.h"

#include <cassert>
#include <cstdint>
#include <vector>

#include <vulkan/vulkan.h>

namespace vgfx
{
    class UniformBuffer : public DescriptorUpdater
    {
    public:
        struct Config
        {
            // Size of memory required for this UniformBuffer.
            size_t bufferSize = 0u;
            VmaMemoryUsage memoryUsage = VmaMemoryUsage::VMA_MEMORY_USAGE_CPU_TO_GPU;
            VkSharingMode sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            // If VK_SHARING_MODE_CONCURRENT then queueFamilyIndices is required, which
            // is list of queue families that will access this buffer.
            std::vector<uint32_t> queueFamilyIndices;

            Config(size_t bufSize) : bufferSize(bufSize) {}
        };
        UniformBuffer(
            Context& context,
            const Config& config);

        ~UniformBuffer() {
            destroy();
        }

        bool update(
            void* pData,
            size_t sizeOfDataBytes,
            bool leaveMapped)
        {
            assert(sizeOfDataBytes <= m_bufferSize);
            if (m_pMappedPtr == nullptr) {
                if (!m_context.getMemoryAllocator().mapBuffer(m_buffer, &m_pMappedPtr)) {
                    return false;
                }
            }

            memcpy(m_pMappedPtr, pData, sizeOfDataBytes);

            if (!leaveMapped) {
                m_context.getMemoryAllocator().unmapBuffer(m_buffer);
                m_pMappedPtr = nullptr;
            }
            return true;
        }

        VkBuffer getHandle() { return m_buffer.handle;  }
        size_t getSize() const { return m_bufferSize; }

        void write(VkWriteDescriptorSet* pWriteSet) override
        {
            DescriptorUpdater::write(pWriteSet);

            VkWriteDescriptorSet& writeSet = *pWriteSet;
            writeSet.dstArrayElement = 0;

            writeSet.pBufferInfo = &m_bufferInfo;
        }

    private:
        void destroy();

        Context& m_context;
        size_t m_bufferSize = 0u;

        MemoryAllocator::Buffer m_buffer;
        void* m_pMappedPtr = nullptr;

        VkDescriptorBufferInfo m_bufferInfo = {};
    };
}

#endif