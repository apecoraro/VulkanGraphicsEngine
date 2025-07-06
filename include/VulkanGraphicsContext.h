//
//  Created by Alex Pecoraro on 3/5/20.
//
#pragma once

#include "VulkanGraphicsCommandQueue.h"
#include "VulkanGraphicsMemoryAllocator.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <vulkan/vulkan.h>

namespace vgfx
{
    // Forward declarations
    class Renderer;
    class ImageDownsampler;

    class Context
    {
    public:
        Context() = default;
        ~Context() = default;

        using ValidationLayerFunc = std::function<
            VkBool32(
                VkDebugReportFlagsEXT flags,
                VkDebugReportObjectTypeEXT objType,
                uint64_t obj,
                size_t location,
                int32_t code,
                const char* pLayerPrefix,
                const char* pMsg)>;

        struct AppConfig
        {
            std::string name;
            uint16_t majorVersion = 1u;
            uint16_t minorVersion = 0u;
            uint16_t patchVersion = 0u;
            bool enableValidationLayers = false;
            ValidationLayerFunc onValidationLayerFunc = nullptr;
            std::string dataDirectoryPath = ".";

            AppConfig(
                const std::string& appName,
                uint16_t major=1u,
                uint16_t minor=0u,
                uint16_t patch=0u)
                : name(appName)
                , majorVersion(major)
                , minorVersion(minor)
                , patchVersion(patch)
            {
            }

            AppConfig() = default;
        };

        struct InstanceConfig
        {
            uint16_t minMajorVersion = 1u;
            uint16_t minMinorVersion = 0u;
            uint16_t minPatchVersion = 0u;
            std::vector<std::string> requiredExtensions;
            std::vector<std::string> validationLayers;
        };

        struct DeviceConfig
        {
            std::optional<uint32_t> requiredVendorId;
            std::optional<VkPhysicalDeviceType> requiredDeviceType;
            VkPhysicalDeviceFeatures requiredDeviceFeatures = {};
            std::vector<std::string> requiredDeviceExtensions;
            bool graphicsQueueRequired = true;
            bool dedicatedComputeQueueRequired = false;
            bool dedicatedTransferQueueRequired = false;
        };

        void init(
            const AppConfig& appConfig,
            const InstanceConfig& instanceConfig,
            const DeviceConfig& deviceConfig,
            Renderer* pRenderer);

        void shutdown();

        void waitForDeviceToIdle();

        void enableDebugReportCallback(PFN_vkDebugReportCallbackEXT pfnCallback, void* pUserData);
        void disableDebugReportCallback();

        struct InstanceVersion
        {
            int32_t major = 0;
            int32_t minor = 0;
            int32_t patch = 0;
        };
        const InstanceVersion& getInstanceVersion() const { return m_instanceVersion; }

        VkInstance getInstance() { return m_instance;  }
        VkPhysicalDevice getPhysicalDevice() { return m_physicalDevice;  }
        VkAllocationCallbacks* getAllocationCallbacks() { return m_pAllocationCallbacks;  }
        VkDevice getLogicalDevice() { return m_device;  }

        const bool hasGraphicsQueueFamily() const {
            return m_queueFamilyIndices.graphicsFamily.has_value();
        }
        const uint32_t getGraphicsQueueFamilyIndex() const {
            return m_queueFamilyIndices.graphicsFamily.value();
        }
        CommandQueue getGraphicsQueue(uint32_t index);
        uint32_t getGraphicsQueueMaxCount() const;

        const bool hasPresentQueueFamily() const {
            return m_queueFamilyIndices.presentFamily.has_value();
        }
        const uint32_t getPresentQueueFamilyIndex() const {
            return m_queueFamilyIndices.presentFamily.value();
        }
        CommandQueue getPresentQueue(uint32_t index);
        uint32_t getPresentQueueMaxCount() const;

        const bool hasDedicatedComputeQueueFamily() const {
            return m_queueFamilyIndices.computeFamily.has_value();
        }
        const uint32_t getDedicatedComputeQueueFamilyIndex() const {
            return m_queueFamilyIndices.computeFamily.value();
        }
        CommandQueue getDedicatedComputeQueue(uint32_t index);
        uint32_t getDedicatedComputeQueueMaxCount() const;

        const bool hasDedicatedTransferQueueFamily(uint32_t index) const {
            return m_queueFamilyIndices.transferFamily.value();
        }
        const uint32_t getDedicatedTransferQueueFamilyIndex(uint32_t index) const {
            return m_queueFamilyIndices.transferFamily.value();
        }
        CommandQueue getDedicatedTransferQueue(uint32_t index);
        uint32_t getDedicatedTransferQueueMaxCount() const;

        MemoryAllocator& getMemoryAllocator() { return m_memoryAllocator;  }
        const MemoryAllocator& getMemoryAllocator() const { return m_memoryAllocator;  }

        bool isFp16Supported() const { return m_fp16IsSupported; }

        bool areShaderSubgroupsSupported() const { return m_shaderSubgroupsAreSupported; }

        bool isDescriptorIndexingSupported() const { return m_descriptorIndexingIsSupported;  }

        ImageDownsampler& getOrCreateImageDownsampler();

        const AppConfig& getAppConfig() const { return m_appConfig; }

    private:
        void createInstance(
            const AppConfig& appConfig,
            const InstanceConfig& instanceConfig);

        void destroyInstance();

        void checkInstanceExtensionSupport(
            const std::vector<VkExtensionProperties>& supportedExtensions,
            const std::vector<std::string>& requiredExtensions);

        void addSupportedValidationLayers(
            const std::vector<std::string>& validationLayers,
            std::vector<std::string>& requestedAndAvailableLayers);

        void pickPhysicalDevice(
            const DeviceConfig& deviceConfig,
            const Renderer* pRenderer);

        struct QueueFamilyIndices
        {
            std::optional<uint32_t> graphicsFamily;

            std::optional<uint32_t> presentFamily;

            std::optional<uint32_t> computeFamily;

            std::optional<uint32_t> transferFamily;

            bool isComplete(
                const DeviceConfig& deviceConfig,
                const Renderer* pRenderer) const;
        };

        QueueFamilyIndices checkQueueFamilySupport(
            VkPhysicalDevice device,
            const DeviceConfig& deviceConfig,
            const Renderer* pRenderer,
            std::vector<VkQueueFamilyProperties>* pQueueFamilyProperties);

        bool isDeviceSuitable(
            VkPhysicalDevice device,
            uint32_t vendorId,
            VkPhysicalDeviceType deviceType,
            const DeviceConfig& deviceConfig,
            const Renderer* pRenderer,
            std::vector<VkQueueFamilyProperties>* pQueueFamilyProperties);

        void checkDeviceExtensionSupport(
            VkPhysicalDevice device,
            const Context::DeviceConfig& deviceConfig);

        void checkDeviceFeatureSupport(
            VkPhysicalDevice device,
            const Context::DeviceConfig& deviceConfig);

        void createLogicalDevice(
            const DeviceConfig& deviceConfig);

        AppConfig m_appConfig;

        InstanceVersion m_instanceVersion;
        VkInstance m_instance = VK_NULL_HANDLE;
        VkAllocationCallbacks* m_pAllocationCallbacks = nullptr;
        VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;

        QueueFamilyIndices m_queueFamilyIndices;
        using QueueFamilyIndex = uint32_t;
        std::unordered_map<QueueFamilyIndex, VkQueueFamilyProperties> m_queueFamilyProperties;

        bool m_fp16IsSupported = false;
        bool m_shaderSubgroupsAreSupported = false;
        bool m_descriptorIndexingIsSupported = false;

        VkDebugReportCallbackEXT m_debugReportCallback = VK_NULL_HANDLE;

        VkDevice m_device = VK_NULL_HANDLE;

        MemoryAllocator m_memoryAllocator;

        // TODO this should probably be a component of some type of pluggable system.
        struct ImageDownsamplerDeleter
        {
            ImageDownsamplerDeleter() = default;
            void operator()(ImageDownsampler*);
        };
        std::unique_ptr<ImageDownsampler, ImageDownsamplerDeleter> m_spImageDownsampler;
    };
}
