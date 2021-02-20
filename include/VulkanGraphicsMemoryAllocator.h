#ifndef VGFX_MEMORY_ALLOCATOR_H
#define VGFX_MEMORY_ALLOCATOR_H

#include <cstdint>
#include <vector>

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace vgfx
{
    class Context;

    class MemoryAllocator
    {
    public:
        MemoryAllocator() = default;
        ~MemoryAllocator() {
            shutdown();
        }

        void init(
            uint32_t vulkanApiVersion,
            Context& context);

        void shutdown();

        struct Buffer
        {
            VkBuffer handle = VK_NULL_HANDLE;
            VmaAllocation allocation = VK_NULL_HANDLE;
            bool isValid() const {
                return handle != VK_NULL_HANDLE && allocation != VK_NULL_HANDLE;
            }
        };
        Buffer createBuffer(
            VkDeviceSize sizeBytes,
            VkBufferUsageFlags usage,
            VmaMemoryUsage bufferMemoryUsage);

        Buffer createSharedBuffer(
            VkDeviceSize sizeBytes,
            VkBufferUsageFlags usage,
            const std::vector<uint32_t>& queueFamilyIndices,
            VmaMemoryUsage bufferMemoryUsage);

        Buffer createBuffer(
            const VkBufferCreateInfo& bufferCreateInfo,
            VmaMemoryUsage bufferMemoryUsage);

        void destroyBuffer(Buffer& bufferAllocation);

        bool mapBuffer(Buffer& handle, void** ppData);
        void unmapBuffer(Buffer& handle);

        struct Image
        {
            VkImage handle = VK_NULL_HANDLE;
            VmaAllocation allocation = VK_NULL_HANDLE;
            bool isValid() const {
                return handle != VK_NULL_HANDLE && allocation != VK_NULL_HANDLE;
            }
        };

        Image createImage(
            const VkImageCreateInfo& imageCreateInfo,
            VmaMemoryUsage imageMemoryUsage);

        void destroyImage(Image& image);

    private:
        VmaAllocator m_allocator = VK_NULL_HANDLE;
    };
}

#endif
