#include "VulkanGraphicsMaterials.h"

#include <fstream>
#include <map>

namespace vgfx
{
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

    using ShaderProgramsTable = std::map<std::string, std::unique_ptr<Program>>;

    static Program& GetOrLoadShaderProgram(
        ShaderProgramsTable& shaderProgramsTable,
        Context& context,
        const std::string& shaderPath,
        Program::Type shaderType,
        const std::string& entryPointFunc)
    {
        ShaderProgramsTable::iterator findShaderItr = shaderProgramsTable.find(shaderPath);
        if (findShaderItr != shaderProgramsTable.end()) {
            std::unique_ptr<Program>& spShader = findShaderItr->second;
            return *spShader.get();
        }

        std::vector<char> shaderCode;
        ReadFile(shaderPath, &shaderCode);

        std::unique_ptr<Program>& spShader = shaderProgramsTable[shaderPath];
        spShader =
            std::make_unique<Program>(
                context,
                shaderType,
                entryPointFunc,
                shaderCode);
        return *spShader.get();
    }

    static ShaderProgramsTable s_vertexShadersTable;

    static Program& GetOrLoadVertexShader(
        Context& context,
        const std::string& vertexShaderPath,
        const std::string& vertexShaderEntryPointFunc)
    {
        return GetOrLoadShaderProgram(
            s_vertexShadersTable,
            context,
            vertexShaderPath,
            Program::Type::VERTEX,
            vertexShaderEntryPointFunc);
    }

    static ShaderProgramsTable s_fragmentShadersTable;

    static Program& GetOrLoadFragmentShader(
        Context& context,
        const std::string& fragmentShaderPath,
        const std::string& fragmentShaderEntryPointFunc)
    {
        return GetOrLoadShaderProgram(
            s_fragmentShadersTable,
            context,
            fragmentShaderPath,
            Program::Type::FRAGMENT,
            fragmentShaderEntryPointFunc);
    }

    using MaterialsTable = std::map<MaterialId, std::unique_ptr<Material>>;
    static MaterialsTable s_materialsTable;

    Material& MaterialsLibrary::GetOrLoadMaterial(
        Context& context,
        const MaterialInfo& materialInfo)
    {
        Program& vertexShader =
            GetOrLoadVertexShader(
                context,
                materialInfo.vertexShaderPath,
                materialInfo.vertexShaderEntryPointFunc);

        Program& fragmentShader =
            GetOrLoadFragmentShader(
                context,
                materialInfo.fragmentShaderPath,
                materialInfo.fragmentShaderEntryPointFunc);

        const auto& materialId = MaterialId(&vertexShader, &fragmentShader);
        MaterialsTable::iterator findMaterialItr = s_materialsTable.find(materialId);
        if (findMaterialItr != s_materialsTable.end()) {
            std::unique_ptr<Material>& spMaterial = findMaterialItr->second;
            return *spMaterial.get();
        }

        std::vector<Material::DescriptorSetLayoutInfo> descriptorSetLayouts;
        for (const auto& descSetLayoutBindingInfo : materialInfo.descriptorSetLayoutBindings) {
            descriptorSetLayouts.push_back(
                Material::DescriptorSetLayoutInfo(
                    std::make_unique<DescriptorSetLayout>(context, descSetLayoutBindingInfo.descriptorSetLayoutBindings),
                    descSetLayoutBindingInfo.copyCount));
        }

        std::unique_ptr<Material>& spMaterial = s_materialsTable[materialId];
        spMaterial =
            std::make_unique<Material>(
                vertexShader,
                fragmentShader,
                std::move(descriptorSetLayouts),
                materialInfo.imageTypes);

        return *spMaterial.get();
    }

    void MaterialsLibrary::Optimize()
    {
        // Could probably also destroy descriptor set layouts here too.
        for (auto& itr : s_vertexShadersTable) {
            itr.second->destroyShaderModule();
        }

        for (auto& itr : s_fragmentShadersTable) {
            itr.second->destroyShaderModule();
        }
    }

    void MaterialsLibrary::UnloadAll()
    {
        s_vertexShadersTable.clear();
        s_fragmentShadersTable.clear();
        s_materialsTable.clear();
    }
}