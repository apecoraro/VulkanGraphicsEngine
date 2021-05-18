#include "VulkanGraphicsMaterials.h"

#include <map>

namespace vgfx
{
    using ShaderProgramsTable = std::map<std::string, std::unique_ptr<Program>>;

    static Program& GetOrLoadShaderProgram(
        ShaderProgramsTable& shaderProgramsTable,
        Context& context,
        const std::string& shader,
        Program::Type shaderType,
        const std::string& entryPointFunc)
    {
        std::string shaderPath = context.getAppConfig().dataDirectoryPath + "/" + shader;

        ShaderProgramsTable::iterator findShaderItr = shaderProgramsTable.find(shaderPath);
        if (findShaderItr != shaderProgramsTable.end()) {
            std::unique_ptr<Program>& spShader = findShaderItr->second;
            return *spShader.get();
        }

        shaderProgramsTable[shaderPath] = Program::CreateFromFile(shaderPath, context, shaderType, entryPointFunc);

        return *shaderProgramsTable[shaderPath];
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
            Program::Type::Vertex,
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
            Program::Type::Fragment,
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

        DescriptorSetLayouts descriptorSetLayouts;
        for (const auto& descSetLayoutBindingInfo : materialInfo.descriptorSetLayoutBindings) {
            descriptorSetLayouts.push_back(
                DescriptorSetLayoutInfo(
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