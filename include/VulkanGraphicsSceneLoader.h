#pragma once

#include "VulkanGraphicsContext.h"
#include "VulkanGraphicsModelLibrary.h"

#include <string>

namespace vgfx
{
    class SceneLoader
    {
    public:
        SceneLoader(Context& graphicsContext);
        ~SceneLoader();

        std::unique_ptr<SceneNode> loadScene(
            const std::string& filePath);

    private:
        Context& m_graphicsContext;
        std::unique_ptr<ModelLibrary> m_spModelLibrary;
        std::unique_ptr<CommandBufferFactory> m_spCommandBufferFactory;
    };

};
