#ifndef QUANTUM_GFX_RENDERER_H
#define QUANTUM_GFX_RENDERER_H

#include "VulkanGraphicsContext.h"
#include "VulkanGraphicsCommandBuffers.h"
#include "VulkanGraphicsFence.h"
#include "VulkanGraphicsObject.h"
#include "VulkanGraphicsPipeline.h"
#include "VulkanGraphicsRenderTarget.h"
#include "VulkanGraphicsSwapChain.h"

#include <cassert>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <vulkan/vulkan.h>

namespace vgfx
{
    class Renderer
    {
    public:
        Renderer(bool requiresPresentQueue) : m_requiresPresentQueue(requiresPresentQueue) {}
        virtual ~Renderer() = default;

        // Called by vgfx::Context after the VkInstance has been created.
        virtual void bindToInstance(VkInstance instance, const VkAllocationCallbacks* pAllocationCallbacks) = 0;

        // Called by vgfx::Context to validate that a particular device will work for this Renderer.
        virtual void checkIsDeviceSuitable(VkPhysicalDevice device) const = 0;

        // Returns true if this Renderer requires use of a presentation queue.
        bool requiresPresentQueue() const { return m_requiresPresentQueue; }

        // Sub class should implement this function if it requires a queue that supports present
        // to a surface (see WindowRenderer).
        virtual bool queueFamilySupportsPresent(VkPhysicalDevice device, uint32_t familyIndex) const { return true;  }

        // Callback by vgfx::Context after a physical device has been selected. Allows this Renderer
        // to setup its configuration based on the capabilities of the device.
        virtual void configureForDevice(VkPhysicalDevice device) = 0;

        virtual void initSwapChain(
            Context& context,
            const SwapChain::CreateImageViewFunc& createImageViewFunc,
            uint32_t maxFramesInFlight,
            const vgfx::RenderTarget::Config& renderTargetConfig) = 0;

        virtual SwapChain& getSwapChain() = 0;
        virtual const SwapChain& getSwapChain() const = 0;

        // Record one command buffer for each swap chain image using
        // the provided set of objects.
        void recordCommandBuffers(
            CommandBufferFactory& commandBufferFactory,
            const Pipeline& pipeline,
            const std::vector<std::unique_ptr<Object>>& objects);

        // Acquire a swap chain image to render into outputs the index
        // of the image.
        virtual VkResult acquireNextSwapChainImage(uint32_t* pSwapChainImageIndex) = 0;

        struct QueueSubmitInfo
        {
            void addWait(VkSemaphore sem, VkPipelineStageFlags stage)
            {
                waitSemaphores.push_back(sem); 
                waitSemaphoreStages.push_back(stage);
            }

            void clearAll()
            {
                waitSemaphores.clear();
                waitSemaphoreStages.clear();
                commandBuffers.clear();
                signalSemaphores.clear();
            }
            // Should be same number of waitSemaphoreStages as waitSemaphores
            std::vector<VkSemaphore> waitSemaphores;
            std::vector<VkPipelineStageFlags> waitSemaphoreStages;

            std::vector<VkCommandBuffer> commandBuffers;

            std::vector<VkSemaphore> signalSemaphores;
        };
        // Use previously recorded command buffers to render to the
        // specified swap chain image as specified by the swapChainImageIndex.
        virtual VkResult renderFrame(
            uint32_t swapChainImageIndex,
            QueueSubmitInfo extraSubmitInfo) = 0;

    protected:
        virtual void recordCommandBuffer(
            VkCommandBuffer commandBuffer,
            size_t swapChainImageIndex,
            const Pipeline& pipeline,
            const std::vector<std::unique_ptr<Object>>& objects) = 0;

        bool m_requiresPresentQueue = false;
        Context* m_pContext = nullptr;

        uint32_t m_maxFramesInFlight = 0u;

        std::vector<VkCommandBuffer> m_commandBuffers;

        QueueSubmitInfo m_gfxQueueSubmitInfo;
    };

    class WindowRenderer : public Renderer
    {
    public:
        struct SwapChainConfig
        {
            // defaults to device's "min(minImageCount + 1, maxImageCount)" or by ChooseImageCountFunc.
            std::optional<uint32_t> imageCount;
            // if not specified then defaults determined by ChooseImageExtentFunc, if ChooseImageExtentFunc is not specified
            // then defaults to max(minExtent, min(maxExtent, curExtent)). If explicitly setting the imageExtent, then to specify
            // format without colorSpace, set colorSpace to VK_COLOR_SPACE_MAX_ENUM_KHR, to specify colorSpace without format,
            // set format to VK_FORMAT_UNDEFINED.
            std::optional<VkExtent2D> imageExtent;
            // If not specified then determined by ChooseSurfaceFormatFunc, if ChooseSurfaceFormatFunc is null,
            // then defaults to either VK_FORMAT_B8G8R8A8_SRGB and VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, unless it's not available,
            // in which case the first supported format, as returned by vkGetPhysicalDeviceSurfaceFormatsKHR is used.
            std::optional<VkSurfaceFormatKHR> imageFormat;
            // If not specified then defaults to VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT.
            std::optional<VkImageUsageFlags> imageUsage;
            // If not specified then defaults to VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR.
            std::optional< VkCompositeAlphaFlagBitsKHR> compositeAlphaMode;
            // If not specified then determined by ChoosePresentModeFunc, if ChoosePresentModeFunc is null, then defaults to
            // VK_PRESENT_MODE_FIFO_KHR.
            std::optional<VkPresentModeKHR> presentMode;
            // If not specified then defaults to the current transform as returned by vkGetPhysicalDeviceSurfaceCapabilitiesKHR
            std::optional<VkSurfaceTransformFlagBitsKHR> preTransform;
        };

        using ChooseImageCountFunc = std::function<uint32_t(uint32_t minImageCount, uint32_t maxImageCount)>;
        using ChooseImageExtentFunc = std::function<VkExtent2D(VkExtent2D minExtent, VkExtent2D maxExtent, VkExtent2D curExtent)>;
        using ChooseSurfaceFormatFunc = std::function<VkSurfaceFormatKHR(const std::vector<VkSurfaceFormatKHR>& supportedFormats)>;
        using ChoosePresentModeFunc = std::function<VkPresentModeKHR(const std::vector<VkPresentModeKHR>& supportedModes)>;
        // Use optional parameters of SwapChainConfig to validate that a particular Vulkan
        // physical device is "suitable". If the coresponding callback is not provided (i.e. nullptr)
        // then the value will default to the value provided in the SwapChainConfig. Thus if a value
        // is not specified in the SwapChainConfig then the corresponding callback function must 
        // not be nullptr.
        WindowRenderer(
            const SwapChainConfig& swapChainConfig,
            std::unique_ptr<WindowSwapChain> spSwapChain,
            const ChooseImageCountFunc& chooseImageCountFunc = nullptr,
            const ChooseImageExtentFunc& chooseWindowExtentFunc = nullptr,
            const ChooseSurfaceFormatFunc& chooseSurfaceFormatFunc = nullptr,
            const ChoosePresentModeFunc& choosePresentModeFunc = nullptr)
            : Renderer(true) // requires present queue
            , m_swapChainConfig(swapChainConfig)
            , m_spSwapChain(std::move(spSwapChain))
            , m_chooseImageCountFunc(chooseImageCountFunc)
            , m_chooseWindowExtentFunc(chooseWindowExtentFunc)
            , m_chooseSurfaceFormatFunc(chooseSurfaceFormatFunc)
            , m_choosePresentModeFunc(choosePresentModeFunc)
        {
        }

        void bindToInstance(VkInstance instance, const VkAllocationCallbacks* pAllocationCallbacks) override {
            m_spSwapChain->createVulkanSurface(instance, pAllocationCallbacks);
        }

        void checkIsDeviceSuitable(VkPhysicalDevice device) const override;

        bool queueFamilySupportsPresent(VkPhysicalDevice device, uint32_t familyIndex) const override;

        void configureForDevice(VkPhysicalDevice device) override;

        const SwapChainConfig& getSwapChainConfig() const { return m_swapChainConfig; }

        void initSwapChain(
            Context& context,
            const SwapChain::CreateImageViewFunc& createImageViewFunc,
            uint32_t maxFramesInFlight,
            const vgfx::RenderTarget::Config& renderTargetConfig) override;

        SwapChain& getSwapChain() override { return *m_spSwapChain.get(); }
        virtual const SwapChain& getSwapChain() const { return *m_spSwapChain.get(); }

        const RenderTarget* getRenderTarget() const { return m_spRenderTarget.get(); }

        VkResult acquireNextSwapChainImage(uint32_t* pSwapChainImageIndex) override;

        VkResult renderFrame(
            uint32_t swapChainImageIndex,
            QueueSubmitInfo extraSubmitInfo) override;

    private:
        void recordCommandBuffer(
            VkCommandBuffer commandBuffer,
            size_t swapChainImageIndex,
            const Pipeline& pipeline,
            const std::vector<std::unique_ptr<Object>>& objects) override;

        SwapChainConfig m_swapChainConfig;
        std::unique_ptr<WindowSwapChain> m_spSwapChain;
        std::unique_ptr<RenderTarget> m_spRenderTarget;

        ChooseImageCountFunc m_chooseImageCountFunc = nullptr;
        ChooseImageExtentFunc m_chooseWindowExtentFunc = nullptr;
        ChooseSurfaceFormatFunc m_chooseSurfaceFormatFunc = nullptr;
        ChoosePresentModeFunc m_choosePresentModeFunc = nullptr;

        std::vector<std::unique_ptr<Semaphore>> m_renderFinishedSemaphores;
        std::vector<std::unique_ptr<Fence>> m_inFlightFences;
        uint32_t m_syncObjIndex = 0u;
    };
}

#endif

