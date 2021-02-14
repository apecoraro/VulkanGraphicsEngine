#include "VulkanGraphicsContext.h"
//
//  Created by Alex Pecoraro on 3/5/20.
//  Copyright � 2020 Rainway, Inc. All rights reserved.
//
#include "VulkanGraphicsContext.h"

#include "VulkanGraphicsRenderer.h"

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
        Renderer* pRenderer)
    {
        createInstance(
            appConfig,
            instanceConfig);

        if (pRenderer != nullptr) {
            pRenderer->bindToInstance(m_instance, m_pAllocationCallbacks);
        }

        pickPhysicalDevice(
            deviceConfig,
            pRenderer);

        if (pRenderer != nullptr) {
            pRenderer->configureForDevice(m_physicalDevice);
        }

        createLogicalDevice(deviceConfig);

        m_memoryAllocator.init(
            VK_MAKE_VERSION(
                appConfig.majorVersion,
                appConfig.minorVersion,
                appConfig.patchVersion),
            *this);
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
        appInfo.apiVersion = VK_API_VERSION_1_2;

        VkInstanceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        std::vector<const char*> instanceExtensionsAsCharPtrs;
        if (!instanceConfig.requiredExtensions.empty()) {
            checkInstanceExtensionSupport(instanceConfig.requiredExtensions);

            createInfo.enabledExtensionCount = static_cast<uint32_t>(instanceConfig.requiredExtensions.size());

            ContainerOfStringsToCharPtrs(instanceConfig.requiredExtensions, &instanceExtensionsAsCharPtrs);

            createInfo.ppEnabledExtensionNames = instanceExtensionsAsCharPtrs.data();
        }
        else {
            createInfo.enabledExtensionCount = 0u;
            createInfo.ppEnabledExtensionNames = nullptr;
        }

        std::vector<std::string> requestedAndAvailableLayers;
        std::vector<const char*> validationLayersAsCharPtrs;
        if (!instanceConfig.validationLayers.empty()) {
            requestedAndAvailableLayers =
                checkValidationLayerSupport(instanceConfig.validationLayers);

            createInfo.enabledLayerCount = static_cast<uint32_t>(requestedAndAvailableLayers.size());

            ContainerOfStringsToCharPtrs(requestedAndAvailableLayers, &validationLayersAsCharPtrs);

            createInfo.ppEnabledLayerNames = validationLayersAsCharPtrs.data();
        } else {
            createInfo.enabledLayerCount = 0u;
            createInfo.ppEnabledLayerNames = nullptr;
        }

        VkResult result = vkCreateInstance(&createInfo, nullptr, &m_instance);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to create instance!");
        }
    }

    void Context::destroyInstance()
    {
        if (m_instance != VK_NULL_HANDLE) {
            vkDestroyInstance(m_instance, nullptr);
        }
    }

    void Context::checkInstanceExtensionSupport(
        const std::vector<std::string>& requiredExtensions)
    {
        std::set<std::string> requiredExtensionSet(requiredExtensions.begin(), requiredExtensions.end());

        uint32_t allExtensionsCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &allExtensionsCount, nullptr);

        std::vector<VkExtensionProperties> allExtensions(allExtensionsCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &allExtensionsCount, allExtensions.data());

        for (const auto& vkExtension : allExtensions) {
            requiredExtensionSet.erase(vkExtension.extensionName);
        }

        if (requiredExtensionSet.size() > 0) {
            for (const auto& requiredExtension : requiredExtensionSet) {
                std::cerr << "Required extension not supported: " << requiredExtension << std::endl;
            }
            throw std::runtime_error("Required extensions are not supported.");
        }
    }

    std::vector<std::string> Context::checkValidationLayerSupport(
        const std::vector<std::string>& validationLayers)
    {
        uint32_t availableLayerCount;
        vkEnumerateInstanceLayerProperties(&availableLayerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(availableLayerCount);
        vkEnumerateInstanceLayerProperties(&availableLayerCount, availableLayers.data());

        std::set<std::string> requestedLayers(
            validationLayers.begin(), validationLayers.end());

        std::vector<std::string> requestedAndAvailableLayers;
        for (const auto& vkLayer : availableLayers) {
            if (requestedLayers.erase(vkLayer.layerName) > 0) {
                requestedAndAvailableLayers.push_back(vkLayer.layerName);
            }
        }

        for (const auto& requestedLayer : requestedLayers) {
            std::cerr << "Requested validation layer not supported: " << requestedLayer << std::endl;
        }

        return requestedAndAvailableLayers;
    }

    void Context::pickPhysicalDevice(
        const DeviceConfig& deviceConfig,
        const Renderer* pRenderer)
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
                    pRenderer,
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
        const Renderer* pRenderer,
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
                    pRenderer,
                    pQueueFamilyProperties);

            checkDeviceExtensionSupport(device, deviceConfig);

            if (pRenderer != nullptr) {
                pRenderer->checkIsDeviceSuitable(device);
            }

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
        const Renderer* pRenderer,
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

            if (pRenderer != nullptr
                && pRenderer->requiresPresentQueue()) {
                bool presentSupport = pRenderer->queueFamilySupportsPresent(device, i);

                if (presentSupport &&
                    (!queueFamilyIndices.presentFamily.has_value()
                     || !(queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT))) {
                    // If the presentFamily has not been set yet, then set it to this family,
                    // if it has been set, but this family is not also a graphics queue then
                    // use it instead.
                    queueFamilyIndices.presentFamily = i;
                }
            }

            if (queueFamilyIndices.isComplete(deviceConfig, pRenderer)) {
                *pQueueFamilyProperties = std::move(queueFamilyProperties);
                return queueFamilyIndices;
            }
        }

        throw std::runtime_error("Failed to find suitable set of queues for this device.");
    }

    bool Context::QueueFamilyIndices::isComplete(
        const DeviceConfig& deviceConfig,
        const Renderer* pRenderer) const
    {
        bool presentQueueRequired = pRenderer != nullptr && pRenderer->requiresPresentQueue();

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

        createInfo.pEnabledFeatures = &deviceConfig.requiredDeviceFeatures;

        std::vector<const char*> deviceExtensionsAsCharPtrs;
        if (!deviceConfig.requiredDeviceExtensions.empty()) {
            createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceConfig.requiredDeviceExtensions.size());
            ContainerOfStringsToCharPtrs(deviceConfig.requiredDeviceExtensions, &deviceExtensionsAsCharPtrs);
            createInfo.ppEnabledExtensionNames = deviceExtensionsAsCharPtrs.data();
        }
        else {
            createInfo.enabledExtensionCount = 0;
        }

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
        PFN_vkDebugReportCallbackEXT pfnCallback)
    {
        VkDebugReportCallbackCreateInfoEXT createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
        createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
        createInfo.pfnCallback = pfnCallback;

        CreateDebugReportCallbackEXT(m_instance, &createInfo, nullptr, &m_debugReportCallback);
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
}
