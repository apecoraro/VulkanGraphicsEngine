#include "VulkanGraphicsEngineDemo.h"

#include "VulkanGraphicsCombinedImageSampler.h"
#include "VulkanGraphicsContext.h"
#include "VulkanGraphicsDescriptorPoolBuilder.h"
#include "VulkanGraphicsDescriptors.h"
#include "VulkanGraphicsImageView.h"
#include "VulkanGraphicsMaterials.h"
#include "VulkanGraphicsModelLibrary.h"
#include "VulkanGraphicsRenderer.h"
#include "VulkanGraphicsSampler.h"
#include "VulkanGraphicsSwapChain.h"
#include "VulkanGraphicsUniformBuffer.h"


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
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
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
        const std::string& modelPath,
        const std::string& vertexShaderPath,
        const std::string& fragmentShaderPath)
    {
        vgfx::Context::AppConfig appConfig("Demo", 1, 0, 0);

        vgfx::Context::InstanceConfig instanceConfig;
        instanceConfig.requiredExtensions.insert(
            instanceConfig.requiredExtensions.end(),
            platformSpecificExtensions,
            platformSpecificExtensions + platformSpecificExtensionCount);

        if (m_enableValidationLayers) {
            instanceConfig.requiredExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
            instanceConfig.validationLayers.push_back("VK_LAYER_LUNARG_standard_validation");
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

        m_spWindowRenderer = std::make_unique<vgfx::WindowRenderer>(
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

        vgfx::RenderTarget::Config defaultRenderTargetConfig;
        m_spWindowRenderer->initSwapChain(
            m_graphicsContext,
            // This callback allows for custom implementation of ImageView creation on the
            // SwapChain images.
            [] (vgfx::Context& context, VkImage image, VkFormat imageFormat)
            {
                vgfx::ImageView::Config cfg(
                        imageFormat,
                        VK_IMAGE_VIEW_TYPE_2D,
                        1u);
                return std::make_unique<vgfx::ImageView>(context, cfg, image);
            },
            // If only double buffering is available then one frame in flight, otherwise
            // 2 frames in flight (i.e. triple buffering).
            std::min(m_spWindowRenderer->getSwapChainConfig().imageCount.value() - 1u, 2u),
            defaultRenderTargetConfig);

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

        std::vector<vgfx::MaterialsLibrary::DescriptorSetLayoutBindingInfo> descriptorSetLayoutBindings = {
            vgfx::MaterialsLibrary::DescriptorSetLayoutBindingInfo(
                cameraMatrixDescSetBindings,
                m_spWindowRenderer->getSwapChain().getImageCount()), // Number of copies of this DescriptorSet
            vgfx::MaterialsLibrary::DescriptorSetLayoutBindingInfo(
                combImgSamplerDescSetBindings)
        };

        std::vector<vgfx::Material::ImageType> imageTypes = { vgfx::Material::ImageType::DIFFUSE };

        // Create MaterialInfo which is used to create an instance of the material for a particular
        // model by the ModelLibrary.
        vgfx::MaterialsLibrary::MaterialInfo materialInfo(
            vertexShaderInputs,
            vertexShaderPath,
            vertexShaderEntryPointFunc,
            fragmentShaderInputs,
            fragmentShaderPath,
            fragmentShaderEntryPointFunc,
            descriptorSetLayoutBindings,
            imageTypes);

        vgfx::Material& material = vgfx::MaterialsLibrary::GetOrLoadMaterial(m_graphicsContext, materialInfo);

        m_spModelLibrary = std::make_unique<vgfx::ModelLibrary>();

        initGraphicsObject(m_graphicsContext, *m_spCommandBufferFactory, modelPath, material, *m_spModelLibrary);
    }

#ifdef NDEBUG
    const bool m_enableValidationLayers = false;
#else
    const bool m_enableValidationLayers = true;
#endif
    vgfx::Context m_graphicsContext;
    std::unique_ptr<vgfx::WindowRenderer> m_spWindowRenderer;

    std::unique_ptr<vgfx::CommandBufferFactory> m_spCommandBufferFactory;
    ModelViewProj m_cameraMatrix;
    std::vector<std::unique_ptr<vgfx::UniformBuffer>> m_cameraMatrixUniformBuffers;
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
        vgfx::CommandBufferFactory& commandBufferFactory,
        const std::string& modelPath,
        const vgfx::Material& material,
        vgfx::ModelLibrary& modelLibrary)
    {
        vgfx::DescriptorPoolBuilder poolBuilder;
        poolBuilder.addMaterialDescriptorSets(material);
        //poolBuilder.setCreateFlags(VkDescriptorPoolCreateFlags);
        m_spDescriptorPool =
            poolBuilder.createPool(
                graphicsContext,
                // Last parameter is the max number of sets that will be allocated from
                // this pool, which in our case is the number of swap chain images plus
                // one because we have a set of copies for the camera matrix uniform buffer
                // and the one set for the image sampler.
                m_spWindowRenderer->getSwapChain().getImageCount() + 1u);

        auto& drawable =
            modelLibrary.getOrCreateDrawable(
                graphicsContext,
                modelPath,
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
                m_spWindowRenderer->getSwapChain(),
                *m_spWindowRenderer->getRenderTarget(),
                material,
                vgfx::ModelLibrary::GetDefaultVertexBufferConfig(),
                inputConfig);

        m_graphicsObjects.push_back(std::move(spModelObject));

        m_queueSubmits.resize(m_spWindowRenderer->getSwapChain().getImageCount());
        for (size_t i = 0; i < m_spWindowRenderer->getSwapChain().getImageCount(); ++i) {
            VkCommandBuffer commandBuffer = m_spCommandBufferFactory->createCommandBuffer();
            m_queueSubmits[i].commandBuffers.push_back(commandBuffer);
            m_spWindowRenderer->recordCommandBuffer(
                commandBuffer, i, *m_spGraphicsPipeline.get(), m_graphicsObjects);
        }
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
        bool keepUBMapped = true;
        vgfx::UniformBuffer::Config uniformBufferConfig(sizeof(ModelViewProj));
        for (uint32_t index = 0; index < m_spWindowRenderer->getSwapChain().getImageCount(); ++index) {
            m_cameraMatrixUniformBuffers.emplace_back(
                std::make_unique<vgfx::UniformBuffer>(graphicsContext, uniformBufferConfig));
            m_cameraMatrixUniformBuffers.back()->update(&m_cameraMatrix, sizeof(m_cameraMatrix), keepUBMapped);
        
            drawable.getDescriptorSetBuffers().front()->getDescriptorSet(index).update(
                graphicsContext, {{0, m_cameraMatrixUniformBuffers.back().get()}});
        }

        vgfx::ImageView& imageView =
            modelLibrary.getOrCreateImageView(
                vgfx::ImageView::Config(
                    VK_FORMAT_R8G8B8A8_UNORM,
                    VK_IMAGE_VIEW_TYPE_2D,
                    1u), // TODO add support for mip maps
                graphicsContext,
                *drawable.getImage(vgfx::Material::ImageType::DIFFUSE));

        vgfx::Sampler& sampler =
            modelLibrary.getOrCreateSampler( 
                vgfx::Sampler::Config(
                    VK_FILTER_LINEAR,
                    VK_FILTER_LINEAR,
                    VK_SAMPLER_MIPMAP_MODE_LINEAR,
                    VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                    VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                    VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                    false, 0),
                graphicsContext); // Last two parameters are for anisotropic filtering*/

        vgfx::CombinedImageSampler imageSampler(imageView, sampler);
        // Image sampler descriptor is set index == 1.
        drawable.getDescriptorSetBuffers()[1]->getDescriptorSet(0).update(
            graphicsContext, {{0, &imageSampler}});
    }

    std::unique_ptr<vgfx::Pipeline> createGraphicsPipeline(
        vgfx::Context& context,
        const vgfx::SwapChain& swapChain,
        const vgfx::RenderTarget& renderTarget,
        const vgfx::Material& material,
        const vgfx::VertexBuffer::Config& vertexBufferConfig,
        const vgfx::PipelineBuilder::InputAssemblyConfig& inputConfig)
    {
        vgfx::PipelineBuilder builder(swapChain, renderTarget);

        vgfx::PipelineBuilder::RasterizerConfig rasterizerConfig(
            VK_POLYGON_MODE_FILL,
            VK_CULL_MODE_BACK_BIT,
            VK_FRONT_FACE_COUNTER_CLOCKWISE);

        return builder.configureDrawableInput(material, vertexBufferConfig, inputConfig)
                      .configureRasterizer(rasterizerConfig)
                      .createPipeline(context); 
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
        vgfx::RenderTarget::Config defaultRenderTargetConfig;
        m_spWindowRenderer->initSwapChain(
            m_graphicsContext,
            [](vgfx::Context& context, VkImage image, VkFormat imageFormat)
            {
                vgfx::ImageView::Config cfg(
                    imageFormat,
                    VK_IMAGE_VIEW_TYPE_2D,
                    1u);
                return std::make_unique<vgfx::ImageView>(context, cfg, image);
            },
            // If only double buffering is available then one frame in flight, otherwise
            // 2 frames in flight (i.e. triple buffering).
            std::min(m_spWindowRenderer->getSwapChainConfig().imageCount.value() - 1u, 2u),
            defaultRenderTargetConfig);

        m_spCommandBufferFactory.reset(
            new vgfx::CommandBufferFactory(
                m_graphicsContext,
                m_graphicsContext.getGraphicsQueueFamilyIndex().value()));

        m_queueSubmits.clear();
        m_queueSubmits.resize(m_spWindowRenderer->getSwapChain().getImageCount());
        for (size_t i = 0; i < m_spWindowRenderer->getSwapChain().getImageCount(); ++i) {
            VkCommandBuffer commandBuffer = m_spCommandBufferFactory->createCommandBuffer();
            m_queueSubmits[i].commandBuffers.push_back(commandBuffer);
            m_spWindowRenderer->recordCommandBuffer(
                commandBuffer, i, *m_spGraphicsPipeline.get(), m_graphicsObjects);
        }
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
    const char* pModelPath,
    const char* pVertexShaderPath,
    const char* pFragShaderPath)
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
				pModelPath,
				pVertexShaderPath,
				pFragShaderPath);
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
