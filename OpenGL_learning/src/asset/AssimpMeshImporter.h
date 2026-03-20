#pragma once

#include "graphics/MeshSource.h"
#include <filesystem>
#include <string>
#include <unordered_map>

// 前向声明 Assimp 类型
struct aiNode;
struct aiScene;
struct aiMesh;
struct aiMaterial;
enum aiTextureType;

namespace GLRenderer {

class AssimpMeshImporter {
public:
    explicit AssimpMeshImporter(const std::filesystem::path& path);

    Ref<MeshSource> ImportToMeshSource();

private:
    void TraverseNodes(Ref<MeshSource> meshSource,
                       const aiNode* node,
                       const aiScene* scene,
                       const glm::mat4& parentTransform,
                       uint32_t level = 0);

    void ProcessMesh(Ref<MeshSource> meshSource,
                     const aiMesh* mesh,
                     const aiScene* scene,
                     const glm::mat4& transform,
                     uint32_t& baseVertex,
                     uint32_t& baseIndex);

    std::vector<AssetHandle> LoadMaterials(const aiScene* scene);
    AssetHandle LoadMaterialTexture(aiMaterial* mat, aiTextureType type, const aiScene* scene);

    unsigned int TextureFromFile(const char* path);
    unsigned int TextureFromAssimp(const void* texture);

    const std::filesystem::path m_Path;
    std::string m_Directory;
    std::unordered_map<std::string, AssetHandle> m_LoadedTextures;
};

} // namespace GLRenderer
