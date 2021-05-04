#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
// Inclusion of vk_mem_alloc.h needs to come before VulkanGraphicsMemoryAllocator.h
// so that the implementation code is enabled by the include here.
#include "VulkanGraphicsMemoryAllocator.h"

#include "VulkanGraphicsContext.h"

#include <cstdint>

#include <vulkan/vulkan.h>

namespace vgfx
{
    void MemoryAllocator::init(
        uint32_t vulkanApiVersion,
        Context& context)
    {
        if (m_allocator != VK_NULL_HANDLE) {
            shutdown();
        }

        VmaAllocatorCreateInfo allocatorInfo = {};
        allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_2;
        allocatorInfo.instance = context.getInstance();
        allocatorInfo.physicalDevice = context.getPhysicalDevice();
        allocatorInfo.device = context.getLogicalDevice();
        allocatorInfo.pAllocationCallbacks = context.getAllocationCallbacks();

        VkResult result = vmaCreateAllocator(&allocatorInfo, &m_allocator);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to create VMA, vmaCreateAllocator failed!");
        }
    }

    void MemoryAllocator::shutdown()
    {
        if (m_allocator != VK_NULL_HANDLE) {
            vmaDestroyAllocator(m_allocator);
            m_allocator = VK_NULL_HANDLE;
        }
    }

    MemoryAllocator::Buffer MemoryAllocator::createBuffer(
        VkDeviceSize sizeBytes,
        VkBufferUsageFlags usage,
        VmaMemoryUsage bufferMemoryUsage,
        const char* pBufferName)
    {
        VkBufferCreateInfo bufferCreateInfo = {};
        bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferCreateInfo.size = sizeBytes;
        bufferCreateInfo.usage = usage;
        bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        return createBuffer(bufferCreateInfo, bufferMemoryUsage, pBufferName);
    }

    MemoryAllocator::Buffer MemoryAllocator::createSharedBuffer(
        VkDeviceSize sizeBytes,
        VkBufferUsageFlags usage,
        const std::vector<uint32_t>& queueFamilyIndices,
        VmaMemoryUsage bufferMemoryUsage,
        const char* pBufferName)
    {
        VkBufferCreateInfo bufferCreateInfo = {};
        bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferCreateInfo.size = sizeBytes;
        bufferCreateInfo.usage = usage;
        bufferCreateInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
        bufferCreateInfo.pQueueFamilyIndices = queueFamilyIndices.data();
        bufferCreateInfo.queueFamilyIndexCount = static_cast<uint32_t>(queueFamilyIndices.size());

        return createBuffer(bufferCreateInfo, bufferMemoryUsage, pBufferName);
    }

    MemoryAllocator::Buffer MemoryAllocator::createBuffer(
        const VkBufferCreateInfo& bufferCreateInfo,
        VmaMemoryUsage bufferMemoryUsage,
        const char* pBufferName)
    {
        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = bufferMemoryUsage;
        if (pBufferName != nullptr) {
            allocInfo.flags = VMA_ALLOCATION_CREATE_USER_DATA_COPY_STRING_BIT;
            allocInfo.pUserData = const_cast<char*>(pBufferName);
        }

        Buffer handle;
        VkResult result = vmaCreateBuffer(
            m_allocator,
            &bufferCreateInfo,
            &allocInfo,
            &handle.handle,
            &handle.allocation,
            nullptr);

        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate buffer via vmaCreateBuffer!");
        }

        return handle;
    }

    void MemoryAllocator::destroyBuffer(Buffer& bufferAllocation)
    {
        vmaDestroyBuffer(m_allocator, bufferAllocation.handle, bufferAllocation.allocation);
        bufferAllocation.handle = VK_NULL_HANDLE;
        bufferAllocation.allocation = VK_NULL_HANDLE;
    }

    bool MemoryAllocator::mapBuffer(Buffer& handle, void** ppData)
    {
        VkResult result = vmaMapMemory(m_allocator, handle.allocation, ppData);
        return result == VK_SUCCESS;
    }

    void MemoryAllocator::unmapBuffer(Buffer& handle)
    {
        vmaUnmapMemory(m_allocator, handle.allocation);
    }

    MemoryAllocator::Image MemoryAllocator::createImage(
        const VkImageCreateInfo& imageCreateInfo,
        VmaMemoryUsage imageMemoryUsage)
    {
        VmaAllocationCreateInfo allocationCreateInfo = {};
        allocationCreateInfo.usage = imageMemoryUsage;

        Image image;
        VkResult result =
            vmaCreateImage(
                m_allocator,
                &imageCreateInfo,
                &allocationCreateInfo,
                &image.handle,
                &image.allocation,
                nullptr);

        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image via vmaCreateImage.");
        }

        return image;
    }

    void MemoryAllocator::destroyImage(Image& image)
    {
        vmaDestroyImage(m_allocator, image.handle, image.allocation);
        image.handle = VK_NULL_HANDLE;
        image.allocation = VK_NULL_HANDLE;
    }
}
