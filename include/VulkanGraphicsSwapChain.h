#pragma once

#include "VulkanGraphicsContext.h"
#include "VulkanGraphicsImage.h"
#include "VulkanGraphicsSemaphore.h"
#include "VulkanGraphicsRenderTarget.h"

#include <cstdint>
#include <functional>
#include <limits>
#include <vulkan/vulkan.h>

namespace vgfx
{
    // Encapsulates the VkSwapChain which is a queue images that can be presented to on the screen.
    class SwapChain
    {
    public:
        struct Config
        {
            // defaults to device's "min(minImageCount + 1, maxImageCount)" or by ChooseImageCountFunc.
            std::optional<uint32_t> imageCount;
            // if not specified then defaults determined by ChooseImageExtentFunc, if ChooseImageExtentFunc is not specified
            // then defaults to max(minExtent, min(maxExtent, curExtent)). If explicitly setting the imageExtent, then to specify
            // renderTargetFormat without colorSpace, set colorSpace to VK_COLOR_SPACE_MAX_ENUM_KHR, to specify colorSpace without renderTargetFormat,
            // set renderTargetFormat to VK_FORMAT_UNDEFINED.
            std::optional<VkExtent2D> imageExtent;
            // If not specified then determined by ChooseSurfaceFormatFunc, if ChooseSurfaceFormatFunc is null,
            // then defaults to either VK_FORMAT_B8G8R8A8_SRGB and VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, unless it's not available,
            // in which case the first supported renderTargetFormat, as returned by vkGetPhysicalDeviceSurfaceFormatsKHR is used.
            std::optional<VkSurfaceFormatKHR> imageFormat;
            // If not specified then defaults to VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT.
            std::optional<VkColorSpaceKHR> imageColorSpace;
            // If not specified then defaults to VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT.
            std::optional<VkImageUsageFlags> imageUsage;
            // If not specified then defaults to VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR.
            std::optional<VkCompositeAlphaFlagBitsKHR> compositeAlphaMode;
            // If not specified then determined by ChoosePresentModeFunc, if ChoosePresentModeFunc is null, then defaults to
            // VK_PRESENT_MODE_FIFO_KHR.
            std::optional<VkPresentModeKHR> presentMode;
            // If not specified then defaults to the current transform as returned by vkGetPhysicalDeviceSurfaceCapabilitiesKHR
            std::optional<VkSurfaceTransformFlagBitsKHR> preTransform;
            // Optionally specify a list of preferred formats in order of highest preference to lowest preference. The VkPhysicalDevice
            // will be queried to pick the most preferred format from formats supported by the device.
            std::optional<std::vector<VkFormat>> preferredDepthStencilFormats;
            // If no preferredDepthStencilFormats or explicit depthStencilFormat is specified then no depth stencil buffer is created.
            std::optional<VkFormat> depthStencilFormat;
        };

        SwapChain(Context& context, VkSurfaceKHR surface, const Config& config);

        virtual ~SwapChain()
        {
            destroy(m_swapChain);
        }

        // Using recreate instead of deleting and then creating a new SwapChain has some advantages,
        // for example, full screen exclusive mode can be transferred without unwanted visual effects.
        void recreate(VkSurfaceKHR surface, const Config& config);

        static void GetImageCapabilities(
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
            VkImageUsageFlags* pSupportedUsageFlags);

        static void GetSupportedImageFormats(
            VkPhysicalDevice device,
            VkSurfaceKHR surface,
            std::vector<VkSurfaceFormatKHR>* pSupportedImageFormats);

        static void GetSupportedPresentationModes(
            VkPhysicalDevice device,
            VkSurfaceKHR surface,
            std::vector<VkPresentModeKHR>* pSupportedPresentationModes);

        static bool SurfaceSupportsQueueFamily(
            VkPhysicalDevice device,
            VkSurfaceKHR surface,
            uint32_t queueFamilyIndex);

        size_t getImageAvailableSemaphoreCount() const {
            return m_imageAvailableSemaphores.size();
        }

        VkSemaphore getImageAvailableSemaphore(uint32_t index) {
            return m_imageAvailableSemaphores[index]->getHandle();
        }

        VkSwapchainKHR getHandle() const { return m_swapChain; }

        uint32_t getImageCount() const {
            return static_cast<uint32_t>(m_images.size());
        }

        Image& getImage(size_t index) {
            return *m_images[index].get();
        }

        const Image& getImage(size_t index) const {
            return *m_images[index].get();
        }

        const std::vector<std::unique_ptr<Image>>& getImages() const {
            return m_images;
        }

        ImageView& getImageView(size_t index) {
            return *m_imageViews[index];
        }

        const ImageView& getImageView(size_t index) const {
            return *m_imageViews[index];
        }

        std::vector<ImageView*>& getImageViews() { return m_imageViews; }
        const std::vector<ImageView*>& getImageViews() const { return m_imageViews; }

        const VkExtent2D& getImageExtent() const {
            return m_imageExtent;
        }

        VkFormat getImageFormat() const {
            return m_images[0]->getFormat();
        }

        VkSampleCountFlagBits getSampleCount() const {
            return m_images[0]->getSampleCount();
        }

    protected:
        void destroy(VkSwapchainKHR handle);

    private:
        Context& m_context;

        VkSwapchainKHR m_swapChain = VK_NULL_HANDLE;

        std::vector<std::unique_ptr<Image>> m_images;
        std::vector<ImageView*> m_imageViews;
        VkExtent2D m_imageExtent = { 0u, 0u };
        VkSampleCountFlagBits m_sampleCount = VK_SAMPLE_COUNT_1_BIT;

        std::vector<std::unique_ptr<Semaphore>> m_imageAvailableSemaphores;
    };
}

