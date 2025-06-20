#include "VulkanGraphicsEngineDemo.h"

#include "VulkanGraphicsApplication.h"

#include "VulkanGraphicsDescriptorPoolBuilder.h"
#include "VulkanGraphicsDescriptors.h"
#include "VulkanGraphicsDepthStencilBuffer.h"
#include "VulkanGraphicsImageDescriptorUpdaters.h"
#include "VulkanGraphicsImageSharpener.h"
#include "VulkanGraphicsImageView.h"
#include "VulkanGraphicsMaterials.h"
#include "VulkanGraphicsModelLibrary.h"
#include "VulkanGraphicsRenderTarget.h"
#include "VulkanGraphicsSampler.h"
#include "VulkanGraphicsSwapChain.h"
#include "VulkanGraphicsBuffer.h"


#include <vulkan/vulkan.h>

#include <algorithm>
#include <iostream>
#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>

const int MAX_FRAMES_IN_FLIGHT = 2;

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugReportFlagsEXT flags,
    VkDebugReportObjectTypeEXT objType,
    uint64_t obj,
    size_t location,
    int32_t code,
    const char* layerPrefix,
    const char* msg,
    void* userData)
{
    std::cerr << "Validation layer: " << msg << std::endl;

    return VK_FALSE;
}

class Demo
{
public:
    Demo() = default;
    ~Demo() = default;
    Demo(bool enableValidationLayers) :
        m_enableValidationLayers(enableValidationLayers)
    {
    }

    std::unique_ptr<vgfx::WindowApplication> m_spApplication;
	
    void init(
        void* window,
        const std::function<VkResult(VkInstance, void*, const VkAllocationCallbacks*, VkSurfaceKHR*)>& createVulkanSurface,
        const std::function<void(void*, int*, int*)>& getFramebufferSize,
        uint32_t platformSpecificExtensionCount,
        const char** platformSpecificExtensions,
        const std::string& dataPath,
        const std::string& modelPath,
        const std::string& modelDiffuseTexName,
        const std::string& vertexShader,
        const std::string& fragmentShader)
    {
        vgfx::Context::AppConfig appConfig("Demo", 1, 0, 0, dataPath);

        vgfx::Context::InstanceConfig instanceConfig;
        instanceConfig.requiredExtensions.insert(
            instanceConfig.requiredExtensions.end(),
            platformSpecificExtensions,
            platformSpecificExtensions + platformSpecificExtensionCount);

        if (m_enableValidationLayers) {
            instanceConfig.requiredExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
            instanceConfig.validationLayers.push_back("VK_LAYER_KHRONOS_validation");
        }

        vgfx::Context::DeviceConfig deviceConfig;
        deviceConfig.graphicsQueueRequired = true;
        deviceConfig.requiredDeviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

        int32_t windowWidth, windowHeight;
        // Current size of window is used as default if SwapChainConfig::imageExtent is not specified.
        getFramebufferSize(window, &windowWidth, &windowHeight);

        m_spApplication = std::make_unique<vgfx::WindowApplication>(
            appConfig,
            instanceConfig,
            deviceConfig,
            vgfx::WindowApplication::CreateSwapChainConfig(windowWidth, windowHeight),
            window,
            createVulkanSurface);

        // Render to half size offscreen image then use image sharpener to upscale
        vgfx::Image::Config offscreenImageCfg(
            windowWidth,
            windowHeight,
            VK_FORMAT_R16G16B16A16_SFLOAT,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
            | VK_IMAGE_USAGE_STORAGE_BIT
            | VK_IMAGE_USAGE_SAMPLED_BIT);

        std::vector<std::unique_ptr<vgfx::Image>> offscreenImages;
        offscreenImages.push_back(std::make_unique<vgfx::Image>(m_spApplication->getContext(), offscreenImageCfg));
        offscreenImages.push_back(std::make_unique<vgfx::Image>(m_spApplication->getContext(), offscreenImageCfg));
        offscreenImages.push_back(std::make_unique<vgfx::Image>(m_spApplication->getContext(), offscreenImageCfg));

        vgfx::ImageView::Config offscreenImageViewCfg(
            VK_FORMAT_R16G16B16A16_SFLOAT,
            VK_IMAGE_VIEW_TYPE_2D);

        std::vector<std::unique_ptr<vgfx::ImageView>> offscreenImageViews;
        offscreenImageViews.push_back(
            std::make_unique<vgfx::ImageView>(
                m_spApplication->getContext(), offscreenImageViewCfg, *offscreenImages[0].get()));
        offscreenImageViews.push_back(
            std::make_unique<vgfx::ImageView>(
                m_spApplication->getContext(), offscreenImageViewCfg, *offscreenImages[1].get()));
        offscreenImageViews.push_back(
            std::make_unique<vgfx::ImageView>(
                m_spApplication->getContext(), offscreenImageViewCfg, *offscreenImages[2].get()));

        m_spOffscreenSwapChain =
            std::make_unique<vgfx::SwapChain>(std::move(offscreenImages));

        vgfx::RenderTargetBuilder renderTargetBuilder;
        renderTargetBuilder.addAttachmentChain(offscreenImages, m_spApplication->getRenderer().getDepthStencilBuffer());

        m_spOffscreenRenderTarget =
            renderTargetBuilder.createRenderTarget(m_spApplication->getContext());

        m_spImageSharpener =
            std::make_unique<vgfx::ImageSharpener>(
                m_spApplication->getContext(),
                m_spApplication->getRenderer().getSwapChain().getImageCount(),
                1.0f); // sharpening value

        m_spCommandBufferFactory =
            std::make_unique<vgfx::CommandBufferFactory>(
                m_spApplication->getContext(),
                m_spApplication->getContext().getGraphicsQueueFamilyIndex().value());

        std::string vertexShaderEntryPointFunc = "main";
        std::string fragmentShaderEntryPointFunc = "main";

        // Create MaterialInfo which is used to create an instance of the material for a particular
        // model by the ModelLibrary.
        vgfx::MaterialsLibrary::MaterialInfo materialInfo(
            vertexShader,
            vertexShaderEntryPointFunc,
            vgfx::VertexXyzRgbUvN::GetConfig().vertexAttrDescriptions, // TODO should use reflection for this.
            fragmentShader,
            fragmentShaderEntryPointFunc);

        vgfx::Material& material = vgfx::MaterialsLibrary::GetOrLoadMaterial(m_spApplication->getContext(), materialInfo);

        // Allocate buffer for camera parameters
        // Allocate descriptor set for camera parameters
        // Update descriptor set to point at the buffer
        // Each frame map the buffer and write the new camera data

        m_spModelLibrary = std::make_unique<vgfx::ModelLibrary>();

        vgfx::ModelLibrary::Model model;
        model.modelPathOrShapeName = modelPath;
        if (!modelDiffuseTexName.empty()) {
            model.imageOverrides[vgfx::Material::ImageType::Diffuse] = modelDiffuseTexName;
        }

        glm::mat4 modelWorldTransform = glm::identity<glm::mat4>();
        // Rendering to offscreen image at half the resolution requires dynamic viewport and scissor in the
        // graphics pipeline.
        auto& drawable =
            m_spModelLibrary->getOrCreateDrawable(
                m_spApplication->getContext(),
                model,
                material,
                *m_spCommandBufferFactory,
                m_spApplication->getContext().getGraphicsQueue(0));

        drawable.setWorldTransform(modelWorldTransform);

        createScene(m_spApplication->getContext(),drawable);
    }

#ifdef NDEBUG
    const bool m_enableValidationLayers = false;
#else
    const bool m_enableValidationLayers = true;
#endif

    std::unique_ptr<vgfx::SwapChain> m_spOffscreenSwapChain;
    std::unique_ptr<vgfx::RenderPass> m_spOffscreenRenderPass;
    std::unique_ptr<vgfx::RenderTarget> m_spOffscreenRenderTarget;

    std::unique_ptr<vgfx::ImageSharpener> m_spImageSharpener;

    std::unique_ptr<vgfx::Camera> m_spScene;

    std::unique_ptr<vgfx::ModelLibrary> m_spModelLibrary;

    std::vector<vgfx::WindowRenderer::QueueSubmitInfo> m_queueSubmits;

    void createScene(
        vgfx::Context& graphicsContext,
        vgfx::Drawable& drawable)
    {
        glm::vec3 viewPos(2.0f, 2.0f, 2.0f);
        glm::mat4 cameraView = glm::lookAt(
            viewPos,
            glm::vec3(0.0f, 0.0f, 0.0f), // center
            glm::vec3(0.0f, 0.0f, 1.0f)); // up

        auto& swapChainExtent = m_spApplication->getRenderer().getSwapChain().getImageExtent();
        // Vulkan NDC is different than OpenGL, so use this clip matrix to correct for that.
        const glm::mat4 clip(
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, -1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.5f, 0.0f,
            0.0f, 0.0f, 0.5f, 1.0f);
        glm::mat4 cameraProj =
            clip * glm::perspective(
                    glm::radians(45.0f),
                    static_cast<float>(swapChainExtent.width) / static_cast<float>(swapChainExtent.height),
                    0.1f, // near
                    30.0f); // far
        m_spScene = std::make_unique<vgfx::Camera>(
            graphicsContext,
            m_spApplication->getRenderer().getSwapChain().getImageCount(),
            cameraView,
            cameraProj);

        std::unique_ptr<vgfx::LightNode> spLightNode =
            std::make_unique<vgfx::LightNode>(
                glm::vec4(1.0f, 100.0f, 500.0f, 1.0f), //position
                glm::vec3(1.0f, 1.0f, 1.0f), // color
                150000.0f); // radius

        m_spScene->addNode(std::move(spLightNode));

        // TODO multiple lights
        /*lightingUniforms.lights[1] = {
            glm::vec4(100.0f, 0.0f, 3.0f, 1.0f), //position
            glm::vec3(1.0f, 1.0f, 1.0f), // color
            2000.0f, // radius
        };*/

        std::unique_ptr<vgfx::Object> spGraphicsObject = std::make_unique<vgfx::Object>();
        spGraphicsObject->addDrawable(drawable);

        spLightNode->addNode(std::move(spGraphicsObject));
    }

    /*void recordCommandBuffers()
    {
        m_queueSubmits.clear();
        m_queueSubmits.resize(m_spApplication->getRenderer().getSwapChain().getImageCount());

        for (size_t swapChainImageIndex = 0;
                swapChainImageIndex < m_spApplication->getRenderer().getSwapChain().getImageCount();
                ++swapChainImageIndex) {
            VkCommandBuffer commandBuffer = m_spCommandBufferFactory->createCommandBuffer();

            VkCommandBufferBeginInfo beginInfo = {};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

            vkBeginCommandBuffer(commandBuffer, &beginInfo);

            m_spOffscreenRenderPass->begin(commandBuffer, swapChainImageIndex, *m_spOffscreenRenderTarget.get());

            vkCmdBindPipeline(
                commandBuffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                m_spGraphicsPipeline->getHandle());

            for (auto& spObject : m_graphicsObjects) {
                spObject->draw(commandBuffer, swapChainImageIndex);
            }

            m_spOffscreenRenderPass->end(commandBuffer);

            m_spApplication->getRenderer().transferSwapChainToPresentQueue(commandBuffer, swapChainImageIndex);

            m_spImageSharpener->recordCommandBuffer(
                commandBuffer,
                swapChainImageIndex,
                m_spOffscreenSwapChain->getImage(swapChainImageIndex),
                m_spOffscreenSwapChain->getImageView(swapChainImageIndex),
                m_spApplication->getRenderer().getSwapChain().getImage(swapChainImageIndex),
                m_spApplication->getRenderer().getSwapChain().getImageView(swapChainImageIndex));

            m_spApplication->getRenderer().transferSwapChainToGfxQueue(commandBuffer, swapChainImageIndex);

            vkEndCommandBuffer(commandBuffer);

            m_queueSubmits[swapChainImageIndex].commandBuffers.push_back(commandBuffer);
        }
    }*/

    static constexpr float MILLISECS_IN_SEC = 1000.0f;
    size_t m_curFrame = 0;
    int64_t m_firstFrameTimestampMS = -1;

    bool draw()
    {
        uint32_t swapChainImageCount = static_cast<uint32_t>(m_spApplication->getRenderer().getSwapChain().getImageCount());
        uint32_t curSwapChainImage = swapChainImageCount;
        bool frameDropped = false;

        bool frameDrawn = false;
        while (true) {
            size_t syncObjIndex = m_curFrame % m_queueSubmits.size();
            if (m_spApplication->getRenderer().presentFrame(m_queueSubmits[curSwapChainImage]) == VK_ERROR_OUT_OF_DATE_KHR) {
                recreateSwapChain();
            }

            ++m_curFrame;
        }

        return true;
    }

    void recreateSwapChain()
    {
        m_spApplication->getContext().waitForDeviceToIdle();

        //Recreate SwapChain, Pipeline, and Command Buffers
        m_spApplication->getRenderer().initSwapChain(
            m_spApplication->getContext(),
            // If only double buffering is available then one frame in flight, otherwise
            // 2 frames in flight (i.e. triple buffering).
            std::min(m_spApplication->getRenderer().getSwapChainConfig().imageCount.value() - 1u, 2u));

        m_spCommandBufferFactory.reset(
            new vgfx::CommandBufferFactory(
                m_spApplication->getContext(),
                m_spApplication->getContext().getGraphicsQueueFamilyIndex().value()));

        recordCommandBuffers();
    }

    void cleanup()
    {
        m_spApplication->getContext().waitForDeviceToIdle();

        m_graphicsObjects.clear();

        m_spModelLibrary.reset();

        vgfx::MaterialsLibrary::UnloadAll();

        m_spCommandBufferFactory.reset();

        m_spApplication.reset();
    }
};

static Demo* s_pDemo = nullptr;


#ifdef __cplusplus
extern "C" {
#endif
bool DemoInit(
    void* window,
    VkResult (*createVulkanSurface)(VkInstance, void*, const VkAllocationCallbacks*, VkSurfaceKHR*),
    void (*getFramebufferSize)(void*, int*, int*),
    uint32_t platformSpecificExtensionCount,
    const char** platformSpecificExtensions,
    bool enableValidationLayers,
    const char* pDataPath,
    const char* pModelFilename,
    const char* pModelDiffuseTexName,
    const char* pVertexShaderFilename,
    const char* pFragShaderFilename)
{
    if (s_pDemo == nullptr) {
        try {
            s_pDemo = new Demo(enableValidationLayers);
			s_pDemo->init(
				window,
				createVulkanSurface,
				getFramebufferSize,
				platformSpecificExtensionCount,
				platformSpecificExtensions,
                pDataPath,
				pModelFilename,
                pModelDiffuseTexName,
				pVertexShaderFilename,
				pFragShaderFilename);
        } catch(const std::exception& e) {
            std::cerr << e.what() << '\n';
            return false;
        }
    }
    return true;
}

void DemoSetFramebufferResized(bool flag)
{
    s_pDemo->setFrameBufferResized(flag);
}

bool DemoDraw()
{
    return s_pDemo->draw();
}

void DemoCleanUp()
{
    if (s_pDemo)
        s_pDemo->cleanup();
}

#ifdef __cplusplus
}
#endif
