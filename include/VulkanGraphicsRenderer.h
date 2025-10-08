#pragma once

#include "VulkanGraphicsBuffer.h"
#include "VulkanGraphicsCamera.h"
#include "VulkanGraphicsContext.h"
#include "VulkanGraphicsCommandBufferFactory.h"
#include "VulkanGraphicsDepthStencilBuffer.h"
#include "VulkanGraphicsFence.h"
#include "VulkanGraphicsObject.h"
#include "VulkanGraphicsPipeline.h"
#include "VulkanGraphicsRenderTarget.h"
#include "VulkanGraphicsSceneNode.h"
#include "VulkanGraphicsSwapChain.h"

#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <vulkan/vulkan.h>

#include <cassert>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <vector>

namespace vgfx
{
    struct ViewState
    {
        glm::mat4 cameraViewMatrix;
        glm::mat4 cameraProjectionMatrix;
        Buffer* pCameraProjectionBuffer;
        VkViewport viewport;
        Pipeline::RasterizerConfig rasterizerConfig;
    };

    struct LightState
    {
        glm::vec4 position;
        glm::vec3 color;
        float radius;
    };

    struct SceneState
    {
        std::vector<ViewState> views;
        std::vector<LightState> lights;
        Buffer* pLightsBuffer;
    };

    struct DrawContext
    {
        Context& context;
        DescriptorPool& descriptorPool;
        size_t frameIndex;
        bool depthBufferEnabled;
        VkCommandBuffer commandBuffer;
        const RenderTarget& renderTarget;
        SceneState sceneState = {};

        void pushLight(
            const glm::vec4& position,
            const glm::vec3& color,
            float radius)
        {
            this->sceneState.lights.push_back({
                .position = position,
                .color = color,
                .radius = radius });
        }

        void popLight()
        {
            this->sceneState.lights.pop_back();
        }

        void pushView(
            const glm::mat4& view,
            const glm::mat4& proj,
            Buffer* pCameraProjectionBuffer,
            const VkViewport& viewport,
            const Pipeline::RasterizerConfig& rasterizerConfig)
        {
            this->sceneState.views.push_back({
                .cameraViewMatrix = view,
                .cameraProjectionMatrix = proj,
                .pCameraProjectionBuffer = pCameraProjectionBuffer,
                .viewport = viewport,
                .rasterizerConfig = rasterizerConfig });
        }

        void popView()
        {
            this->sceneState.views.pop_back();
        }
    };

    class Renderer
    {
    protected:
        friend class Context;
        // Called by vgfx::Context after the VkInstance has been created.
        virtual void bindToInstance(VkInstance instance, const VkAllocationCallbacks* pAllocationCallbacks) = 0;

        // Called by vgfx::Context to validate that a particular device will work for this Renderer.
        virtual void checkIsDeviceSuitable(VkPhysicalDevice device) const = 0;

        // Sub class should implement this function if it requires a queue that supports present
        // to a surface (see WindowRenderer for example).
        virtual bool queueFamilySupportsPresent(VkPhysicalDevice device, uint32_t familyIndex) const { return true;  }

        // Callback by vgfx::Context after a physical device has been selected. Allows this Renderer
        // to setup its configuration based on the capabilities of the device.
        virtual void configureForDevice(VkPhysicalDevice device) = 0;

        virtual std::unique_ptr<Camera> createCamera(uint32_t frameBufferingCount) = 0;

        Context& m_context;

    public:

        Renderer(Context& context, bool requiresPresentQueue) : m_context(context), m_requiresPresentQueue(requiresPresentQueue) {}
        virtual ~Renderer() = default;

        // Returns true if this Renderer requires use of a presentation queue.
        bool requiresPresentQueue() const { return m_requiresPresentQueue; }

        const Camera& getCamera() const { return *m_spCamera.get(); }
        Camera& getCamera() { return *m_spCamera.get(); }

        DepthStencilBuffer* getDepthStencilBuffer() { return m_spDepthStencilBuffer.get(); }
        const DepthStencilBuffer* getDepthStencilBuffer() const { return m_spDepthStencilBuffer.get(); }

        void setDepthStencilBuffer(std::unique_ptr<DepthStencilBuffer>& spNewBuffer) { return spNewBuffer.swap(m_spDepthStencilBuffer); }

        Context& getContext() { return m_context; }

        void initGraphicsResources(uint32_t frameBufferingCount);

        // Records command buffer(s) to draw the scene in its current state.
        VkResult renderFrame(SceneNode& scene);

        // Called by drawScene() right before scene.draw(...)
        virtual size_t preDrawScene(VkCommandBuffer commandBuffer, size_t frameIndex) = 0;
        // Called by drawScene() right after scene.draw(...)
        virtual void postDrawScene(VkCommandBuffer commandBuffer, size_t frameIndex) = 0;

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
        // Submit commands for rendering
        virtual VkResult endRenderFrame(QueueSubmitInfo& submitInfo, size_t bufferIndex) = 0;

    protected:
        virtual const RenderTarget& getRenderTarget(size_t frameIndex) = 0;

        std::unique_ptr<DepthStencilBuffer> m_spDepthStencilBuffer;
        std::unique_ptr<Camera> m_spCamera;

    private:
        bool m_requiresPresentQueue = false;

        uint32_t m_frameBufferingCount = 1u;
        QueueSubmitInfo m_gfxQueueSubmitInfo;

        size_t m_frameIndex = 0u;
        std::vector<std::unique_ptr<DescriptorPool>> m_descriptorPools;
        std::unique_ptr<CommandBufferFactory> m_spCommandBufferFactory;
        std::vector<VkCommandBuffer> m_commandBuffers;

        std::vector<std::unique_ptr<Buffer>> m_lightsBuffers;
    };

    class WindowRenderer : public Renderer
    {
    public:
        using CreateVulkanSurfaceFunc = std::function<VkResult(VkInstance, void*, const VkAllocationCallbacks*, VkSurfaceKHR*)>;
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
            Context& context,
            const SwapChain::Config& swapChainConfig,
            void* pWindow,
            CreateVulkanSurfaceFunc createVulkanSurfaceFunc,
            ChooseImageCountFunc chooseImageCountFunc = nullptr,
            ChooseImageExtentFunc chooseWindowExtentFunc = nullptr,
            ChooseSurfaceFormatFunc chooseSurfaceFormatFunc = nullptr,
            ChoosePresentModeFunc choosePresentModeFunc = nullptr)
            : Renderer(context, true) // requires present queue
            , m_swapChainConfig(swapChainConfig)
            , m_pWindow(pWindow)
            , m_createVulkanSurfaceFunc(createVulkanSurfaceFunc)
            , m_chooseImageCountFunc(chooseImageCountFunc)
            , m_chooseWindowExtentFunc(chooseWindowExtentFunc)
            , m_chooseSurfaceFormatFunc(chooseSurfaceFormatFunc)
            , m_choosePresentModeFunc(choosePresentModeFunc)
        {
        }

        void bindToInstance(VkInstance instance, const VkAllocationCallbacks* pAllocationCallbacks) override {
            VkResult result = m_createVulkanSurfaceFunc(
                instance,
                m_pWindow,
                pAllocationCallbacks,
                &m_surface);

            if (result != VK_SUCCESS) {
                throw std::runtime_error("Creation of Vulkan surface failed");
            }
        }

        void checkIsDeviceSuitable(VkPhysicalDevice device) const override;

        bool queueFamilySupportsPresent(VkPhysicalDevice device, uint32_t familyIndex) const override;

        void configureForDevice(VkPhysicalDevice device) override;

        const SwapChain::Config& getSwapChainConfig() const { return m_swapChainConfig; }

        void initSwapChain();

        std::unique_ptr<Camera> createCamera(uint32_t frameBufferingCount) override;

        SwapChain& getSwapChain() { return *m_spSwapChain.get(); }
        const SwapChain& getSwapChain() const { return *m_spSwapChain.get(); }

        VkResult endRenderFrame(QueueSubmitInfo& submitInfo, size_t bufferIndex);

        void resizeWindow(uint32_t width, uint32_t height);

    private:
        const RenderTarget& getRenderTarget(size_t frameIndex) override
        {
            size_t swapChainImageIndex = frameIndex % m_swapChainRenderTargets.size();
            return m_swapChainRenderTargets[swapChainImageIndex];
        }

        const Image& getSwapChainImage(size_t frameIndex)
        {
            size_t swapChainImageIndex = frameIndex % m_spSwapChain->getImageCount();
            return m_spSwapChain->getImage(swapChainImageIndex);
        }

        size_t preDrawScene(VkCommandBuffer commandBuffer, size_t frameIndex) override;
        void postDrawScene(VkCommandBuffer commandBuffer, size_t frameIndex) override;

        VkResult acquireNextSwapChainImage(uint32_t* pSwapChainImageIndex);

        VkSurfaceKHR m_surface = VK_NULL_HANDLE;
        SwapChain::Config m_swapChainConfig;
        void* m_pWindow = nullptr;
        CreateVulkanSurfaceFunc m_createVulkanSurfaceFunc = nullptr;
        std::unique_ptr<SwapChain> m_spSwapChain;
        std::vector<RenderTarget> m_swapChainRenderTargets;

        ChooseImageCountFunc m_chooseImageCountFunc = nullptr;
        ChooseImageExtentFunc m_chooseWindowExtentFunc = nullptr;
        ChooseSurfaceFormatFunc m_chooseSurfaceFormatFunc = nullptr;
        ChoosePresentModeFunc m_choosePresentModeFunc = nullptr;

        std::vector<std::unique_ptr<Semaphore>> m_renderFinishedSemaphores;
        std::vector<std::unique_ptr<Fence>> m_inFlightFences;
        uint32_t m_syncObjIndex = 0u;
    };
}

