//
//  Created by Alex Pecoraro on 3/5/20.
//
#include "VulkanGraphicsContext.h"

#include "VulkanGraphicsImageDownsampler.h"
#include "VulkanGraphicsRenderer.h"
#include "VulkanGraphicsRenderTarget.h"

#include <exception>
#include <iostream>
#include <set>
#include <sstream>

namespace vgfx
{
    constexpr static const char* k_vgfxEngineName = "VulkanGraphicsEngine";
    constexpr static uint32_t k_vgfxVersion = VK_MAKE_VERSION(1, 0, 0);

    void Context::init(
        const AppConfig& appConfig,
        const InstanceConfig& instanceConfig,
        const DeviceConfig& deviceConfig,
        Renderer& renderer)
    {
        m_appConfig = appConfig;

        createInstance(
            appConfig,
            instanceConfig);

        renderer.getPresenter().createVulkanSurface(m_instance, m_pAllocationCallbacks);

        pickPhysicalDevice(
            deviceConfig,
            renderer);

        renderer.getPresenter().configureForDevice(m_physicalDevice);

        createLogicalDevice(deviceConfig);

        m_memoryAllocator.init(
            VK_MAKE_VERSION(
                appConfig.majorVersion,
                appConfig.minorVersion,
                appConfig.patchVersion),
            *this);

        renderer.getPresenter().initSwapChain(renderer);
    }

    void Context::shutdown()
    {
        waitForDeviceToIdle();

        m_memoryAllocator.shutdown();

        disableDebugReportCallback();

        if (m_device != VK_NULL_HANDLE) {
            vkDestroyDevice(m_device, m_pAllocationCallbacks);
            m_device = VK_NULL_HANDLE;
        }

        if (m_instance != VK_NULL_HANDLE) {
            vkDestroyInstance(m_instance, m_pAllocationCallbacks);
            m_instance = VK_NULL_HANDLE;
        }
    }

    void Context::waitForDeviceToIdle()
    {
        if (m_device != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(m_device);
        }
    }

    template<typename ContainerType> void ContainerOfStringsToCharPtrs(
        const ContainerType& input,
        std::vector<const char*>* pOutput)
    {
        for (auto& extStr : input) {
            pOutput->push_back(extStr.c_str());
        }
    }

    CommandBufferFactory& Context::getOrCreateUtilCommandBufferFactory()
    {
        if (m_spUtilCommandBufferFactory == nullptr) {
            m_spUtilCommandBufferFactory.reset(new CommandBufferFactory(*this, getGraphicsQueue(0u)));
        }

        return *m_spUtilCommandBufferFactory.get();
    }

    ImageDownsampler& Context::getOrCreateImageDownsampler()
    {
        assert(m_shaderSubgroupsAreSupported && m_descriptorIndexingIsSupported);

        if (m_spImageDownsampler == nullptr) {
            m_spImageDownsampler.reset(new ImageDownsampler(
                    *this,
                    m_fp16IsSupported ?
                        ImageDownsampler::Precision::FP16 :
                        ImageDownsampler::Precision::FP32));
        }
        return *m_spImageDownsampler.get();
    }

    void Context::beginRendering(VkCommandBuffer commandBuffer, const RenderTarget& renderTarget)
    {
        std::vector<VkRenderingAttachmentInfoKHR> colorAttachments(renderTarget.getAttachmentCount());
        VkClearValue clearColor;
        clearColor.color = { {0.0f, 0.0f, 0.0f, 1.0f} };
        for (size_t i = 0; i < colorAttachments.size(); ++i) {
            auto& colorAttachmentInfo = colorAttachments[i];
            colorAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;

            const ImageView& imageView = renderTarget.getAttachmentView(i);
            colorAttachmentInfo.imageView = imageView.getHandle();

            colorAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colorAttachmentInfo.resolveMode = VK_RESOLVE_MODE_NONE;
            colorAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            colorAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;


            colorAttachmentInfo.clearValue = clearColor;
        }

        VkRenderingAttachmentInfoKHR depthAttachment = {};
        depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        if (renderTarget.hasDepthStencilBuffer())
            depthAttachment.imageView = renderTarget.getDepthStencilView()->getHandle();

        depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        depthAttachment.resolveMode = VK_RESOLVE_MODE_NONE;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

        VkClearValue clearDepth;
        clearDepth.depthStencil = { 1.0f, ~0U };
        depthAttachment.clearValue = clearDepth;

        auto renderArea = VkRect2D{
            VkOffset2D { },
            VkExtent2D {
                renderTarget.getExtent().width,
                renderTarget.getExtent().height
            }
        };

        VkRenderingInfoKHR renderingInfo = {};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.renderArea = renderArea;
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments = colorAttachments.data();
        renderingInfo.layerCount = 1;
        if (renderTarget.hasDepthStencilBuffer()) {
            renderingInfo.pDepthAttachment = &depthAttachment;
        }

        //if (renderTarget.hasDepthStencilBuffer()) {
        //    renderingInfo.pStencilAttachment = &depthAttachment;
        //}
        m_vkCmdBeginRendering(commandBuffer, &renderingInfo);
    }

    static Context::InstanceVersion QueryInstanceVersion()
    {
        uint32_t versionBits = 0;
        VkResult result = vkEnumerateInstanceVersion(&versionBits);
        if (result != VK_SUCCESS)
            throw std::runtime_error("Failed to query instance version!");

        Context::InstanceVersion instanceVersion;
        instanceVersion.major = VK_API_VERSION_MAJOR(versionBits);
        instanceVersion.minor = VK_API_VERSION_MINOR(versionBits);
        instanceVersion.patch = VK_API_VERSION_PATCH(versionBits);

        return instanceVersion;
    }

    static void QuerySupportedExtensions(std::vector<VkExtensionProperties>* pSupportedExtensions)
    {
        uint32_t allExtensionsCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &allExtensionsCount, nullptr);

        std::vector<VkExtensionProperties> allExtensions(allExtensionsCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &allExtensionsCount, allExtensions.data());

        pSupportedExtensions->swap(allExtensions);
    }

    void Context::createInstance(
        const AppConfig& appConfig,
        const InstanceConfig& instanceConfig)
    {
        VkApplicationInfo appInfo = {};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = appConfig.name.c_str();
        appInfo.applicationVersion =
            VK_MAKE_VERSION(
                appConfig.majorVersion,
                appConfig.minorVersion,
                appConfig.patchVersion);

        appInfo.pEngineName = k_vgfxEngineName;
        appInfo.engineVersion = k_vgfxVersion;

        m_instanceVersion = QueryInstanceVersion();
        if (instanceConfig.minMajorVersion > m_instanceVersion.major
            || (instanceConfig.minMajorVersion == m_instanceVersion.major
                && instanceConfig.minMinorVersion > m_instanceVersion.minor)
            || (instanceConfig.minMajorVersion == m_instanceVersion.major
                && instanceConfig.minMinorVersion == m_instanceVersion.minor
                && instanceConfig.minPatchVersion > m_instanceVersion.patch))
        {
            throw std::runtime_error("Minimum required API version not supported!");
        }

        appInfo.apiVersion =
            VK_MAKE_API_VERSION(
                0,
                m_instanceVersion.major,
                m_instanceVersion.minor,
                instanceConfig.minPatchVersion);

        VkInstanceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        std::vector<VkExtensionProperties> supportedExtensions;
        QuerySupportedExtensions(&supportedExtensions);

        std::vector<const char*> instanceExtensionsAsCharPtrs;
        if (!instanceConfig.requiredExtensions.empty()) {
            checkInstanceExtensionSupport(supportedExtensions, instanceConfig.requiredExtensions);
            ContainerOfStringsToCharPtrs(instanceConfig.requiredExtensions, &instanceExtensionsAsCharPtrs);
        }
        // Make sure dynamic rendering is available
        // (either API v1.3 or greater, or the dynamic rendering extension is required)
        if (m_instanceVersion.major < 1 || (m_instanceVersion.major == 1 && m_instanceVersion.minor < 3)) {
            checkInstanceExtensionSupport(supportedExtensions, {VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME});
            instanceExtensionsAsCharPtrs.push_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
        }

        std::vector<std::string> supportedValidationLayers;
        if (appConfig.enableValidationLayers) {
            checkInstanceExtensionSupport(supportedExtensions, {VK_EXT_DEBUG_REPORT_EXTENSION_NAME});
            instanceExtensionsAsCharPtrs.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
            addSupportedValidationLayers({"VK_LAYER_KHRONOS_validation"}, supportedValidationLayers);
        }

        createInfo.ppEnabledExtensionNames = instanceExtensionsAsCharPtrs.data();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensionsAsCharPtrs.size());

        if (!instanceConfig.validationLayers.empty()) {
            addSupportedValidationLayers(instanceConfig.validationLayers, supportedValidationLayers);
        }

        std::vector<const char*> validationLayersAsCharPtrs;
        ContainerOfStringsToCharPtrs(supportedValidationLayers, &validationLayersAsCharPtrs);

        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayersAsCharPtrs.size());
        createInfo.ppEnabledLayerNames = validationLayersAsCharPtrs.data();

        VkResult result = vkCreateInstance(&createInfo, nullptr, &m_instance);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to create instance!");
        }

        if (m_instanceVersion.major < 1 || (m_instanceVersion.major == 1 && m_instanceVersion.minor < 3)) {
            m_vkCmdBeginRendering = reinterpret_cast<PFN_vkCmdBeginRenderingKHR>(vkGetInstanceProcAddr(m_instance, "vkCmdBeginRenderingKHR"));
            m_vkCmdEndRendering = reinterpret_cast<PFN_vkCmdEndRenderingKHR>(vkGetInstanceProcAddr(m_instance, "vkCmdEndRenderingKHR"));
            std::cerr << "GetProcAddr" << (void*)m_vkCmdBeginRendering << std::endl;
            if (!m_vkCmdBeginRendering || !m_vkCmdEndRendering) {
                throw std::runtime_error("Unable to dynamically load vkCmdBeginRenderingKHR and vkCmdEndRenderingKHR");
            }
#if VK_HEADER_VERSION >= 313
        } else {
            m_vkCmdBeginRendering = vkCmdBeginRendering;
            m_vkCmdEndRendering = vkCmdEndRendering;
            std::cerr << "313: " << (void*)m_vkCmdBeginRendering << std::endl;
#endif
        }
    }

    void Context::destroyInstance()
    {
        if (m_instance != VK_NULL_HANDLE) {
            vkDestroyInstance(m_instance, nullptr);
        }
    }

    void Context::checkInstanceExtensionSupport(
        const std::vector<VkExtensionProperties>& supportedExtensions,
        const std::vector<std::string>& requiredExtensions)
    {
        std::set<std::string> requiredExtensionSet(requiredExtensions.begin(), requiredExtensions.end());

        for (const auto& vkExtension : supportedExtensions) {
            requiredExtensionSet.erase(vkExtension.extensionName);
        }

        if (requiredExtensionSet.size() > 0) {
            for (const auto& requiredExtension : requiredExtensionSet) {
                std::cerr << "Required extension not supported: " << requiredExtension << std::endl;
            }
            throw std::runtime_error("Required extensions are not supported.");
        }
    }

    void Context::addSupportedValidationLayers(
        const std::vector<std::string>& validationLayers,
        std::vector<std::string>& requestedAndAvailableLayers)
    {
        uint32_t availableLayerCount;
        vkEnumerateInstanceLayerProperties(&availableLayerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(availableLayerCount);
        vkEnumerateInstanceLayerProperties(&availableLayerCount, availableLayers.data());

        std::set<std::string> requestedLayers(
            validationLayers.begin(), validationLayers.end());

        for (const auto& vkLayer : availableLayers) {
            if (requestedLayers.erase(vkLayer.layerName) > 0) {
                requestedAndAvailableLayers.push_back(vkLayer.layerName);
            }
        }

        for (const auto& requestedLayer : requestedLayers) {
            std::cerr << "Requested validation layer not supported: " << requestedLayer << std::endl;
        }
    }

    void Context::pickPhysicalDevice(
        const DeviceConfig& deviceConfig,
        const Renderer& renderer)
    {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
        if (deviceCount == 0) {
            throw std::runtime_error("Failed to find GPUs with Vulkan support!");
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

        for (const auto& device : devices) {
            VkPhysicalDeviceProperties physicalDeviceProperties = {};

            vkGetPhysicalDeviceProperties(device, &physicalDeviceProperties);

            std::vector<VkQueueFamilyProperties> queueFamilyProperties;
            std::cout << "Checking device: " << physicalDeviceProperties.deviceName << std::endl;
            if (isDeviceSuitable(
                    device,
                    physicalDeviceProperties.vendorID,
                    physicalDeviceProperties.deviceType,
                    deviceConfig,
                    renderer,
                    &queueFamilyProperties)) {
                m_physicalDevice = device;

                std::cout << "Device is suitable. Device properties: " << std::endl << "  "
                    << "apiVersion: " << physicalDeviceProperties.apiVersion << std::endl << "  "
                    << "driverVersion: " << physicalDeviceProperties.driverVersion << std::endl << "  "
                    << "vendorID: " << physicalDeviceProperties.vendorID << std::endl << "  "
                    << "deviceID: " << physicalDeviceProperties.deviceID << std::endl << "  "
                    << "deviceType: " << physicalDeviceProperties.deviceType << std::endl;
 
                // Now that we have the required queue family indices, save the
                // properties of each queue to use when we create the logical device.
                if (m_queueFamilyIndices.graphicsFamily.has_value()) {
                    m_queueFamilyProperties[m_queueFamilyIndices.graphicsFamily.value()] =
                        queueFamilyProperties[m_queueFamilyIndices.graphicsFamily.value()];
                }

                if (m_queueFamilyIndices.computeFamily.has_value()) {
                    m_queueFamilyProperties[m_queueFamilyIndices.computeFamily.value()] =
                        queueFamilyProperties[m_queueFamilyIndices.computeFamily.value()];
                }

                if (m_queueFamilyIndices.transferFamily.has_value()) {
                    m_queueFamilyProperties[m_queueFamilyIndices.transferFamily.value()] =
                        queueFamilyProperties[m_queueFamilyIndices.transferFamily.value()];
                }

                if (m_queueFamilyIndices.presentFamily.has_value()) {
                    m_queueFamilyProperties[m_queueFamilyIndices.presentFamily.value()] =
                        queueFamilyProperties[m_queueFamilyIndices.presentFamily.value()];
                }
                break;
            }
        }

        if (m_physicalDevice == VK_NULL_HANDLE) {
            throw std::runtime_error("Failed to find a suitable GPU!");
        }
    }

    bool Context::isDeviceSuitable(
        VkPhysicalDevice device,
        uint32_t vendorId,
        VkPhysicalDeviceType deviceType,
        const DeviceConfig& deviceConfig,
        const Renderer& renderer,
        std::vector<VkQueueFamilyProperties>* pQueueFamilyProperties)
    {

        try {
            if (deviceConfig.requiredVendorId.has_value()
                && deviceConfig.requiredVendorId != vendorId) {
                throw std::runtime_error("Device is from wrong vendor.");
            }

            if (deviceConfig.requiredDeviceType.has_value()
                && deviceConfig.requiredDeviceType != deviceType) {
                throw std::runtime_error("Device is not correct type.");
            }

            m_queueFamilyIndices =
                checkQueueFamilySupport(
                    device,
                    deviceConfig,
                    renderer,
                    pQueueFamilyProperties);

            checkDeviceExtensionSupport(device, deviceConfig);

            renderer.getPresenter().checkIsDeviceSuitable(device);

            checkDeviceFeatureSupport(device, deviceConfig);
        } catch (const std::exception& exc) {
            std::cout << "Device is not suitable: " << exc.what() << std::endl;
            return false;
        }

        return true;
    }

    Context::QueueFamilyIndices Context::checkQueueFamilySupport(
        VkPhysicalDevice device,
        const DeviceConfig& deviceConfig,
        const Renderer& renderer,
        std::vector<VkQueueFamilyProperties>* pQueueFamilyProperties)
    {
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilyProperties.data());

        Context::QueueFamilyIndices queueFamilyIndices;
        for (auto i = 0u; i < queueFamilyProperties.size(); ++i) {
            const auto& queueFamily = queueFamilyProperties[i];

            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT
                && deviceConfig.graphicsQueueRequired
                && !queueFamilyIndices.graphicsFamily.has_value()) {
                queueFamilyIndices.graphicsFamily = i;
            }

            if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) {
                if (!deviceConfig.dedicatedComputeQueueRequired
                    || (queueFamilyIndices.graphicsFamily != i
                        && queueFamilyIndices.transferFamily != i
                        && !queueFamilyIndices.computeFamily.has_value())) {
                    queueFamilyIndices.computeFamily = i;
                }
            }

            if (queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT) {
                if (!deviceConfig.dedicatedTransferQueueRequired
                    || (queueFamilyIndices.graphicsFamily != i
                        && queueFamilyIndices.computeFamily != i
                        && !queueFamilyIndices.transferFamily.has_value())) {
                    queueFamilyIndices.transferFamily = i;
                }
            }

            bool presentQueueIsRequired = renderer.getPresenter().presentQueueIsRequired();
            if (presentQueueIsRequired) {
                bool presentSupport = renderer.getPresenter().queueFamilySupportsPresent(device, i);

                if (presentSupport &&
                    (!queueFamilyIndices.presentFamily.has_value()
                     || !(queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT))) {
                    // If the presentFamily has not been set yet, then set it to this family,
                    // if it has been set, but this family is not also a graphics queue then
                    // use it instead.
                    queueFamilyIndices.presentFamily = i;
                }
            }

            if (queueFamilyIndices.isComplete(deviceConfig, presentQueueIsRequired)) {
                *pQueueFamilyProperties = std::move(queueFamilyProperties);
                return queueFamilyIndices;
            }
        }

        throw std::runtime_error("Failed to find suitable set of queues for this device.");
    }

    bool Context::QueueFamilyIndices::isComplete(
        const DeviceConfig& deviceConfig,
        bool presentQueueRequired) const
    {
        return (!deviceConfig.graphicsQueueRequired || this->graphicsFamily.has_value())
            && (!presentQueueRequired || this->presentFamily.has_value())
            && (!deviceConfig.dedicatedComputeQueueRequired || this->computeFamily.has_value())
            && (!deviceConfig.dedicatedTransferQueueRequired || this->transferFamily.has_value());
    }

    void Context::checkDeviceExtensionSupport(
        VkPhysicalDevice device,
        const Context::DeviceConfig& deviceConfig)
    {
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableDeviceExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableDeviceExtensions.data());

        // create set of required extensions, then go thru available ones and delete them
        // if any left after deletion, where missing something.
        std::set<std::string> missingDeviceExtensions(deviceConfig.requiredDeviceExtensions.begin(), deviceConfig.requiredDeviceExtensions.end());
        for (const auto& availableExtension : availableDeviceExtensions) {
            missingDeviceExtensions.erase(availableExtension.extensionName);
        }

        if (!missingDeviceExtensions.empty()) {
            std::stringstream error;
            error << "Missing required Vulkan extensions:";
            for (const auto& missingExtension : missingDeviceExtensions) {
                error << "  " << missingExtension;
            }
            throw std::runtime_error(error.str());
        }
    }

    void Context::checkDeviceFeatureSupport(
        VkPhysicalDevice device,
        const Context::DeviceConfig& deviceConfig)
    {
        VkPhysicalDeviceFeatures supportedFeatures;
        memset(&supportedFeatures, 0, sizeof(supportedFeatures));

        vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

        VkBool32* pCurSupportedFeature = &supportedFeatures.robustBufferAccess;
        VkBool32* pSupportedFeaturesEnd = &supportedFeatures.inheritedQueries;
        ++pSupportedFeaturesEnd;

        const VkBool32* pCurRequiredFeature = &deviceConfig.requiredDeviceFeatures.robustBufferAccess;
        while (pCurSupportedFeature != pSupportedFeaturesEnd) {
            if (*pCurRequiredFeature != 0u && *pCurSupportedFeature == 0u) {
                throw std::runtime_error("Device does not support all required features.");
            }
            ++pCurRequiredFeature;
            ++pCurSupportedFeature;
        }
        // Used by ImageDownsampler.
        if (!supportedFeatures.samplerAnisotropy) {
            throw std::runtime_error("Device does not support sampler anisotropy.");
        }
        // Used by ImageSharpener.
        if (!supportedFeatures.shaderInt16) {
            throw std::runtime_error("Device does not support shader int16.");
        }
    }

    static bool CheckExtensionSupport(VkPhysicalDevice device, const char* findExtensions[], size_t findCount)
    {
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableDeviceExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableDeviceExtensions.data());

        size_t foundExtCount = 0u;
        for (size_t i = 0; i < availableDeviceExtensions.size(); ++i) {
            for (size_t j = 0; j < findCount; ++j) {
                if (_stricmp(availableDeviceExtensions[i].extensionName, findExtensions[j]) == 0) {
                    ++foundExtCount;
                    if (foundExtCount == findCount) {
                        return true;
                    }
                }
            }
        }

        return false;
    }

    static bool TryAddFp16DeviceExtensions(
        VkPhysicalDevice device,
        std::vector<const char*>* pExtOut)
    {
        VkPhysicalDeviceFeatures2 features = {};
        features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

        // Query 16 bit storage
        VkPhysicalDevice16BitStorageFeatures storage16BitFeatures = {};
        storage16BitFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES;

        features.pNext = &storage16BitFeatures;
        vkGetPhysicalDeviceFeatures2(device, &features);

        if (!storage16BitFeatures.storageBuffer16BitAccess) {
            return false;
        }

        // Query 16 bit ops
        VkPhysicalDeviceFloat16Int8FeaturesKHR fp16Features = {};
        fp16Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT16_INT8_FEATURES_KHR;

        features.pNext = &fp16Features;
        vkGetPhysicalDeviceFeatures2(device, &features);

        if (!fp16Features.shaderFloat16) {
            return false;
        }

        static const char* fp16Extensions[] = { VK_KHR_16BIT_STORAGE_EXTENSION_NAME, VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME };
        size_t extCount = sizeof(fp16Extensions) / sizeof(fp16Extensions[0]);
        
        if (CheckExtensionSupport(device, fp16Extensions, extCount)) {
            pExtOut->insert(pExtOut->end(), fp16Extensions, &fp16Extensions[extCount]);
            return true;
        }

        return false;
    }

    static bool TryAddShaderSubgroupExtension(
        VkPhysicalDevice device,
        std::vector<const char*>* pExtOut)
    {
        VkPhysicalDeviceFeatures2 features = {};
        features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

        VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures shaderSubgroupFeatures = {};
        shaderSubgroupFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SUBGROUP_EXTENDED_TYPES_FEATURES;

        features.pNext = &shaderSubgroupFeatures;
        vkGetPhysicalDeviceFeatures2(device, &features);

        if (!shaderSubgroupFeatures.shaderSubgroupExtendedTypes) {
            return false;
        }

        static const char* shaderSubgroupExt[] = {
            VK_KHR_SHADER_SUBGROUP_EXTENDED_TYPES_EXTENSION_NAME,
        };
        size_t extCount = sizeof(shaderSubgroupExt) / sizeof(shaderSubgroupExt[0]);

        if (CheckExtensionSupport(device, shaderSubgroupExt, extCount)) {
            pExtOut->insert(pExtOut->end(), shaderSubgroupExt, &shaderSubgroupExt[extCount]);
            return true;
        }

        return false;
    }

    static bool TryAddDescriptorIndexingExtension(
        VkPhysicalDevice device,
        std::vector<const char*>* pExtOut)
    {
        VkPhysicalDeviceFeatures2 features = {};
        features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

        VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexingFeatures = {};
        descriptorIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;

        features.pNext = &descriptorIndexingFeatures;
        vkGetPhysicalDeviceFeatures2(device, &features);

        if (!descriptorIndexingFeatures.descriptorBindingPartiallyBound) {
            return false;
        }

        static const char* extArray[] = {
            VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
        };

        size_t extCount = sizeof(extArray) / sizeof(extArray[0]);

        if (CheckExtensionSupport(device, extArray, extCount)) {
            pExtOut->insert(pExtOut->end(), extArray, &extArray[extCount]);
            return true;
        }

        return false;
    }

    void Context::createLogicalDevice(
        const Context::DeviceConfig& deviceConfig)
    {
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::vector<std::vector<float>> priorityVectors;
        for (const auto& itr : m_queueFamilyProperties) {
            VkDeviceQueueCreateInfo queueCreateInfo = {};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;

            const QueueFamilyIndex& queueFamilyIndex = itr.first;
            queueCreateInfo.queueFamilyIndex = queueFamilyIndex;

            const VkQueueFamilyProperties& queueFamily = itr.second;

            priorityVectors.resize(priorityVectors.size() + 1);
            priorityVectors.back().resize(queueFamily.queueCount, 1.0f);
            queueCreateInfo.pQueuePriorities = priorityVectors.back().data();

            queueCreateInfo.queueCount = queueFamily.queueCount;

            queueCreateInfos.push_back(queueCreateInfo);
        }

        VkDeviceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.pEnabledFeatures = nullptr;

        VkPhysicalDeviceFeatures2 physDevFeatures = {};
        physDevFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        physDevFeatures.features = deviceConfig.requiredDeviceFeatures;

        // Used by ImageDownsampler.
        physDevFeatures.features.samplerAnisotropy = VK_TRUE;
        // Used by ImageSharpener.
        physDevFeatures.features.shaderInt16 = VK_TRUE;

        VkPhysicalDeviceDynamicRenderingFeaturesKHR dynRenderingFeatures = {};
        dynRenderingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
        dynRenderingFeatures.dynamicRendering = VK_TRUE;
        dynRenderingFeatures.pNext = nullptr;

        VkPhysicalDeviceDynamicRenderingFeaturesKHR dynRenderingFeaturesKHR{};
        dynRenderingFeaturesKHR.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
        dynRenderingFeaturesKHR.dynamicRendering = VK_TRUE;
        dynRenderingFeaturesKHR.pNext = nullptr;

        createInfo.pNext = &physDevFeatures;

        void** ppDevFeaturesNext = nullptr;
        if (m_instanceVersion.major < 1 || (m_instanceVersion.major == 1 && m_instanceVersion.minor < 3)) {
            physDevFeatures.pNext = &dynRenderingFeaturesKHR;
            ppDevFeaturesNext = &dynRenderingFeaturesKHR.pNext;
        } else {
            physDevFeatures.pNext = &dynRenderingFeatures;
            ppDevFeaturesNext = &dynRenderingFeatures.pNext;
        }

        std::vector<const char*> deviceExtensionsAsCharPtrs;
        if (!deviceConfig.requiredDeviceExtensions.empty()) {
            createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceConfig.requiredDeviceExtensions.size());
            ContainerOfStringsToCharPtrs(deviceConfig.requiredDeviceExtensions, &deviceExtensionsAsCharPtrs);
        }

        // TODO add way for internal components to register to add extensions and/or features.
        VkPhysicalDevice16BitStorageFeatures storage16BitFeatures = {};
        VkPhysicalDeviceFloat16Int8FeaturesKHR fp16Features = {};
        if (TryAddFp16DeviceExtensions(
                m_physicalDevice,
                &deviceExtensionsAsCharPtrs)) {
            // Add fp16 device features.
            m_fp16IsSupported = true;
            storage16BitFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES;
            storage16BitFeatures.storageBuffer16BitAccess = VK_TRUE;

            fp16Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT16_INT8_FEATURES_KHR;
            fp16Features.shaderFloat16 = VK_TRUE;

            *ppDevFeaturesNext = &storage16BitFeatures;
            storage16BitFeatures.pNext = &fp16Features;
            ppDevFeaturesNext = &fp16Features.pNext;
        } else {
            m_fp16IsSupported = false;
        }

        VkPhysicalDeviceShaderSubgroupExtendedTypesFeaturesKHR shaderSubgroupExtendedType = {};
        if (TryAddShaderSubgroupExtension(
                m_physicalDevice,
                &deviceExtensionsAsCharPtrs)) {
            // Add shader subgroup device feature.
            m_shaderSubgroupsAreSupported = true;

            shaderSubgroupExtendedType.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SUBGROUP_EXTENDED_TYPES_FEATURES_KHR;
            shaderSubgroupExtendedType.shaderSubgroupExtendedTypes = VK_TRUE;

            *ppDevFeaturesNext = &shaderSubgroupExtendedType;
            ppDevFeaturesNext = &shaderSubgroupExtendedType.pNext;
        }

        VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexingFeatures = {};
        if (TryAddDescriptorIndexingExtension(
                m_physicalDevice,
                &deviceExtensionsAsCharPtrs)) {
            // Add descriptor indexing feature.
            m_descriptorIndexingIsSupported = true;
            descriptorIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
            descriptorIndexingFeatures.descriptorBindingPartiallyBound = VK_TRUE;

            *ppDevFeaturesNext = &descriptorIndexingFeatures;
            ppDevFeaturesNext = &descriptorIndexingFeatures.pNext;
        }

        createInfo.ppEnabledExtensionNames = deviceExtensionsAsCharPtrs.data();

        createInfo.enabledLayerCount = 0;

        VkResult result =
            vkCreateDevice(
                m_physicalDevice,
                &createInfo,
                nullptr,
                &m_device);

        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to create logical device!");
        }
    }

    static VkResult CreateDebugReportCallbackEXT(
        VkInstance instance,
        const VkDebugReportCallbackCreateInfoEXT* pCreateInfo,
        const VkAllocationCallbacks* pAllocator,
        VkDebugReportCallbackEXT* pCallback)
    {
        auto func = (PFN_vkCreateDebugReportCallbackEXT)
            vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");
        if (func != nullptr) {
            return func(instance, pCreateInfo, pAllocator, pCallback);
        } else {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
    }

    void Context::enableDebugReportCallback(
        PFN_vkDebugReportCallbackEXT pfnCallback,
        void* pUserData)
    {
        VkDebugReportCallbackCreateInfoEXT createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
        createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
        createInfo.pfnCallback = pfnCallback;
        createInfo.pUserData = pUserData;

        if (CreateDebugReportCallbackEXT(m_instance, &createInfo, nullptr, &m_debugReportCallback) != VK_SUCCESS) {
            throw std::runtime_error("Debug report callback is not supported!");
        }
    }

    static void DestroyDebugReportCallbackEXT(
        VkInstance instance,
        VkDebugReportCallbackEXT callback,
        const VkAllocationCallbacks* pAllocator)
    {
        auto func = (PFN_vkDestroyDebugReportCallbackEXT)
            vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
        if (func != nullptr) {
            func(instance, callback, pAllocator);
        }
    }

    void Context::disableDebugReportCallback()
    {
        if (m_debugReportCallback != VK_NULL_HANDLE) {
            DestroyDebugReportCallbackEXT(
                m_instance, m_debugReportCallback, nullptr);
            m_debugReportCallback = VK_NULL_HANDLE;
        }
    }

    CommandQueue Context::getGraphicsQueue(uint32_t queueIndex)
    {
        uint32_t maxQueueCount = getGraphicsQueueMaxCount();
        if (queueIndex < maxQueueCount) {
            CommandQueue commandQueue(m_queueFamilyIndices.graphicsFamily.value());
            vkGetDeviceQueue(
                m_device,
                commandQueue.queueFamilyIndex,
                queueIndex,
                &commandQueue.queue);

            return commandQueue;
        }

        throw std::runtime_error("Invalid graphics queue index.");
    }

    uint32_t Context::getGraphicsQueueMaxCount() const
    {
        if (m_queueFamilyIndices.graphicsFamily.has_value()) {
            return const_cast<Context*>(this)->m_queueFamilyProperties[
                m_queueFamilyIndices.graphicsFamily.value()
            ].queueCount;
        }
        return 0u;
    }

    CommandQueue Context::getPresentQueue(uint32_t queueIndex)
    {
        uint32_t maxQueueCount = getPresentQueueMaxCount();
        if (queueIndex < maxQueueCount) {
            CommandQueue commandQueue(m_queueFamilyIndices.presentFamily.value());
            vkGetDeviceQueue(
                m_device,
                commandQueue.queueFamilyIndex,
                queueIndex,
                &commandQueue.queue);

            return commandQueue;
        }

        throw std::runtime_error("Invalid present queue index.");
    }

    uint32_t Context::getPresentQueueMaxCount() const
    {
        if (m_queueFamilyIndices.presentFamily.has_value()) {
            return const_cast<Context*>(this)->m_queueFamilyProperties[
                m_queueFamilyIndices.presentFamily.value()
            ].queueCount;
        }
        return 0u;
    }

    CommandQueue Context::getDedicatedComputeQueue(uint32_t queueIndex)
    {
        uint32_t maxQueueCount = getDedicatedComputeQueueMaxCount();
        if (queueIndex < maxQueueCount) {
            CommandQueue commandQueue(m_queueFamilyIndices.computeFamily.value());
            vkGetDeviceQueue(
                m_device,
                commandQueue.queueFamilyIndex,
                queueIndex,
                &commandQueue.queue);

            return commandQueue;
        }

        throw std::runtime_error("Invalid compute queue index.");
    }

    uint32_t Context::getDedicatedComputeQueueMaxCount() const
    {
        if (m_queueFamilyIndices.computeFamily.has_value()) {
            return const_cast<Context*>(this)->m_queueFamilyProperties[
                m_queueFamilyIndices.computeFamily.value()
            ].queueCount;
        }
        return 0u;
    }

    CommandQueue Context::getDedicatedTransferQueue(uint32_t queueIndex)
    {
        uint32_t maxQueueCount = getDedicatedTransferQueueMaxCount();
        if (queueIndex < maxQueueCount) {
            CommandQueue commandQueue(m_queueFamilyIndices.transferFamily.value());
            vkGetDeviceQueue(
                m_device,
                commandQueue.queueFamilyIndex,
                queueIndex,
                &commandQueue.queue);

            return commandQueue;
        }

        throw std::runtime_error("Invalid transfer queue index.");
    }

    uint32_t Context::getDedicatedTransferQueueMaxCount() const
    {
        if (m_queueFamilyIndices.transferFamily.has_value()) {
            return const_cast<Context*>(this)->m_queueFamilyProperties[
                m_queueFamilyIndices.transferFamily.value()
            ].queueCount;
        }
        return 0u;
    }

    void Context::ImageDownsamplerDeleter::operator()(ImageDownsampler* pImageDownsampler)
    {
        if (pImageDownsampler != nullptr) {
            delete pImageDownsampler;
        }
    }
}
