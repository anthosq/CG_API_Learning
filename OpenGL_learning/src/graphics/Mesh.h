#pragma once

// [DEPRECATED] 旧版 Mesh/Model 系统，将被 MeshSource/StaticMesh 替代
// 新代码请使用:
//   - MeshSource: 原始网格数据
//   - StaticMesh: 可渲染的静态网格
//   - AssimpMeshImporter: Assimp 导入器

#include "utils/GLCommon.h"
#include "graphics/Shader.h"
#include "core/AABB.h"
#include "core/Ref.h"
#include <vector>
#include <string>
#include <filesystem>
#include <glm/glm.hpp>
#include <unordered_map>

#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/config.h>

#ifdef USE_GLI
#include <gli/gli.hpp>
#endif

// 指定Bone影响的最大数量
#define MAX_BONE_INFLUENCE 4

namespace GLRenderer {
    // 网格顶点结构（用于模型加载）
    struct MeshVertex {
        glm::vec3 Position;
        glm::vec3 Normal;
        glm::vec2 TexCoords;
        glm::vec3 Tangent;
        glm::vec3 Bitangent;

        glm::vec4 BoneIDs[MAX_BONE_INFLUENCE];
        glm::vec4 Weights[MAX_BONE_INFLUENCE];
    };

    // 网格纹理结构（避免与 GLRenderer::Texture 冲突）
    struct MeshTexture {
        unsigned int id;
        std::string type;
        aiString path;
    };

    // [DEPRECATED] 使用 StaticMesh 替代
    class Mesh {
    public:
        std::vector<MeshVertex> vertices;
        std::vector<unsigned int> indices;
        std::vector<MeshTexture> textures;
        AABB BoundingBox;  // 包围盒

        Mesh(std::vector<MeshVertex> vertices, std::vector<unsigned int> indices, std::vector<MeshTexture> textures);
        void Draw(const Shader &shader);

        // 获取包围盒
        const AABB& GetBoundingBox() const { return BoundingBox; }

    private:
        unsigned int VAO, VBO, EBO;
        void SetupMesh();
        void CalculateBoundingBox();  // 从顶点计算包围盒
    };

    // [DEPRECATED] 使用 MeshSource + AssimpMeshImporter 替代
    class Model : public RefCounter {
    public:
        Model() = default;
        Model(const std::filesystem::path &path, bool gamma = false) : gamma_correction(gamma) { LoadModel(path); }
        void Draw(const Shader &shader);

        bool HasDiffuseTextures() const { return m_HasDiffuseTextures; }
        bool HasSpecularTextures() const { return m_HasSpecularTextures; }

        // 获取合并的包围盒（所有子 Mesh 的 AABB）
        const AABB& GetBoundingBox() const { return m_BoundingBox; }

        // 获取子 Mesh 列表
        const std::vector<Mesh>& GetMeshes() const { return meshes; }

    private:
        void LoadModel(const std::filesystem::path &path);
        void ProcessNode(const aiNode *node, const aiScene *scene);
        Mesh ProcessMesh(const aiMesh *mesh, const aiScene *scene);
        std::vector<MeshTexture> LoadMaterialTextures(aiMaterial *mat, aiTextureType type, const std::string &typeName, const aiScene *scene);
        unsigned int TextureFromFile(const char *path, const std::string &directory);
        unsigned int TextureFromAssimp(const aiTexture *texture, const std::string &directory);

        unsigned int LoadDDSTextureFromMemory(const char *path, const std::string &directory);
        unsigned int CreateDefaultTexture();

        void CalculateModelBoundingBox();  // 合并所有 Mesh 的 AABB

    private:
        std::vector<Mesh> meshes;
        std::string directory;
        std::unordered_map<std::string, MeshTexture> loaded_textures;
        bool gamma_correction = false;
        bool m_HasDiffuseTextures = false;
        bool m_HasSpecularTextures = false;
        AABB m_BoundingBox;  // 模型的合并包围盒
    };
}
