#pragma once

#include "VulkanGraphicsContext.h"
#include "VulkanGraphicsDescriptors.h"

#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include <vulkan/vulkan.h>

namespace vgfx
{
    class Buffer
    {
    public:
        struct Config
        {
            // Name attached to this buffer, can be used for debugging purposes.
            std::string bufferName;
            // Size of memory required for this Buffer.
            size_t bufferSize = 0u;
            VmaMemoryUsage memoryUsage = VmaMemoryUsage::VMA_MEMORY_USAGE_CPU_TO_GPU;
            VkSharingMode sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            // If VK_SHARING_MODE_CONCURRENT then queueFamilyIndices is required, which
            // is list of queue families that will access this buffer.
            std::vector<uint32_t> queueFamilyIndices;

            Config(size_t bufSize) : bufferSize(bufSize) {}
            Config(const std::string& bufName, size_t bufSize)
                : bufferName(bufName)
                , bufferSize(bufSize)
            {}
        };

        enum class Type
        {
            UniformBuffer = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            StorageBuffer = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            DynamicUniformBuffer = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
            DynamicStorageBuffer = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
        };

        Buffer(
            Context& context,
            Type type,
            const Config& config);

        ~Buffer() {
            destroy();
        }

        enum class MemMap
        {
            LeaveMapped,
            UnMap,
        };
        bool write(
            void* pData,
            size_t sizeOfDataBytes,
            size_t writeOffsetBytes = 0u,
            MemMap memMap = MemMap::UnMap)
        {
            assert(sizeOfDataBytes <= m_bufferSize);
            if (m_pMappedPtr == nullptr) {
                if (!m_context.getMemoryAllocator().mapBuffer(m_buffer, reinterpret_cast<void**>(&m_pMappedPtr))) {
                    return false;
                }
            }

            memcpy(m_pMappedPtr + writeOffsetBytes, pData, sizeOfDataBytes);

            if (memMap != MemMap::LeaveMapped) {
                m_context.getMemoryAllocator().unmapBuffer(m_buffer);
                m_pMappedPtr = nullptr;
            }
            return true;
        }

        VkBuffer getHandle() { return m_buffer.handle;  }
        size_t getSize() const { return m_bufferSize; }
		Type getType() const { return m_type; }

    private:
        void destroy();

        Context& m_context;
        Type m_type;
        size_t m_bufferSize = 0u;

        MemoryAllocator::Buffer m_buffer;
        uint8_t* m_pMappedPtr = nullptr;
    };

    class BufferUpdater : public DescriptorUpdater
    {
    public:
        BufferUpdater(Buffer& buffer)
            : DescriptorUpdater(static_cast<VkDescriptorType>(buffer.getType()))
            , m_buffer(buffer)
        {
            m_bufferInfo.buffer = m_buffer.getHandle();
            m_bufferInfo.offset = 0;
            m_bufferInfo.range = buffer.getSize();
		}

        BufferUpdater(Buffer& buffer, size_t offset, size_t range)
            : DescriptorUpdater(static_cast<VkDescriptorType>(buffer.getType()))
            , m_buffer(buffer)
        {
            m_bufferInfo.buffer = m_buffer.getHandle();
            m_bufferInfo.offset = offset;
            m_bufferInfo.range = range;
        }

        void update(VkWriteDescriptorSet* pWriteSet) const override
        {
            DescriptorUpdater::update(pWriteSet);

            VkWriteDescriptorSet& writeSet = *pWriteSet;
            writeSet.dstArrayElement = 0;

            writeSet.pBufferInfo = &m_bufferInfo;
        }

    private:
        Buffer& m_buffer;
        VkDescriptorBufferInfo m_bufferInfo = {};
    };

    class DynamicRingBuffer {
    public:
        DynamicRingBuffer(
            Context& context,
            size_t bufferSize)
            : m_spBuffer(std::make_unique<Buffer>(
                context,
                Buffer::Type::DynamicStorageBuffer,
				Buffer::Config(bufferSize)))
            , m_head(0u)
            , m_tail(0u)
        {
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(context.getPhysicalDevice(), &props);
            m_alignment = props.limits.minUniformBufferOffsetAlignment;
        }

        ~DynamicRingBuffer() { }

		Buffer& getBuffer() { return *m_spBuffer; }

        size_t write(
            void* pData,
            size_t sizeOfDataBytes)
        {
            // Align the current head
            size_t alignedHead = (m_head + m_alignment - 1) & ~(m_alignment - 1);

            // Wrap around if we hit the end of the physical buffer
            if (alignedHead + sizeOfDataBytes > m_spBuffer->getSize()) {
                alignedHead = 0;
            }

            // Check if we've caught up to the tail (buffer full)
            // Note: Simple logic assumes frame sync handles actual safety
            if (alignedHead < m_tail && alignedHead + sizeOfDataBytes > m_tail) {
                throw std::runtime_error("Ring buffer exhausted!");
            }

            m_head = alignedHead + sizeOfDataBytes;
            m_spBuffer->write(pData, sizeOfDataBytes, alignedHead, Buffer::MemMap::LeaveMapped);
            return alignedHead;
        }

        // Reset the head to the beginning (or move tail to head after GPU sync)
        void reset()
        {
            m_head = 0;
            m_tail = 0;
        }

        // Move tail forward to release memory after a GPU fence is hit
        void releaseTo(size_t offset)
        {
            m_tail = offset;
        }

    private:
        std::unique_ptr<Buffer> m_spBuffer;
        size_t m_alignment = 0u;
        size_t m_head = 0u; // Where we are currently allocating
        size_t m_tail = 0u; // Where the GPU has finished reading
    };

}

