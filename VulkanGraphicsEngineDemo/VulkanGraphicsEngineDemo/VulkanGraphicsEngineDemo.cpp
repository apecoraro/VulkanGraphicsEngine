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

struct CameraParams {
    glm::mat4 proj = glm::identity<glm::mat4>();
};

struct Light
{
    glm::vec4 position;
    glm::vec3 color;
    float radius;
};

struct LightingUniforms {
    Light lights[2];
    glm::vec4 viewPos;
};

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
            vgfx::VertexXyzRgbUvN::GetConfig().vertexAttrDescriptions,
            fragmentShader,
            fragmentShaderEntryPointFunc);

        vgfx::Material& material = vgfx::MaterialsLibrary::GetOrLoadMaterial(m_spApplication->getContext(), materialInfo);

        vgfx::DescriptorPoolBuilder poolBuilder;
        // For each descriptor this will add 3 to the pool of that descriptor's type.
        poolBuilder.addDescriptorSets(material.getDescriptorSetLayouts(), 3);
        poolBuilder.addMaxSets(3); // We need to create three copies of this descriptor set for each in flight frame

        // For each descriptor this will add 3 to the pool of that descriptor's type.
        poolBuilder.addDescriptorSets(m_spImageSharpener->getComputeShader().getDescriptorSetLayouts(), 3);
        poolBuilder.addMaxSets(3); // We need to create three copies of this descriptor set for each in flight frame

        //poolBuilder.setCreateFlags(VkDescriptorPoolCreateFlags);
        m_spDescriptorPool = poolBuilder.createPool(m_spApplication->getContext());

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
        initGraphicsObject(
            m_spApplication->getContext(),
            *m_spDescriptorPool.get(),
            *m_spCommandBufferFactory,
            model,
            material,
            modelWorldTransform,
            *m_spModelLibrary);

        m_spImageSharpener->createRenderingResources(*m_spDescriptorPool.get());

        recordCommandBuffers();
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

    std::unique_ptr<vgfx::CommandBufferFactory> m_spCommandBufferFactory;

    CameraParams m_cameraMatrix;
    std::vector<std::unique_ptr<vgfx::Buffer>> m_cameraMatrixBuffers;

    std::unique_ptr<vgfx::Buffer> m_spLightingUniformsBuffer;

    std::vector<std::unique_ptr<vgfx::Object>> m_graphicsObjects;
    std::unique_ptr<vgfx::ModelLibrary> m_spModelLibrary;
    std::unique_ptr<vgfx::Pipeline> m_spGraphicsPipeline;
    std::unique_ptr<vgfx::DescriptorPool> m_spDescriptorPool;

    std::vector<vgfx::WindowRenderer::QueueSubmitInfo> m_queueSubmits;

    bool m_frameBufferResized = false;

    void setFrameBufferResized(bool flag)
    {
        m_frameBufferResized = flag;
    }

    void initGraphicsObject(
        vgfx::Context& graphicsContext,
        vgfx::DescriptorPool& descriptorPool,
        vgfx::CommandBufferFactory& commandBufferFactory,
        const vgfx::ModelLibrary::Model& model,
        const vgfx::Material& material,
        const glm::mat4& modelWorldTransform,
        vgfx::ModelLibrary& modelLibrary)
    {
        auto& drawable =
            modelLibrary.getOrCreateDrawable(
                graphicsContext,
                model,
                material,
                *m_spDescriptorPool.get(),
                commandBufferFactory,
                graphicsContext.getGraphicsQueue(0));

        drawable.setWorldTransform(modelWorldTransform);

        initDrawableDescriptorSets(graphicsContext, drawable, modelLibrary);

        std::unique_ptr<vgfx::Object> spModelObject = std::make_unique<vgfx::Object>();
        spModelObject->addDrawable(drawable);

        // GraphicsPipeline is specific to a material, vertex buffer config,
        // and input assembly config.
        vgfx::PipelineBuilder::InputAssemblyConfig inputConfig(
            drawable.getVertexBuffer().getConfig().primitiveTopology,
            vgfx::ModelLibrary::GetDefaultIndexBufferConfig().hasPrimitiveRestartValues);

        m_spGraphicsPipeline =
            createGraphicsPipeline(
                graphicsContext,
                *m_spOffscreenSwapChain.get(),
                m_spApplication->getRenderer().getDepthStencilBuffer(),
                *m_spOffscreenRenderPass.get(),
                material,
                drawable.getVertexBuffer().getConfig(),
                inputConfig);

        m_graphicsObjects.push_back(std::move(spModelObject));
    }

 
    void initDrawableDescriptorSets(
        vgfx::Context& graphicsContext,
        vgfx::Drawable& drawable,
        vgfx::ModelLibrary& modelLibrary)
    {
        glm::vec3 viewPos(2.0f, 2.0f, 2.0f);
        m_cameraMatrix.view = glm::lookAt(
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
        m_cameraMatrix.proj =
            clip * glm::perspective(
                    glm::radians(45.0f),
                    static_cast<float>(swapChainExtent.width) / static_cast<float>(swapChainExtent.height),
                    0.1f, // near
                    30.0f); // far
        vgfx::Buffer::Config cameraMatrixBufferCfg(sizeof(CameraParams));
        for (uint32_t index = 0; index < m_spApplication->getRenderer().getSwapChain().getImageCount(); ++index) {
            m_cameraMatrixBuffers.emplace_back(
                std::make_unique<vgfx::Buffer>(
                    graphicsContext,
                    vgfx::Buffer::Type::UniformBuffer,
                    cameraMatrixBufferCfg));
            m_cameraMatrixBuffers.back()->update(
                &m_cameraMatrix,
                sizeof(m_cameraMatrix));
        
            drawable.getDescriptorSetChain().front()->getDescriptorSet(index).update(
                graphicsContext, {{0, m_cameraMatrixBuffers.back().get()}});
        }

        uint32_t mipLevels =
            vgfx::Image::ComputeMipLevels2D(
                drawable.getImage(vgfx::Material::ImageType::Diffuse)->getWidth(),
                drawable.getImage(vgfx::Material::ImageType::Diffuse)->getHeight());

        vgfx::ImageView& imageView =
            modelLibrary.getOrCreateImageView(
                vgfx::ImageView::Config(
                    VK_FORMAT_R8G8B8A8_UNORM,
                    VK_IMAGE_VIEW_TYPE_2D,
                    0u, // base mip level
                    mipLevels),
                graphicsContext,
                *drawable.getImage(vgfx::Material::ImageType::Diffuse));

        vgfx::Sampler& sampler =
            modelLibrary.getOrCreateSampler(
                vgfx::Sampler::Config(
                    VK_FILTER_LINEAR,
                    VK_FILTER_LINEAR,
                    VK_SAMPLER_MIPMAP_MODE_LINEAR,
                    0.0f, // min lod
                    static_cast<float>(mipLevels),
                    VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                    VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                    VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                    false, 0), // Last two parameters are for anisotropic filtering
                graphicsContext); 

        LightingUniforms lightingUniforms;
        lightingUniforms.viewPos = glm::vec4(viewPos, 1.0f);
        lightingUniforms.lights[0] = {
            glm::vec4(1.0f, 100.0f, 500.0f, 1.0f), //position
            glm::vec3(1.0f, 1.0f, 1.0f), // color
            150000.0f, // radius
        };
        lightingUniforms.lights[1] = {
            glm::vec4(100.0f, 0.0f, 3.0f, 1.0f), //position
            glm::vec3(1.0f, 1.0f, 1.0f), // color
            2000.0f, // radius
        };

        vgfx::Buffer::Config lightingBufferCfg(sizeof(lightingUniforms));
        m_spLightingUniformsBuffer =
            std::make_unique<vgfx::Buffer>(
                graphicsContext, vgfx::Buffer::Type::UniformBuffer, lightingBufferCfg);
        m_spLightingUniformsBuffer->update(&lightingUniforms, sizeof(lightingUniforms));

        vgfx::CombinedImageSamplerDescriptorUpdater descriptorUpdater(imageView,  sampler);
        // frag shader descriptor set is set index == 1.
        drawable.getDescriptorSetChain()[1]->getDescriptorSet(0).update(
            graphicsContext, { {0, &descriptorUpdater}, {1, m_spLightingUniformsBuffer.get()} });
    }

    std::unique_ptr<vgfx::Pipeline> createGraphicsPipeline(
        vgfx::Context& context,
        const vgfx::SwapChain& swapChain,
        const vgfx::DepthStencilBuffer* pDepthStencilBuffer,
        const vgfx::RenderPass& renderPass,
        const vgfx::Material& material,
        const vgfx::VertexBuffer::Config& vertexBufferConfig,
        const vgfx::PipelineBuilder::InputAssemblyConfig& inputConfig)
    {
        VkViewport viewport = {
            0.0f, 0.0f,
            static_cast<float>(swapChain.getImageExtent().width),
            static_cast<float>(swapChain.getImageExtent().height),
            0.0f,
            1.0f
        };
        vgfx::PipelineBuilder builder(viewport, renderPass, pDepthStencilBuffer);

        vgfx::PipelineBuilder::RasterizerConfig rasterizerConfig(
            VK_POLYGON_MODE_FILL,
            VK_CULL_MODE_BACK_BIT,
            VK_FRONT_FACE_COUNTER_CLOCKWISE);

        //std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

        return builder.configureDrawableInput(material, vertexBufferConfig, inputConfig)
                      .configureRasterizer(rasterizerConfig)
                      //.configureDynamicStates(dynamicStates)
                      .createPipeline(context); 
    }

    void recordCommandBuffers()
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
    }

    void setViewportAndScissor(VkCommandBuffer commandBuffer, const VkExtent2D& extent)
    {
        VkViewport viewport = {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(extent.width);
        viewport.height = static_cast<float>(extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        vkCmdSetViewport(commandBuffer, 0u, 1u, &viewport);

        VkRect2D scissor = {};
        scissor.offset = { 0, 0 };
        scissor.extent = extent;
        vkCmdSetScissor(commandBuffer, 0u, 1u, &scissor);
    }

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

        m_spDescriptorPool.reset();

        m_spModelLibrary.reset();

        vgfx::MaterialsLibrary::UnloadAll();

        m_spCommandBufferFactory.reset();

        m_spGraphicsPipeline.reset();

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
