#include "VulkanGraphicsProgram.h"

#include <fstream>
#include <memory>
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

    static void ReadFile(const std::string& filename, std::vector<char>* pBuffer)
    {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);

        if (!file.is_open()) {
            throw std::runtime_error("Failed to open shader file!");
        }

        auto fileSize = file.tellg();
        pBuffer->resize(fileSize);
        file.seekg(0);

        file.read(pBuffer->data(), fileSize);
        file.close();
    }

    std::unique_ptr<Program> Program::CreateFromFile(
        const std::string& spvFilePath,
        Context& context,
        Program::Type type,
        const std::string& entryPointFuncName)
    {
        std::vector<char> spirvCode;
        ReadFile(spvFilePath, &spirvCode);

        return std::make_unique<Program>(context, type, entryPointFuncName, spirvCode);
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