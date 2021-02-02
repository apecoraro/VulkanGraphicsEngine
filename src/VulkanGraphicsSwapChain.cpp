#include "VulkanGraphicsSwapChain.h"

#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <vector>

namespace vgfx
{
    WindowSwapChain::WindowSwapChain(
        void* pWindow,
        uint32_t windowWidth,
        uint32_t windowHeight,
        const CreateVulkanSurfaceFunc& createVulkanSurfaceFunc)
        : m_pWindow(pWindow)
        , m_initWindowWidth(windowWidth)
        , m_initWindowHeight(windowHeight)
        , m_createVulkanSurfaceFunc(createVulkanSurfaceFunc)
    {
    }

    void WindowSwapChain::createVulkanSurface(
        VkInstance instance,
        const VkAllocationCallbacks* pAllocationCallbacks)
    {
        VkResult result = m_createVulkanSurfaceFunc(
            instance,
            m_pWindow,
            pAllocationCallbacks,
            &m_surface);

        if (result != VK_SUCCESS) {
            throw std::runtime_error("Creation of Vulkan surface failed");
        }
	}

    void WindowSwapChain::getImageCapabilities(
        VkPhysicalDevice device,
        uint32_t* pMinImageCount,
        uint32_t* pMaxImageCount,
        VkExtent2D* pMinImageExtent,
        VkExtent2D* pMaxImageExtent,
        VkExtent2D* pCurImageExtent,
        VkSurfaceTransformFlagsKHR* pSupportedTransforms,
        VkSurfaceTransformFlagBitsKHR* pCurrentTransform,
        VkCompositeAlphaFlagsKHR* pSupportedCompositeAlpha,
        VkImageUsageFlags* pSupportedUsageFlags) const
    {
        VkSurfaceCapabilitiesKHR surfaceCaps;
        VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &surfaceCaps);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
        }

        *pMinImageCount = surfaceCaps.minImageCount;
        *pMaxImageCount = surfaceCaps.maxImageCount;

        *pMinImageExtent = surfaceCaps.minImageExtent;
        *pMaxImageExtent = surfaceCaps.maxImageExtent;
        *pCurImageExtent = surfaceCaps.currentExtent;

        *pSupportedTransforms = surfaceCaps.supportedTransforms;
        *pCurrentTransform = surfaceCaps.currentTransform;

        *pSupportedCompositeAlpha = surfaceCaps.supportedCompositeAlpha;

        *pSupportedUsageFlags = surfaceCaps.supportedUsageFlags;
    }

    void WindowSwapChain::getSupportedImageFormats(
        VkPhysicalDevice device,
        std::vector<VkSurfaceFormatKHR>* pSupportedImageFormats) const
    {
        uint32_t formatCount = 0;
        VkResult result = vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, nullptr);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed vkGetPhysicalDeviceSurfaceFormatsKHR");
        }

        pSupportedImageFormats->resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, pSupportedImageFormats->data());
    }

    void WindowSwapChain::getSupportedPresentationModes(
        VkPhysicalDevice device,
        std::vector<VkPresentModeKHR>* pSupportedPresentationModes) const
    {
        uint32_t presentModeCount = 0u;
        VkResult result = vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, nullptr);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed vkGetPhysicalDeviceSurfacePresentModesKHR");
        }

        pSupportedPresentationModes->resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, pSupportedPresentationModes->data());
    } 

    bool WindowSwapChain::surfaceSupportsQueueFamily(VkPhysicalDevice device, uint32_t queueFamilyIndex) const
    {
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, queueFamilyIndex, m_surface, &presentSupport);
        return presentSupport != 0;
    }

    void WindowSwapChain::createRenderingResources(
        Context& context,
        const Config& config,
        const SwapChain::CreateImageViewFunc& createImageViewFunc)
    {
        VkSwapchainCreateInfoKHR createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = m_surface;

        createInfo.minImageCount = config.imageCount;
        createInfo.imageFormat = config.imageFormat.format;
        createInfo.imageColorSpace = config.imageFormat.colorSpace;
        createInfo.imageExtent = config.imageExtent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = config.imageUsage;

        uint32_t graphicsQueueFamilyIndex = context.getGraphicsQueueFamilyIndex().value();
        uint32_t presentQueueFamilyIndex = context.getPresentQueueFamilyIndex().value();

        uint32_t queueFamilyIndices[] = { graphicsQueueFamilyIndex, presentQueueFamilyIndex };
        if (graphicsQueueFamilyIndex != presentQueueFamilyIndex) {
            // TODO setting this to EXCLUSIVE aids performance but requires explicit ownership transfer.
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        } else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0; // Optional
            createInfo.pQueueFamilyIndices = nullptr; // Optional
        }

        createInfo.preTransform = config.preTransform;

        createInfo.compositeAlpha = config.compositeAlphaMode;

        createInfo.presentMode = config.presentMode;
        createInfo.clipped = VK_TRUE;

        if (m_swapChain != VK_NULL_HANDLE) {
            createInfo.oldSwapchain = m_swapChain;
            destroy();
        } else {
            createInfo.oldSwapchain = VK_NULL_HANDLE;
        }

        VkDevice device = context.getLogicalDevice();
        VkAllocationCallbacks* pAllocationCallbacks = context.getAllocationCallbacks();
        VkResult result = vkCreateSwapchainKHR(device, &createInfo, pAllocationCallbacks, &m_swapChain);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to create swap chain!");
        }

        m_pContext = &context;

        uint32_t actualImageCount = 0u;
        vkGetSwapchainImagesKHR(device, m_swapChain, &actualImageCount, nullptr);

        m_images.resize(actualImageCount);
        vkGetSwapchainImagesKHR(device, m_swapChain, &actualImageCount, m_images.data());

        m_imageExtent = config.imageExtent;

        m_imageFormat = config.imageFormat.format;

        m_imageViews.resize(m_images.size());

        for (size_t imageViewIndex = 0; imageViewIndex < m_imageViews.size(); ++imageViewIndex) {
            m_imageViews[imageViewIndex] =
                createImageViewFunc(
                    context,
                    m_images[imageViewIndex],
                    createInfo.imageFormat);
        }

        uint32_t maxFramesInFlight = std::min(actualImageCount, config.maxFramesInFlight);
        m_imageAvailableSemaphores.reserve(maxFramesInFlight);
        for (uint32_t i = 0; i < maxFramesInFlight; ++i) {
            m_imageAvailableSemaphores.push_back(std::make_unique<Semaphore>(*m_pContext));
        }
    }

    void WindowSwapChain::destroy()
    {
        if (m_swapChain != VK_NULL_HANDLE) {
            VkDevice device = m_pContext->getLogicalDevice();
            assert(device != VK_NULL_HANDLE && "Memory leak in WindowSwapChain!");

            vkDestroySwapchainKHR(device, m_swapChain, m_pContext->getAllocationCallbacks());
            m_swapChain = VK_NULL_HANDLE;

            m_images.clear();
            m_imageViews.clear();

            m_pContext = nullptr;
        }
    }
}