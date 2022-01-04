#ifndef VGFX_DEMO_H
#define VGFX_DEMO_H

#include <vulkan/vulkan.h>

#ifdef __cplusplus
extern "C" {
#endif
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
