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

    using MaterialsTable = std::map<std::pair<Program*, Program*>, std::unique_ptr<Material>>;
    static MaterialsTable s_materialsTable;

    Material& MaterialsDatabase::GetOrLoadMaterial(
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

        const auto& materialId = std::make_pair(&vertexShader, &fragmentShader);
        MaterialsTable::iterator findMaterialItr = s_materialsTable.find(materialId);
        if (findMaterialItr != s_materialsTable.end()) {
            std::unique_ptr<Material>& spMaterial = findMaterialItr->second;
            return *spMaterial.get();
        }

        std::unique_ptr<Material>& spMaterial = s_materialsTable[materialId];
        spMaterial = std::make_unique<Material>(
            vertexShader,
            fragmentShader);

        return *spMaterial.get();
    }

    void MaterialsDatabase::Optimize()
    {
        // Could probably also destroy descriptor set layouts here too.
        for (auto& itr : s_vertexShadersTable) {
            itr.second->destroyShaderModule();
        }

        for (auto& itr : s_fragmentShadersTable) {
            itr.second->destroyShaderModule();
        }
    }

    void MaterialsDatabase::UnloadAll()
    {
        s_vertexShadersTable.clear();
        s_fragmentShadersTable.clear();
        s_materialsTable.clear();
    }
}