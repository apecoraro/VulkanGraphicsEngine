#include "VulkanGraphicsEngineDemo.h"

#include "VulkanGraphicsContext.h"
#include "VulkanGraphicsDescriptorPoolBuilder.h"
#include "VulkanGraphicsDescriptors.h"
#include "VulkanGraphicsImageDescriptorUpdaters.h"
#include "VulkanGraphicsImageSharpener.h"
#include "VulkanGraphicsImageView.h"
#include "VulkanGraphicsMaterials.h"
#include "VulkanGraphicsModelLibrary.h"
#include "VulkanGraphicsRenderer.h"
#include "VulkanGraphicsRenderPass.h"
#include "VulkanGraphicsRenderTarget.h"
#include "VulkanGraphicsSampler.h"
#include "VulkanGraphicsSwapChain.h"
#include "VulkanGraphicsBuffer.h"


#include <vulkan/vulkan.h>

#include <algorithm>
#include <iostream>
#include <glm\ext\matrix_transform.hpp>
#include <glm\ext\matrix_clip_space.hpp>

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

struct ModelViewProj {
    glm::mat4 model = glm::identity<glm::mat4>();
    glm::mat4 view = glm::identity<glm::mat4>();
    glm::mat4 proj = glm::identity<glm::mat4>();
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
	
    void init(
        void* window,
        const std::function<VkResult(VkInstance, void*, const VkAllocationCallbacks*, VkSurfaceKHR*)>& createVulkanSurface,
        const std::function<void(void*, int*, int*)>& getFramebufferSize,
        uint32_t platformSpecificExtensionCount,
        const char** platformSpecificExtensions,
        const std::string& dataPath,
        const std::string& model,
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

        std::unique_ptr<vgfx::WindowSwapChain> spWindowSwapChain =
            std::make_unique<vgfx::WindowSwapChain>(
                window,
                windowWidth,
                windowHeight,
                createVulkanSurface);

        vgfx::WindowRenderer::SwapChainConfig swapChainConfig;
        swapChainConfig.imageCount = 3;
        // Seems like if the window is not fullscreen then we are basically limited to the size of
        // the window for the SwapChain. If fullscreen then there might be other choices, although
        // I need to test that out a bit.
        swapChainConfig.imageExtent = {
			static_cast<uint32_t>(windowWidth),
			static_cast<uint32_t>(windowHeight)
		};

        swapChainConfig.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT;

        swapChainConfig.imageFormat = { VK_FORMAT_R16G16B16A16_SFLOAT, VK_COLORSPACE_SRGB_NONLINEAR_KHR };

        swapChainConfig.pickDepthStencilFormat = [](const std::set<VkFormat>& formats) {
            if (formats.find(VK_FORMAT_D32_SFLOAT) != formats.end()) {
                return VK_FORMAT_D32_SFLOAT;
            } else if (formats.find(VK_FORMAT_D32_SFLOAT_S8_UINT) != formats.end()) {
                return VK_FORMAT_D32_SFLOAT_S8_UINT;
            } else if (formats.find(VK_FORMAT_D24_UNORM_S8_UINT) != formats.end()) {
                return VK_FORMAT_D24_UNORM_S8_UINT;
            } else {
                throw std::runtime_error("Failed to find suitable depth stencil format!");
                return VK_FORMAT_MAX_ENUM;
            }
        };

        m_spWindowRenderer =
            std::make_unique<vgfx::WindowRenderer>(
                swapChainConfig,
                std::move(spWindowSwapChain));

        m_graphicsContext.init(
            appConfig,
            instanceConfig,
            deviceConfig,
            m_spWindowRenderer.get());

        if (m_enableValidationLayers) {
            m_graphicsContext.enableDebugReportCallback(DebugCallback);
        }

        m_spWindowRenderer->initSwapChain(
            m_graphicsContext,
            // This callback allows for custom implementation of ImageView creation on the
            // SwapChain images.
            [](vgfx::Context& context, VkImage image, VkFormat imageFormat)
            {
                vgfx::ImageView::Config cfg(
                    imageFormat,
                    VK_IMAGE_VIEW_TYPE_2D,
                    0u, // base mip level
                    1u); // mip levels
                return std::make_unique<vgfx::ImageView>(context, cfg, image);
            },
            // If only double buffering is available then one frame in flight, otherwise
            // 2 frames in flight (i.e. triple buffering).
            std::min(m_spWindowRenderer->getSwapChainConfig().imageCount.value() - 1u, 2u));

        // Render to half size offscreen image then use image sharpener to upscale
        vgfx::Image::Config offscreenImageCfg(
            windowWidth >> 1,
            windowHeight >> 1,
            VK_FORMAT_R16G16B16A16_SFLOAT,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
            | VK_IMAGE_USAGE_STORAGE_BIT
            | VK_IMAGE_USAGE_SAMPLED_BIT);

        std::vector<std::unique_ptr<vgfx::Image>> offscreenImages;
        offscreenImages.push_back(std::make_unique<vgfx::Image>(m_graphicsContext, offscreenImageCfg));
        offscreenImages.push_back(std::make_unique<vgfx::Image>(m_graphicsContext, offscreenImageCfg));
        offscreenImages.push_back(std::make_unique<vgfx::Image>(m_graphicsContext, offscreenImageCfg));

        vgfx::ImageView::Config offscreenImageViewCfg(
            VK_FORMAT_R16G16B16A16_SFLOAT,
            VK_IMAGE_VIEW_TYPE_2D);

        std::vector<std::unique_ptr<vgfx::ImageView>> offscreenImageViews;
        offscreenImageViews.push_back(
            std::make_unique<vgfx::ImageView>(
                m_graphicsContext, offscreenImageViewCfg, *offscreenImages[0].get()));
        offscreenImageViews.push_back(
            std::make_unique<vgfx::ImageView>(
                m_graphicsContext, offscreenImageViewCfg, *offscreenImages[1].get()));
        offscreenImageViews.push_back(
            std::make_unique<vgfx::ImageView>(
                m_graphicsContext, offscreenImageViewCfg, *offscreenImages[2].get()));

        m_spOffscreenSwapChain =
            std::make_unique<vgfx::OffscreenSwapChain>(
                std::move(offscreenImages),
                std::move(offscreenImageViews));

        vgfx::RenderPassBuilder renderPassBuilder;

        vgfx::RenderTarget::Config offscreenRenderTargetConfig(
            *m_spOffscreenSwapChain.get(),
            m_spWindowRenderer->getDepthStencilBuffer());

        renderPassBuilder.addPass(offscreenRenderTargetConfig)
            // The compute shader expects the image to be in general layout.
            // TODO figure out if possible to create a COMPUTE render pass?
            .attachments[0].finalLayout = VK_IMAGE_LAYOUT_GENERAL;

        m_spOffscreenRenderPass = renderPassBuilder.createPass(m_graphicsContext);
            
        m_spOffscreenRenderTarget =
            std::make_unique<vgfx::RenderTarget>(
                m_graphicsContext,
                offscreenRenderTargetConfig,
                *m_spOffscreenRenderPass.get());

        m_spImageSharpener =
            std::make_unique<vgfx::ImageSharpener>(
                m_graphicsContext,
                m_spWindowRenderer->getSwapChain().getImageCount(),
                1.0f); // sharpening value

        m_spCommandBufferFactory =
            std::make_unique<vgfx::CommandBufferFactory>(
                m_graphicsContext,
                m_graphicsContext.getGraphicsQueueFamilyIndex().value());

        // Vertex shader input types, in location order
        std::vector<VkFormat> vertexShaderInputs = {
            VK_FORMAT_R32G32B32_SFLOAT, // position
            VK_FORMAT_R32G32B32_SFLOAT, // color
            VK_FORMAT_R32G32_SFLOAT // uv
        };
        std::string vertexShaderEntryPointFunc = "main";
        // Frag shader input types, in location order
        std::vector<VkFormat> fragmentShaderInputs = {
            VK_FORMAT_R32G32B32_SFLOAT, // color
            VK_FORMAT_R32G32_SFLOAT // uv
        };

        std::string fragmentShaderEntryPointFunc = "main";

        uint32_t bindingIndex = 0;
        vgfx::DescriptorSetLayout::DescriptorBindings cameraMatrixDescSetBindings;
        cameraMatrixDescSetBindings[bindingIndex] =
            vgfx::DescriptorSetLayout::DescriptorBinding(
                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                VK_SHADER_STAGE_VERTEX_BIT);

        vgfx::DescriptorSetLayout::DescriptorBindings combImgSamplerDescSetBindings;
        combImgSamplerDescSetBindings[bindingIndex] =
            vgfx::DescriptorSetLayout::DescriptorBinding(
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                VK_SHADER_STAGE_FRAGMENT_BIT);

        std::vector<vgfx::DescriptorSetLayoutBindingInfo> descriptorSetLayoutBindings = {
            vgfx::DescriptorSetLayoutBindingInfo(
                cameraMatrixDescSetBindings,
                m_spWindowRenderer->getSwapChain().getImageCount()), // Number of copies of this DescriptorSet
            vgfx::DescriptorSetLayoutBindingInfo(
                combImgSamplerDescSetBindings)
        };

        std::vector<vgfx::Material::ImageType> imageTypes = { vgfx::Material::ImageType::Diffuse };

        // Create MaterialInfo which is used to create an instance of the material for a particular
        // model by the ModelLibrary.
        vgfx::MaterialsLibrary::MaterialInfo materialInfo(
            vertexShaderInputs,
            vertexShader,
            vertexShaderEntryPointFunc,
            fragmentShaderInputs,
            fragmentShader,
            fragmentShaderEntryPointFunc,
            descriptorSetLayoutBindings,
            imageTypes);

        vgfx::Material& material = vgfx::MaterialsLibrary::GetOrLoadMaterial(m_graphicsContext, materialInfo);

        vgfx::DescriptorPoolBuilder poolBuilder;
        poolBuilder.addMaterialDescriptorSets(material);
        poolBuilder.addComputeShaderDescriptorSets(m_spImageSharpener->getComputeShader());
        //poolBuilder.setCreateFlags(VkDescriptorPoolCreateFlags);
        m_spDescriptorPool = poolBuilder.createPool(m_graphicsContext);

        m_spModelLibrary = std::make_unique<vgfx::ModelLibrary>();
        // Rendering to offscreen image at half the resolution requires dynamic viewport and scissor in the
        // graphics pipeline.
        initGraphicsObject(
            m_graphicsContext,
            *m_spDescriptorPool.get(),
            *m_spCommandBufferFactory,
            model,
            material,
            *m_spModelLibrary);

        m_spImageSharpener->createRenderingResources(*m_spDescriptorPool.get());

        recordCommandBuffers();
    }

#ifdef NDEBUG
    const bool m_enableValidationLayers = false;
#else
    const bool m_enableValidationLayers = true;
#endif
    vgfx::Context m_graphicsContext;
    std::unique_ptr<vgfx::WindowRenderer> m_spWindowRenderer;

    std::unique_ptr<vgfx::OffscreenSwapChain> m_spOffscreenSwapChain;
    std::unique_ptr<vgfx::RenderPass> m_spOffscreenRenderPass;
    std::unique_ptr<vgfx::RenderTarget> m_spOffscreenRenderTarget;

    std::unique_ptr<vgfx::ImageSharpener> m_spImageSharpener;

    std::unique_ptr<vgfx::CommandBufferFactory> m_spCommandBufferFactory;
    ModelViewProj m_cameraMatrix;
    std::vector<std::unique_ptr<vgfx::Buffer>> m_cameraMatrixBuffers;
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
        const std::string& model,
        const vgfx::Material& material,
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

        initDrawableDescriptorSets(graphicsContext, drawable, modelLibrary);

        std::unique_ptr<vgfx::Object> spModelObject = std::make_unique<vgfx::Object>();
        spModelObject->addDrawable(drawable);

        // GraphicsPipeline is specific to a material, vertex buffer config,
        // and input assembly config.
        vgfx::PipelineBuilder::InputAssemblyConfig inputConfig(
            vgfx::ModelLibrary::GetDefaultVertexBufferConfig().primitiveTopology,
            vgfx::ModelLibrary::GetDefaultIndexBufferConfig().hasPrimitiveRestartValues);

        m_spGraphicsPipeline =
            createGraphicsPipeline(
                graphicsContext,
                *m_spOffscreenSwapChain.get(),
                m_spWindowRenderer->getDepthStencilBuffer(),
                *m_spOffscreenRenderPass.get(),
                material,
                vgfx::ModelLibrary::GetDefaultVertexBufferConfig(),
                inputConfig);

        m_graphicsObjects.push_back(std::move(spModelObject));
    }
 
    void initDrawableDescriptorSets(
        vgfx::Context& graphicsContext,
        vgfx::Drawable& drawable,
        vgfx::ModelLibrary& modelLibrary)
    {
        m_cameraMatrix.model = glm::identity<glm::mat4>();
        m_cameraMatrix.view = glm::lookAt(
            glm::vec3(2.0f, 2.0f, 2.0f), // eye
            glm::vec3(0.0f, 0.0f, 0.0f), // center
            glm::vec3(0.0f, 0.0f, 1.0f)); // up
        auto& swapChainExtent = m_spWindowRenderer->getSwapChain().getImageExtent();
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
        vgfx::Buffer::Config uniformBufferConfig(sizeof(ModelViewProj));
        for (uint32_t index = 0; index < m_spWindowRenderer->getSwapChain().getImageCount(); ++index) {
            m_cameraMatrixBuffers.emplace_back(
                std::make_unique<vgfx::Buffer>(
                    graphicsContext,
                    vgfx::Buffer::Type::UniformBuffer,
                    uniformBufferConfig));
            m_cameraMatrixBuffers.back()->update(
                &m_cameraMatrix,
                sizeof(m_cameraMatrix),
                vgfx::Buffer::MemMap::LeaveMapped);
        
            drawable.getDescriptorSetBuffers().front()->getDescriptorSet(index).update(
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
                    false, 0),
                graphicsContext); // Last two parameters are for anisotropic filtering*/

        vgfx::CombinedImageSamplerDescriptorUpdater descriptorUpdater(imageView,  sampler);
        // Image sampler descriptor is set index == 1.
        drawable.getDescriptorSetBuffers()[1]->getDescriptorSet(0).update(
            graphicsContext, {{0, &descriptorUpdater}});
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
        m_queueSubmits.resize(m_spWindowRenderer->getSwapChain().getImageCount());

        for (size_t swapChainImageIndex = 0;
                swapChainImageIndex < m_spWindowRenderer->getSwapChain().getImageCount();
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
                spObject->recordDrawCommands(commandBuffer, swapChainImageIndex);
            }

            m_spOffscreenRenderPass->end(commandBuffer);

            m_spWindowRenderer->startFrame(commandBuffer, swapChainImageIndex);

            m_spImageSharpener->recordCommandBuffer(
                commandBuffer,
                swapChainImageIndex,
                m_spOffscreenSwapChain->getImage(swapChainImageIndex),
                m_spOffscreenSwapChain->getImageView(swapChainImageIndex),
                m_spWindowRenderer->getSwapChain().getImage(swapChainImageIndex),
                m_spWindowRenderer->getSwapChain().getImageView(swapChainImageIndex));

            m_spWindowRenderer->endFrame(commandBuffer, swapChainImageIndex);

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
        uint32_t swapChainImageCount = static_cast<uint32_t>(m_spWindowRenderer->getSwapChain().getImageCount());
        uint32_t curSwapChainImage = swapChainImageCount;
        bool frameDropped = false;

        bool frameDrawn = false;
        while (true) {
            size_t syncObjIndex = m_curFrame % MAX_FRAMES_IN_FLIGHT;
            if (m_spWindowRenderer->acquireNextSwapChainImage(&curSwapChainImage) == VK_ERROR_OUT_OF_DATE_KHR) {
                recreateSwapChain();
                continue;
            }

            if (m_spWindowRenderer->renderFrame(curSwapChainImage, m_queueSubmits[curSwapChainImage]) == VK_ERROR_OUT_OF_DATE_KHR) {
                recreateSwapChain();
            }

            ++m_curFrame;
        }

        return true;
    }

    void recreateSwapChain()
    {
        m_graphicsContext.waitForDeviceToIdle();

        //Recreate SwapChain, Pipeline, and Command Buffers
        m_spWindowRenderer->initSwapChain(
            m_graphicsContext,
            [](vgfx::Context& context, VkImage image, VkFormat imageFormat)
            {
                vgfx::ImageView::Config cfg(
                    imageFormat,
                    VK_IMAGE_VIEW_TYPE_2D,
                    0u, // base mip level
                    1u); // mip levels
                return std::make_unique<vgfx::ImageView>(context, cfg, image);
            },
            // If only double buffering is available then one frame in flight, otherwise
            // 2 frames in flight (i.e. triple buffering).
            std::min(m_spWindowRenderer->getSwapChainConfig().imageCount.value() - 1u, 2u));

        m_spCommandBufferFactory.reset(
            new vgfx::CommandBufferFactory(
                m_graphicsContext,
                m_graphicsContext.getGraphicsQueueFamilyIndex().value()));

        recordCommandBuffers();
    }

    void cleanup()
    {
        m_graphicsContext.waitForDeviceToIdle();

        m_graphicsObjects.clear();

        m_spDescriptorPool.reset();

        m_spModelLibrary.reset();

        vgfx::MaterialsLibrary::UnloadAll();

        m_spCommandBufferFactory.reset();

        m_spGraphicsPipeline.reset();

        m_spWindowRenderer.reset();
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
