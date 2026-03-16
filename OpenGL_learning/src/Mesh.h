#pragma once

#include "utils/GLCommon.h"
#include "graphics/Shader.h"
#include <vector>
#include <string>
#include <filesystem>
#include <glm/glm.hpp>
#include <unordered_map>

#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>

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

    class Mesh {
    public:
        std::vector<MeshVertex> vertices;
        std::vector<unsigned int> indices;
        std::vector<MeshTexture> textures;
        Mesh(std::vector<MeshVertex> vertices, std::vector<unsigned int> indices, std::vector<MeshTexture> textures);
        void Draw(const Shader &shader);

    private:
        unsigned int VAO, VBO, EBO;
        void SetupMesh();
    };

    class Model {
    public:
        Model() = default;
        Model(const std::filesystem::path &path, bool gamma = false) : gamma_correction(gamma) { LoadModel(path); }
        void Draw(const Shader &shader);

    private:
        void LoadModel(const std::filesystem::path &path);
        void ProcessNode(const aiNode *node, const aiScene *scene);
        Mesh ProcessMesh(const aiMesh *mesh, const aiScene *scene);
        std::vector<MeshTexture> LoadMaterialTextures(aiMaterial *mat, aiTextureType type, const std::string &typeName, const aiScene *scene);
        unsigned int TextureFromFile(const char *path, const std::string &directory);
        unsigned int TextureFromAssimp(const aiTexture *texture, const std::string &directory);

        unsigned int LoadDDSTextureFromMemory(const char *path, const std::string &directory);
        unsigned int CreateDefaultTexture();

    private:
        std::vector<Mesh> meshes;
        std::string directory;
        // 类似记录一个library
        std::unordered_map<std::string, MeshTexture> loaded_textures;
        bool gamma_correction = false;
    };
}
