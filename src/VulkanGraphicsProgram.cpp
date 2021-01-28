#include "VulkanGraphicsProgram.h"

#include <stdexcept>

namespace vgfx
{
    static VkShaderModule CreateShaderModule(Context& context, const std::vector<char>& code)
    {
        VkShaderModuleCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
        VkShaderModule shaderModule;
        if (vkCreateShaderModule(
            context.getLogicalDevice(),
            &createInfo,
            context.getAllocationCallbacks(),
            &shaderModule) != VK_SUCCESS) {
            throw std::runtime_error("failed to create shader module!");
        }

        return shaderModule;
    }

    Program::Program(
        Context& context,
        Type type,
        const std::string& entryPointFuncName,
        const std::vector<char>& spirvCode)
        : m_context(context)
        , m_type(type)
        , m_entryPointerFuncName(entryPointFuncName)
    {
        m_shaderModule = CreateShaderModule(context, spirvCode);
    }

    void Program::destroy()
    {
        VkDevice device = m_context.getLogicalDevice();
        VkAllocationCallbacks* pAllocationCallbacks = m_context.getAllocationCallbacks();

        if (m_shaderModule != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device, m_shaderModule, pAllocationCallbacks);
            m_shaderModule = VK_NULL_HANDLE;
        }
    }
}