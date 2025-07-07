#include "VulkanGraphicsSwapChain.h"

#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <vector>

namespace vgfx
{
    SwapChain::SwapChain(Context& context, VkSurfaceKHR surface, const Config& config)
        : m_context(context)
    {
        recreate(surface, config);
    }

    void SwapChain::recreate(VkSurfaceKHR surface, const Config& config)
    {
        VkSwapchainCreateInfoKHR createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = surface;

        createInfo.minImageCount = config.imageCount.value();
        createInfo.imageFormat = config.imageFormat.value().format;
        createInfo.imageColorSpace = config.imageFormat.value().colorSpace;
        createInfo.imageExtent = config.imageExtent.value();
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = config.imageUsage.value();

        uint32_t graphicsQueueFamilyIndex = m_context.getGraphicsQueueFamilyIndex();
        uint32_t presentQueueFamilyIndex = m_context.getPresentQueueFamilyIndex();

        uint32_t queueFamilyIndices[] = { graphicsQueueFamilyIndex, presentQueueFamilyIndex };
        //if (graphicsQueueFamilyIndex != presentQueueFamilyIndex) {
        //    // TODO setting this to EXCLUSIVE aids performance but requires explicit ownership transfer.
        //    createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        //    createInfo.queueFamilyIndexCount = 2;
        //    createInfo.pQueueFamilyIndices = queueFamilyIndices;
        //} else {
        // TODO should make sharing mode part of the configuration, also would need to add a check for sharing
        // mode in the renderFrame function before it decided whether to create barrier to transfer ownership
        // of the image from one queue to the other queue.
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0; // Optional
        createInfo.pQueueFamilyIndices = nullptr; // Optional
        //}

        createInfo.preTransform = config.preTransform.value();

        createInfo.compositeAlpha = config.compositeAlphaMode.value();

        createInfo.presentMode = config.presentMode.value();
        createInfo.clipped = VK_TRUE;

        createInfo.oldSwapchain = m_swapChain;

        VkDevice device = m_context.getLogicalDevice();
        VkAllocationCallbacks* pAllocationCallbacks = m_context.getAllocationCallbacks();
        VkResult result = vkCreateSwapchainKHR(device, &createInfo, pAllocationCallbacks, &m_swapChain);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to create swap chain!");
        }

        if (createInfo.oldSwapchain != VK_NULL_HANDLE) {
            destroy(createInfo.oldSwapchain);
        }

        uint32_t actualImageCount = 0u;
        vkGetSwapchainImagesKHR(device, m_swapChain, &actualImageCount, nullptr);

        std::vector<VkImage> imageHandles(actualImageCount);
        vkGetSwapchainImagesKHR(device, m_swapChain, &actualImageCount, imageHandles.data());

        m_images.reserve(actualImageCount);
        for (VkImage imageHandle : imageHandles) {
            Image::Config metadata(
                config.imageExtent.value().width,
                config.imageExtent.value().height,
                config.imageFormat.value().format,
                VK_IMAGE_TILING_OPTIMAL,
                config.imageUsage.value());
            m_images.emplace_back(std::make_unique<Image>(m_context, imageHandle, metadata));
        }

        m_imageExtent = config.imageExtent.value();

        m_imageViews.reserve(actualImageCount);
        for (size_t imageViewIndex = 0; imageViewIndex < actualImageCount; ++imageViewIndex) {
            ImageView::Config imageViewCfg(config.imageFormat.value().format, VK_IMAGE_VIEW_TYPE_2D);
            m_imageViews.push_back(&m_images[imageViewIndex]->getOrCreateView(imageViewCfg));
        }

        m_imageAvailableSemaphores.reserve(actualImageCount);
        for (uint32_t i = 0; i < actualImageCount; ++i) {
            m_imageAvailableSemaphores.emplace_back(std::make_unique<Semaphore>(m_context));
        }
    }

    void SwapChain::GetImageCapabilities(
        VkPhysicalDevice device,
        VkSurfaceKHR surface,
        uint32_t* pMinImageCount,
        uint32_t* pMaxImageCount,
        VkExtent2D* pMinImageExtent,
        VkExtent2D* pMaxImageExtent,
        VkExtent2D* pCurImageExtent,
        VkSurfaceTransformFlagsKHR* pSupportedTransforms,
        VkSurfaceTransformFlagBitsKHR* pCurrentTransform,
        VkCompositeAlphaFlagsKHR* pSupportedCompositeAlpha,
        VkImageUsageFlags* pSupportedUsageFlags)
    {
        VkSurfaceCapabilitiesKHR surfaceCaps;
        VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &surfaceCaps);
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

    void SwapChain::GetSupportedImageFormats(
        VkPhysicalDevice device,
        VkSurfaceKHR surface,
        std::vector<VkSurfaceFormatKHR>* pSupportedImageFormats)
    {
        uint32_t formatCount = 0;
        VkResult result = vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed vkGetPhysicalDeviceSurfaceFormatsKHR");
        }

        pSupportedImageFormats->resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, pSupportedImageFormats->data());
    }

    void SwapChain::GetSupportedPresentationModes(
        VkPhysicalDevice device,
        VkSurfaceKHR surface,
        std::vector<VkPresentModeKHR>* pSupportedPresentationModes)
    {
        uint32_t presentModeCount = 0u;
        VkResult result = vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed vkGetPhysicalDeviceSurfacePresentModesKHR");
        }

        pSupportedPresentationModes->resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, pSupportedPresentationModes->data());
    } 

    bool SwapChain::SurfaceSupportsQueueFamily(
        VkPhysicalDevice device,
        VkSurfaceKHR surface,
        uint32_t queueFamilyIndex)
    {
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, queueFamilyIndex, surface, &presentSupport);
        return presentSupport != 0;
    }

    void SwapChain::destroy(VkSwapchainKHR swapChain)
    {
        if (swapChain != VK_NULL_HANDLE) {
            VkDevice device = m_context.getLogicalDevice();
            assert(device != VK_NULL_HANDLE && "Memory leak in WindowSwapChain!");

            vkDestroySwapchainKHR(device, swapChain, m_context.getAllocationCallbacks());
            swapChain = VK_NULL_HANDLE;

            m_imageAvailableSemaphores.clear();
            m_imageViews.clear();
            m_images.clear();
        }
    } 
}