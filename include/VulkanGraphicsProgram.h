#ifndef VGFX_PROGRAM_H
#define VGFX_PROGRAM_H

#include "VulkanGraphicsContext.h"

#include <cassert>
#include <string>
#include <vector>

namespace vgfx
{
    class Program
    {
    public:
        enum class Type
        {
            Vertex = VK_SHADER_STAGE_VERTEX_BIT,
            Fragment = VK_SHADER_STAGE_FRAGMENT_BIT,
            Compute = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        };

        Program(
            Context& context,
            Program::Type type,
            const std::string& entryPointFuncName,
            const std::vector<char>& spirvCode);
        ~Program()
        {
            destroy();
        }

        VkShaderStageFlagBits getShaderStage() const
        {
            return static_cast<VkShaderStageFlagBits>(m_type);
        }

        const std::string& getEntryPointFunction() const { return m_entryPointerFuncName; }
        
        VkShaderModule getShaderModule() const { return m_shaderModule; }

        void destroyShaderModule()
        {
            destroy();
        }
    private:
        void destroy();

        Context& m_context;

        Type m_type;
        std::string m_entryPointerFuncName;

        VkShaderModule m_shaderModule;
    };
}

#endif
