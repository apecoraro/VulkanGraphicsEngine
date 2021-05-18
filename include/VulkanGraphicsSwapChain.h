#ifndef VGFX_SWAP_CHAIN_H
#define VGFX_SWAP_CHAIN_H

#include "VulkanGraphicsContext.h"
#include "VulkanGraphicsImage.h"
#include "VulkanGraphicsImageView.h"
#include "VulkanGraphicsSemaphore.h"
#include "VulkanGraphicsRenderTarget.h"

#include <cstdint>
#include <functional>
#include <limits>
#include <vulkan/vulkan.h>

namespace vgfx
{
    class SwapChain
    {
    public:
        SwapChain() = default;
        virtual ~SwapChain() = default;

        uint32_t getImageCount() const {
            return static_cast<uint32_t>(m_images.size());
        }

        Image& getImage(size_t index) {
            return m_images[index];
        }

        const Image& getImage(size_t index) const {
            return m_images[index];
        }

        const VkExtent2D& getImageExtent() const {
            return m_imageExtent;
        }

        VkFormat getImageFormat() const {
            return m_imageFormat;
        }

        VkSampleCountFlagBits getSampleCount() const {
            return m_sampleCount;
        }

        using CreateImageViewFunc =
            std::function<std::unique_ptr<ImageView>(
                Context& context, VkImage image, VkFormat imageFormat)>;

        const ImageView& getImageView(size_t index) const
        {
            return *m_imageViews[index].get();
        }

        ImageView& getImageView(size_t index)
        {
            return *m_imageViews[index].get();
        }

    protected:
        std::vector<Image> m_images;
        VkExtent2D m_imageExtent = { 0u, 0u };
        VkFormat m_imageFormat = VK_FORMAT_UNDEFINED;
        VkSampleCountFlagBits m_sampleCount = VK_SAMPLE_COUNT_1_BIT;
        std::vector<std::unique_ptr<ImageView>> m_imageViews; 
    };

    class WindowSwapChain : public SwapChain
    {
    public:
        using CreateVulkanSurfaceFunc = std::function<VkResult(VkInstance, void*, const VkAllocationCallbacks*, VkSurfaceKHR*)>;

        WindowSwapChain(
            void* pWindow,
            uint32_t initWindowWidth,
            uint32_t initWindowHeight,
            const CreateVulkanSurfaceFunc& createVulkanSurfaceFunc);

        virtual ~WindowSwapChain()
        {
            destroy();
        }

        void createVulkanSurface(VkInstance instance, const VkAllocationCallbacks* pAllocationCallbacks);

        void getWindowInitialSize(uint32_t* pInitialWindowWidth, uint32_t* pInitialWindowHeight) const
        {
            *pInitialWindowWidth = m_initWindowWidth;
            *pInitialWindowHeight = m_initWindowHeight;
        }

        void getImageCapabilities(
            VkPhysicalDevice device,
            uint32_t* pMinImageCount,
            uint32_t* pMaxImageCount,
            VkExtent2D* pMinImageExtent,
            VkExtent2D* pMaxImageExtent,
            VkExtent2D* pCurImageExtent,
            VkSurfaceTransformFlagsKHR* pSupportedTransforms,
            VkSurfaceTransformFlagBitsKHR* pCurrentTransform,
            VkCompositeAlphaFlagsKHR* pSupportedCompositeAlpha,
            VkImageUsageFlags* pSupportedUsageFlags) const;

        void getSupportedImageFormats(
            VkPhysicalDevice device,
            std::vector<VkSurfaceFormatKHR>* pSupportedImageFormats) const;

        void getSupportedPresentationModes(
            VkPhysicalDevice device,
            std::vector<VkPresentModeKHR>* pSupportedPresentationModes) const;

        bool surfaceSupportsQueueFamily(VkPhysicalDevice device, uint32_t queueFamilyIndex) const;

        struct Config
        {
            uint32_t imageCount;
            uint32_t maxFramesInFlight;
            VkSurfaceFormatKHR imageFormat;
            VkExtent2D imageExtent;
            VkImageUsageFlags imageUsage;
            VkCompositeAlphaFlagBitsKHR compositeAlphaMode;
            VkPresentModeKHR presentMode;
            VkSurfaceTransformFlagBitsKHR preTransform;

            Config(
                uint32_t ic,
                uint32_t mfif,
                VkSurfaceFormatKHR isf,
                VkExtent2D ie,
                VkImageUsageFlags iu,
                VkCompositeAlphaFlagBitsKHR cam,
                VkPresentModeKHR pm,
                VkSurfaceTransformFlagBitsKHR pt)
                : imageCount(ic)
                , maxFramesInFlight(mfif)
                , imageFormat(isf)
                , imageExtent(ie)
                , imageUsage(iu)
                , compositeAlphaMode(cam)
                , presentMode(pm)
                , preTransform(pt)
            {
            }
            Config() = delete;
        };
        void createRenderingResources(
            Context& context,
            const Config& config,
            const SwapChain::CreateImageViewFunc& createImageViewFunc);

        void destroy();

        size_t getImageAvailableSemaphoreCount() const {
            return m_imageAvailableSemaphores.size();
        }

        VkSemaphore getImageAvailableSemaphore(uint32_t index) {
            return m_imageAvailableSemaphores[index]->getHandle();
        }

        VkSwapchainKHR getHandle() const { return m_swapChain; }
    private:
        void* m_pWindow = nullptr;
        uint32_t m_initWindowWidth = 0u;
        uint32_t m_initWindowHeight = 0u;

        CreateVulkanSurfaceFunc m_createVulkanSurfaceFunc = nullptr;
        VkSurfaceKHR m_surface = VK_NULL_HANDLE;

        VkSwapchainKHR m_swapChain = VK_NULL_HANDLE;

        Context* m_pContext = nullptr;
        std::vector<std::unique_ptr<Semaphore>> m_imageAvailableSemaphores;
    };

    class OffscreenSwapChain : public SwapChain
    {
    public:
        OffscreenSwapChain(
            std::vector<std::unique_ptr<Image>>&& images,
            std::vector<std::unique_ptr<ImageView>>&& imageViews);

    private:
        std::vector<std::unique_ptr<Image>> m_imagePtrs;
    };
}

#endif

