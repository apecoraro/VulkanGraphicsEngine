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

    class Presenter
    {
    protected:
        friend class Context;
        virtual void createVulkanSurface(VkInstance instance, const VkAllocationCallbacks* pAllocationCallbacks) = 0;
        virtual void checkIsDeviceSuitable(VkPhysicalDevice device) const = 0;
        virtual bool presentQueueIsRequired() const { return false; }
        virtual bool queueFamilySupportsPresent(VkPhysicalDevice device, uint32_t familyIndex) const { return true; }
        virtual void configureForDevice(VkPhysicalDevice device) = 0;

    public:
        virtual void initSwapChain(Renderer& renderer) = 0;
        virtual const RenderTarget& acquireSwapChainImageForRendering(VkCommandBuffer commandBuffer) = 0;
        virtual void transitionSwapChainImageForPresent(VkCommandBuffer commandBuffer) {};
        virtual VkResult present(Renderer& renderer) = 0;
    };

    class SwapChainPresenter : public Presenter
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
        SwapChainPresenter(
            const SwapChain::Config& swapChainConfig,
            void* pWindow,
            CreateVulkanSurfaceFunc createVulkanSurfaceFunc,
            ChooseImageCountFunc chooseImageCountFunc = nullptr,
            ChooseImageExtentFunc chooseWindowExtentFunc = nullptr,
            ChooseSurfaceFormatFunc chooseSurfaceFormatFunc = nullptr,
            ChoosePresentModeFunc choosePresentModeFunc = nullptr)
            : m_swapChainConfig(swapChainConfig)
            , m_pWindow(pWindow)
            , m_createVulkanSurfaceFunc(createVulkanSurfaceFunc)
            , m_chooseImageCountFunc(chooseImageCountFunc)
            , m_chooseWindowExtentFunc(chooseWindowExtentFunc)
            , m_chooseSurfaceFormatFunc(chooseSurfaceFormatFunc)
            , m_choosePresentModeFunc(choosePresentModeFunc)
        {
        }

        void createVulkanSurface(VkInstance instance, const VkAllocationCallbacks* pAllocationCallbacks) {
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

        bool presentQueueIsRequired() const override { return true; }

        bool queueFamilySupportsPresent(VkPhysicalDevice device, uint32_t familyIndex) const override;

        void configureForDevice(VkPhysicalDevice device) override;

        const SwapChain::Config& getSwapChainConfig() const { return m_swapChainConfig; }
        SwapChain::Config& getSwapChainConfig() { return m_swapChainConfig; }

        void* getWindow() { return m_pWindow; }

        void initSwapChain(Renderer& renderer) override;

        SwapChain& getSwapChain() { return *m_spSwapChain.get(); }
        const SwapChain& getSwapChain() const { return *m_spSwapChain.get(); }

        VkResult present(Renderer& renderer);

        void resizeWindow(uint32_t width, uint32_t height, Renderer& renderer);

        const RenderTarget& acquireSwapChainImageForRendering(VkCommandBuffer commandBuffer) override;
        void transitionSwapChainImageForPresent(VkCommandBuffer commandBuffer) override;

    private:
        const Image& getSwapChainImage(size_t frameIndex)
        {
            size_t swapChainImageIndex = frameIndex % m_spSwapChain->getImageCount();
            return m_spSwapChain->getImage(swapChainImageIndex);
        }

        VkResult acquireNextSwapChainImage(uint32_t* pSwapChainImageIndex);

        SwapChain::Config m_swapChainConfig;
        VkSurfaceKHR m_surface = VK_NULL_HANDLE;
        void* m_pWindow = nullptr;
        CreateVulkanSurfaceFunc m_createVulkanSurfaceFunc = nullptr;
        VkDevice m_logicalDevice = VK_NULL_HANDLE;
        std::unique_ptr<SwapChain> m_spSwapChain;
        std::vector<RenderTarget> m_swapChainRenderTargets;
		std::vector<std::unique_ptr<DepthStencilBuffer>> m_swapChainDepthStencilBuffers;
        uint32_t m_curSwapChainImageIndex = 0u; // most recently acquired swap chain

        ChooseImageCountFunc m_chooseImageCountFunc = nullptr;
        ChooseImageExtentFunc m_chooseWindowExtentFunc = nullptr;
        ChooseSurfaceFormatFunc m_chooseSurfaceFormatFunc = nullptr;
        ChoosePresentModeFunc m_choosePresentModeFunc = nullptr;

        Renderer* pRenderer;

        std::vector<std::unique_ptr<Semaphore>> m_renderFinishedSemaphores;
        std::vector<std::unique_ptr<Fence>> m_inFlightFences;
        uint32_t m_syncObjIndex = 0u;
    };

    class Renderer
    {
    protected:
        Context& m_context;
        Presenter& m_presenter;

    public:
        Renderer(Context& context, Presenter& presenter) : m_context(context), m_presenter(presenter) { }
        virtual ~Renderer() = default;

        Presenter& getPresenter() { return m_presenter;  }
        const Presenter& getPresenter() const { return m_presenter;  }

        const Camera& getCamera() const { return *m_spCamera.get(); }
        Camera& getCamera() { return *m_spCamera.get(); }

        Context& getContext() { return m_context; }

        CommandBufferFactory& getCommandBufferFactory() { return *m_spCommandBufferFactory.get(); }

        void initGraphicsResources(uint32_t renderTargetWidth, uint32_t renderTargetHeight, uint32_t frameBufferingCount);
        void resizeRenderTargetResources(uint32_t width, uint32_t height, uint32_t frameBufferingCount);

        virtual void buildPipelines(Object& object, DrawContext& drawContext);
        virtual void createImageSamplers(Drawable& drawable);

        // Records command buffer(s) to draw the scene in its current state.
        virtual void renderFrame(SceneNode& scene);
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
            VkFence fence = VK_NULL_HANDLE;
        };


        void addCommandBufferToSubmitInfo(VkCommandBuffer commandBuffer)
        {
            m_queueSubmitInfo.commandBuffers.push_back(commandBuffer);
        }

        QueueSubmitInfo& getSubmitInfo() { return m_queueSubmitInfo; }
        void submitGraphicsCommands();

    protected:
        virtual const RenderTarget& prepareRenderTarget(VkCommandBuffer commandBuffer)
        {
            return m_presenter.acquireSwapChainImageForRendering(commandBuffer);
        }

        virtual void postDrawScene(VkCommandBuffer commandBuffer)
        {
            return m_presenter.transitionSwapChainImageForPresent(commandBuffer);
        }

        std::unique_ptr<Camera> m_spCamera;

        std::vector<std::unique_ptr<Pipeline>> m_pipelines;

        void addPipeline(std::unique_ptr<Pipeline>&& spPipeline)
        {
            m_pipelines.emplace_back(std::move(spPipeline));
        }

        void createResourcePools(uint32_t framesInFlightPlusOne);
        void createCamera(uint32_t renderTargetWidth, uint32_t renderTargetHeight, uint32_t frameBufferingCount);

    private:
        uint32_t m_frameBufferingCount = 1u;

        size_t m_frameIndex = 0u;
        std::vector<std::unique_ptr<DescriptorPool>> m_descriptorPools;
        std::unique_ptr<CommandBufferFactory> m_spCommandBufferFactory;
        std::vector<VkCommandBuffer> m_commandBuffers;

        std::vector<std::unique_ptr<Buffer>> m_lightsBuffers;

        QueueSubmitInfo m_queueSubmitInfo;
    };
}

