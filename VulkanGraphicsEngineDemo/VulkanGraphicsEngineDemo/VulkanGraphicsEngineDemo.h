#ifndef VGFX_DEMO_H
#define VGFX_DEMO_H

#include "VulkanGraphicsSceneNode.h"

#include <vulkan/vulkan.h>

    std::unique_ptr<vgfx::SceneNode> LoadScene();
    bool DemoInit(
        void* window,
        VkResult(*createVulkanSurface)(VkInstance, void*, const VkAllocationCallbacks*, VkSurfaceKHR*),
        void (*getFramebufferSize)(void*, int*, int*),
        uint32_t platformSpecificExtensionCount,
        const char** platformSpecificExtensions,
        bool enableValidationLayers,
        const char* pDataPath,
        const char* pModelFilename,
        const char* pModelDiffuseTexName,
        const char* pVertShaderFilename,
        const char* pFragShaderFilename);

    void DemoSetFramebufferResized(bool flag);

    bool DemoDraw(void);

    void DemoCleanUp(void);

#ifdef __cplusplus
}
#endif

#endif
