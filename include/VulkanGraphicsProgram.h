#ifndef VGFX_PROGRAM_H
#define VGFX_PROGRAM_H

#include "VulkanGraphicsContext.h"

#include <cassert>
#include <vector>

namespace vgfx
{
    class Program
    {
    public:
        enum class Type
        {
            VERTEX,
            FRAGMENT
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
            if (m_type == Type::VERTEX) {
                return VK_SHADER_STAGE_VERTEX_BIT;
            } else if (m_type == Type::FRAGMENT) {
                return VK_SHADER_STAGE_FRAGMENT_BIT;
            }
            assert(false && "Unhandled Type!");
            return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
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
