//
//  Created by Alex Pecoraro on 3/5/20.
//  Copyright � 2020 Rainway, Inc. All rights reserved.
//
#ifndef QUANTUM_GFX_CONTEXT_H
#define QUANTUM_GFX_CONTEXT_H

#include "VulkanGraphicsCommandBuffers.h"
#include "VulkanGraphicsMemoryAllocator.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>

namespace vgfx
{
    class Renderer; // Forward declaration

    class Context
    {
    public:
        Context() = default;
        ~Context() = default;

        struct AppConfig
        {
            std::string name;
            uint16_t majorVersion = 1u;
            uint16_t minorVersion = 0u;
            uint16_t patchVersion = 0u;
            AppConfig(
                const std::string& nm,
                uint16_t major,
                uint16_t minor,
                uint16_t patch)
                : name(nm)
                , majorVersion(major)
                , minorVersion(minor)
                , patchVersion(patch)
            {

            }
        };

        struct InstanceConfig
        {
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

        void enableDebugReportCallback(PFN_vkDebugReportCallbackEXT pfnCallback);
        void disableDebugReportCallback();

        VkInstance getInstance() { return m_instance;  }
        VkPhysicalDevice getPhysicalDevice() { return m_physicalDevice;  }
        VkAllocationCallbacks* getAllocationCallbacks() { return m_pAllocationCallbacks;  }
        VkDevice getLogicalDevice() { return m_device;  }

        const std::optional<uint32_t>& getGraphicsQueueFamilyIndex() const {
            return m_queueFamilyIndices.graphicsFamily;
        }
        CommandQueue getGraphicsQueue(uint32_t index);
        uint32_t getGraphicsQueueMaxCount() const;

        const std::optional<uint32_t>& getPresentQueueFamilyIndex() const {
            return m_queueFamilyIndices.presentFamily;
        }
        CommandQueue getPresentQueue(uint32_t index);
        uint32_t getPresentQueueMaxCount() const;

        const std::optional<uint32_t>& getDedicatedComputeQueueFamilyIndex() const {
            return m_queueFamilyIndices.computeFamily;
        }
        CommandQueue getDedicatedComputeQueue(uint32_t index);
        uint32_t getDedicatedComputeQueueMaxCount() const;

        const std::optional<uint32_t>& getDedicatedTransferQueueFamilyIndex(uint32_t index) const {
            return m_queueFamilyIndices.transferFamily;
        }
        CommandQueue getDedicatedTransferQueue(uint32_t index);
        uint32_t getDedicatedTransferQueueMaxCount() const;

        MemoryAllocator& getMemoryAllocator() { return m_memoryAllocator;  }
        const MemoryAllocator& getMemoryAllocator() const { return m_memoryAllocator;  }
    private:
        void createInstance(
            const AppConfig& appConfig,
            const InstanceConfig& instanceConfig);

        void destroyInstance();

        void checkInstanceExtensionSupport(
            const std::vector<std::string>& requiredExtensions);

        std::vector<std::string> checkValidationLayerSupport(
            const std::vector<std::string>& validationLayers);

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

        VkInstance m_instance = VK_NULL_HANDLE;
        VkAllocationCallbacks* m_pAllocationCallbacks = nullptr;
        VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;

        QueueFamilyIndices m_queueFamilyIndices;
        using QueueFamilyIndex = uint32_t;
        std::unordered_map<QueueFamilyIndex, VkQueueFamilyProperties> m_queueFamilyProperties;

        VkDebugReportCallbackEXT m_debugReportCallback = VK_NULL_HANDLE;

        VkDevice m_device = VK_NULL_HANDLE;

        MemoryAllocator m_memoryAllocator;
    };
}

#endif
